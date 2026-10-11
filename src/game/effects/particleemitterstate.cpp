#include "game/effects/particleemitterstate.h"

#include <stdexcept>
#include <utility>

namespace particles {
    namespace {
        std::uint32_t read(const Resource& source, unsigned offset, unsigned bytes) {
            if (!source || offset > source->size() || bytes > source->size() - offset) {
                throw std::out_of_range("Original particle draw parameters are truncated.");
            }
            std::uint32_t value = 0;
            for (unsigned i = 0; i < bytes; ++i) value = (value << 8) | (*source)[offset + i];
            return value;
        }
    }

    void DrawState::load(Resource data, unsigned offset, Resource image, bool extended) {
        source = std::move(data);
        sourceOffset = offset;
        texture = std::move(image);
        if (extended) {
            fields2C[0] = read(source, offset, 4);
            fields2C[1] = read(source, offset + 4, 4);
            offset += 8;
        }
        for (unsigned i = 0; i < 3; ++i) fields18[i] = static_cast<std::uint8_t>(read(source, offset + i, 1));
        count = static_cast<std::uint8_t>(read(source, offset + 3, 1));
        if (extended) {
            fields34[0] = (fields18[2] >> 2) & 1;
            fields34[1] = (fields18[2] >> 1) & 1;
            fields18[2] &= 1;
        }
        arrays[0] = offset + 4;
        arrays[1] = offset + 4 + count * 4;
        if (fields18[0] & 2) arrays[2] = offset + 4 + count * 8;
    }

    Emitter::Emitter() : next(this), previous(this) {}

    void Emitter::reset() {
        flags = 0;
        position.fill(0);
        field220.fill(0);
        field22C.fill(0);
        parameters.flags = 0;
        fieldB8 = 0;
        fieldBC = 0;
        fieldC0 = 0;
    }

    void Emitter::release() { flags = (flags | 0x14) & 0xfffd; }
    void Emitter::stop() { flags = (flags | 0x24) & 0xfffd; }
    bool Emitter::inactive() const { return flags == 0; }
    int Emitter::attachmentState() { return inactive(); }
    void Emitter::setAttachmentPosition(const std::array<float, 3>& value, const std::array<float, 3>& vector) {
        position = value;
        field220 = vector;
    }
    void Emitter::setMatrix(const std::array<float, 12>&) {}
    void Emitter::setVisible(bool visible) {
        if (visible) flags |= 8;
        else flags &= 0xfff7;
    }
    void Emitter::setRunning(bool running) {
        if (running) flags |= 6;
        else flags &= 0xfff9;
    }
    bool Emitter::empty() const { return nodes.count + shortNodes.count == 0; }

    void Emitter::clearParticles() {
        while (nodes.sentinel.next != &nodes.sentinel) {
            auto* node = nodes.sentinel.next;
            if (!nodePool) throw std::runtime_error("Original emitter has no particle pool.");
            nodes.remove(*node);
            nodePool->append(*node);
            node->reset();
        }
        while (shortNodes.sentinel.next != &shortNodes.sentinel) {
            auto* node = shortNodes.sentinel.next;
            if (!shortNodePool) throw std::runtime_error("Original emitter has no child particle pool.");
            shortNodes.remove(*node);
            shortNodePool->append(*node);
            node->fields0.fill(0);
            node->fields18.fill(0);
            node->fields20.fill(0);
        }
    }

    void Emitter::reload(particles::Resource source, particles::Resource texture, particles::Resource alternateTexture) {
        parameters.flags = 0;
        fieldB8 = 0;
        fieldBC = 0;
        fieldC0 = 0;
        parameters.load(source, read(source, 4, 2));
        if (read(source, 6, 1)) {
            draw = &ordinaryDraw;
            draw->nodes = &nodes;
            draw->shortNodes = &shortNodes;
            draw->load(source, 8, texture, false);
            draw->alternateTexture = alternateTexture ? std::move(alternateTexture) : std::move(texture);
        }
    }

    EmitterPool::EmitterPool(unsigned emitterCount, unsigned nodeCount, unsigned shortNodeCount)
        : emitters(emitterCount), nodes(nodeCount), shortNodes(shortNodeCount), destination(&active[0]) {}

    Emitter* EmitterPool::create(Resource source, Resource texture, Resource alternateTexture) {
        if (!source || !texture) return nullptr;
        Emitter* emitter = nullptr;
        if (emitters.free.count) {
            emitter = emitters.free.take();
            if (!emitter) throw std::runtime_error("Original particle emitter free list is inconsistent.");
            destination->append(*emitter);
            emitter->reset();
            emitter->nodePool = &nodes.free;
            emitter->shortNodePool = &shortNodes.free;
            emitter->parameters.load(source, read(source, 4, 2));
            emitter->flags = 0xe;
            const bool extended = read(source, 6, 1) == 0;
            emitter->draw = extended ? &emitter->extendedDraw : &emitter->ordinaryDraw;
            emitter->draw->nodes = &emitter->nodes;
            emitter->draw->shortNodes = &emitter->shortNodes;
            emitter->draw->load(source, 8, texture, extended);
            emitter->draw->alternateTexture = alternateTexture ? std::move(alternateTexture) : std::move(texture);
        }
        ++attempts;
        return emitter;
    }

    void EmitterPool::enterAlternate() {
        destination = &active[1];
        flags |= 1;
    }

    void EmitterPool::leaveAlternate() {
        auto& list = active[1];
        while (list.sentinel.next != &list.sentinel) {
            auto* emitter = list.sentinel.next;
            emitter->clearParticles();
            list.remove(*emitter);
            active[2].append(*emitter);
            emitter->reset();
        }
        destination = &active[0];
        flags &= 0xfe;
    }

    void EmitterPool::update(Simulation& simulation) {
        while (active[2].count) {
            auto* emitter = active[2].take();
            if (!emitter) throw std::runtime_error("Original particle retirement list is inconsistent.");
            emitters.free.append(*emitter);
        }
        auto* emitter = destination->sentinel.next;
        while (emitter != &destination->sentinel) {
            const auto status = emitter->flags;
            bool retire = false;
            if (!(status & 0x100)) {
                if ((status & 1) && emitter->fieldC4) simulation.callback(*emitter);
                if (status & 2) simulation.emit(*emitter);
                if (status & 4) simulation.advance(*emitter);
                if (status & 0x10) {
                    emitter->flags &= 0xfffd;
                    if (emitter->empty()) {
                        emitter->flags = (status & 0xff) | 0x20;
                        retire = true;
                    }
                }
                if ((status & 0x20) || retire) {
                    auto* next = emitter->next;
                    emitter->clearParticles();
                    destination->remove(*emitter);
                    active[2].append(*emitter);
                    emitter->reset();
                    emitter = next;
                    continue;
                }
            }
            emitter = emitter->next;
        }
        for (unsigned i = 28; i > 0; --i) attemptHistory[i] = attemptHistory[i - 1];
        attemptHistory[0] = attempts;
        attempts = 0;
    }
}
