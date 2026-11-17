#include "resources/dolresources.h"
#include "game/effects/modeleffectpool.h"
#include "game/startup/startupresources.h"

#include <stdexcept>
#include <algorithm>
#include <type_traits>
#include "jlib/j3d/j3dkeyanimation.h"
#include "jlib/j3d/j3dmaterialcalculation.h"
#include "jlib/j3d/j3dmaterialdisplaylist.h"
#include "jlib/j3d/j3dtextureanimation.h"

namespace {
    std::vector<std::uint8_t> range(const std::vector<std::uint8_t>& executable, unsigned address, unsigned size) {
        std::vector<std::uint8_t> result;
        std::string error;
        if (!dolresources::read(executable, address, size, result, error)) throw std::runtime_error(error);
        return result;
    }

    unsigned word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
            (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
    }

    std::string name(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::string result;
        for (unsigned i = 0; i < 256; ++i) {
            const auto value = range(executable, address + i, 1).front();
            if (!value) return result;
            result += static_cast<char>(value);
        }
        throw std::runtime_error("Original model-effect resource name is unterminated.");
    }

    void increment(std::int32_t& value) {
        value = static_cast<std::int32_t>(static_cast<std::uint32_t>(value) + 1);
    }
}

std::vector<modeleffects::Definition> modeleffects::readDefinitions(const std::vector<std::uint8_t>& executable, unsigned map) {
    if (map > (0xffffffffu - 0x80384d6cu) / 4) throw std::out_of_range("Original model-effect map index is invalid.");
    const auto count = range(executable, 0x80384b34 + map, 1).front();
    const auto address = word(range(executable, 0x80384d6c + map * 4, 4), 0);
    const auto table = range(executable, address, count * 12);
    std::vector<Definition> result;
    for (unsigned i = 0; i < count; ++i) {
        Definition definition;
        const auto names = word(table, i * 12);
        const auto parameters = range(executable, word(table, i * 12 + 4), 32);
        definition.capacity = word(table, i * 12 + 8);
        definition.lifetime = static_cast<std::int32_t>(word(parameters, 0));
        definition.transformAnimation = parameters[4] != 0;
        unsigned nameCount = definition.transformAnimation ? 2 : 1;
        for (unsigned j = 0; j < 3; ++j) {
            definition.counts[j] = word(parameters, 8 + j * 4);
            if (definition.counts[j] > executable.size() / 4) throw std::runtime_error("Invalid model-effect animation count.");
            nameCount += definition.counts[j];
        }
        definition.resourceType = word(parameters, 20);
        definition.type = word(parameters, 24);
        definition.lighting = word(parameters, 28) != 0;
        const auto pointers = range(executable, names, nameCount * 4);
        for (unsigned j = 0; j < nameCount; ++j) definition.names.push_back(name(executable, word(pointers, j * 4)));
        result.push_back(std::move(definition));
    }
    return result;
}

modeleffects::Pool::Pool(Definition definition, std::nullptr_t) : definition(std::move(definition)) {
    for (unsigned i = 0; i < animations.size(); ++i) animations[i].resize(this->definition.counts[i]);
}

modeleffects::Pool::Pool(Definition definition) : Pool(std::move(definition), nullptr) {
    for (unsigned i = 0; i < this->definition.capacity; ++i) owners.push_back(std::make_unique<Owner>(this->definition.counts));
}

void modeleffects::Pool::load(particles::ArchiveResources& resources, const j3dmodel::MaterialDefaults& defaults) {
    unsigned cursor = 0;
    auto resource = [&]() {
        const auto& name = definition.names.at(cursor++);
        auto result = resources.get(definition.resourceType, name);
        if (!result) throw std::runtime_error("Missing original model-effect resource: " + name);
        return result;
    };
    std::string error;
    auto model = std::make_shared<j3dmodel::ModelData>();
    if (!model->read(*resource(), 0x54020000, defaults, error)) throw std::runtime_error(error);
    auto animation = [&]() {
        auto result = std::make_shared<j3dmodel::Animation>();
        if (!result->read(*resource(), error)) throw std::runtime_error(error);
        return result;
    };
    transform = definition.transformAnimation ? animation() : nullptr;
    for (auto& group : animations) for (auto& entry : group) entry = animation();
    if (transform) {
        if (model->jointCalculators.empty()) throw std::runtime_error("Original model effect has no root joint.");
        const auto kind = model->geometry.flags & 15;
        model->jointCalculators[0] = kind < 3 ? std::make_unique<j3dmodel::MatrixCalculator>(kind) : nullptr;
    }
    if (definition.counts[0] + definition.counts[1] + definition.counts[2]) {
        j3dmodel::prepareMaterialAnimations(*model);
    }
    if (transform) j3dmodel::bindAnimation(*model, transform);
    for (auto& group : animations) for (auto& entry : group) j3dmodel::bindAnimation(*model, entry);
    for (auto& owner : owners) owner->model = std::make_unique<j3dmodel::ModelInstance>(model, 0x10000);
}

modeleffects::Owner* modeleffects::Pool::available() {
    for (auto& owner : owners) if (!(owner->flags & 9)) return owner.get();
    return nullptr;
}

modeleffects::Owner* modeleffects::Pool::available(const j3dmodel::Matrix&) { return available(); }

void modeleffects::Pool::advance(Owner& owner, bool visible) {
    if (!(owner.flags & 1) || !visible) {
        owner.flags &= 0xf7;
        return;
    }
    if ((owner.flags & 0x10) || !(owner.flags & 2)) return;
    if (transform) {
        increment(owner.frame);
        if (owner.frame >= transform->frameCount) owner.frame = 0;
    }
    for (unsigned i = 0; i < animations.size(); ++i) {
        for (unsigned j = 0; j < animations[i].size(); ++j) {
            auto& frame = owner.frames[i][j];
            increment(frame);
            if (frame >= animations[i][j]->frameCount) frame = 0;
        }
    }
    if (definition.lifetime) {
        increment(owner.elapsed);
        if (owner.elapsed == definition.lifetime) owner.release();
    }
}

void modeleffects::Pool::update(const roomlocation::Rooms& rooms, const roomtraversal::State& visibility) {
    for (auto& owner : owners) {
        bool visible = false;
        if (owner->flags & 1) {
            if (!owner->model) throw std::runtime_error("Original model-effect update has no model instance.");
            const auto& matrix = owner->model->matrix;
            const auto room = rooms.resolve({matrix[3], matrix[7], matrix[11]}, 0xffffffff);
            const auto flags = room == 0xffffffff ? 0 : visibility.record(room, 0).flags;
            visible = (flags & 1) && !(flags & 2);
        }
        advance(*owner, visible);
    }
}

void modeleffects::Pool::setPaused(bool paused) {
    for (auto& owner : owners) if (owner->flags & 1) owner->setPaused(paused);
}

void modeleffects::Factory::clear() {
    pools.fill(nullptr);
    count = 0;
}

void modeleffects::Factory::add(Pool& pool) {
    if (count < pools.size()) pools[count++] = &pool;
}

modeleffects::Owner* modeleffects::Factory::create(unsigned type, const j3dmodel::Matrix& matrix) {
    for (unsigned i = 0; i < count; ++i) {
        if (pools[i]->definition.type != type) continue;
        if (auto* owner = pools[i]->available(matrix)) {
            owner->setScale({1, 1, 1});
            owner->model->matrix = matrix;
            owner->flags |= 1;
            return owner;
        }
    }
    return nullptr;
}

modeleffects::Owner* modeleffects::Factory::create(unsigned type, const std::array<float, 3>& position) {
    return create(type, j3dmodel::Matrix{1, 0, 0, position[0], 0, 1, 0, position[1], 0, 0, 1, position[2]});
}

std::vector<j3dmodel::ModelInstance*> modeleffects::Pool::prepare(const roomlocation::Rooms& rooms,
    const roomtraversal::State& visibility, const j3dmodel::Matrix& view,
    const j3dmodel::TransformTables& tables, unsigned lightMask,
    const std::array<std::unique_ptr<j3dmodel::DrawBuffer>, 2>& buffers) {
    std::vector<j3dmodel::ModelInstance*> result;
    const auto submit = [&](j3dmodel::ModelInstance& instance) {
        j3dmodel::calculateAnimated(instance, tables);
        for (auto& entry : instance.model->materials.entries) {
            auto& material = *entry;
            if (definition.lighting && !(material.identityFlags & 0xc0000000) && material.original) {
                material.identityFlags |= 0x80000000;
                material.mode = material.original->mode;
                material.fieldE = material.original->fieldE;
                material.animation.reset();
            }
            auto& channel = material.color->channels[0];
            if (definition.lighting) {
                channel = static_cast<std::uint16_t>((channel & 0xffc3) | ((lightMask << 2) & 0x3c));
                channel = static_cast<std::uint16_t>((channel & 0x87ff) | ((lightMask << 7) & 0x1800));
            }
            if (material.animation) {
                const auto animate = [&](const auto& bindings, auto& colors, unsigned section, int minimum, int maximum) {
                    for (unsigned slot = 0; slot < bindings.size(); ++slot) {
                        if (!bindings[slot]) continue;
                        const auto& binding = *bindings[slot];
                        const auto& animation = *binding.animation;
                        const auto& group = animation.groups[section];
                        for (unsigned component = 0; component < 4; ++component) {
                            const auto value = j3dmodel::sampleAnimationKey(animation, group,
                                group.table + binding.index * 28 + component * 6, component, true, 0);
                            using Color = typename std::decay_t<decltype(colors[slot])>::value_type;
                            colors[slot][component] = static_cast<Color>(std::clamp<int>(int(value), minimum, maximum));
                        }
                    }
                };
                animate(material.animation->tevColors, material.tev->colors, 0, -1024, 1023);
                animate(material.animation->tevConstants, material.tev->constants, 1, 0, 255);
            }
        }
        const auto& geometry = instance.model->geometry;
        std::vector<int> pending;
        if (geometry.hierarchy.root >= 0) pending.push_back(geometry.hierarchy.root);
        while (!pending.empty()) {
            const int joint = pending.back();
            pending.pop_back();
            const auto& links = geometry.hierarchy.joints[joint];
            if (links.sibling >= 0) pending.push_back(links.sibling);
            if (links.child >= 0) pending.push_back(links.child);
            for (int index = links.material; index >= 0; index = geometry.hierarchy.materials[index].next) {
                auto& packet = instance.materials.at(index);
                auto& material = *packet.material;
                j3dmodel::animateTextureMatrices(material);
                j3dmodel::calculateMaterial(material, instance.jointMatrices[geometry.joints[joint].index], view,
                    instance.field8 & 12, tables);
                auto& buffer = *buffers[(material.mode & 3) ? 0 : 1];
                if (buffer.sortType != 0) throw std::runtime_error("Original model effect requires material-sort drawing.");
                packet.materialIdentity = material.identity;
                packet.identityFlags = material.identityFlags;
                if (buffer.entryMaterial(packet, geometry.textureOffset)) j3dmodel::updateMaterialPacket(packet);
            }
        }
        j3dmodel::calculateViewMatrices(instance, view);
        result.push_back(&instance);
    };
    for (auto& entry : owners) {
        auto& owner = *entry;
        if ((owner.flags & 5) != 5) continue;
        const auto& matrix = owner.model->matrix;
        const auto room = rooms.resolve({matrix[3], matrix[7], matrix[11]}, 0xffffffff);
        if (room == 0xffffffff || (visibility.record(room, 0).flags & 3) != 1) continue;
        if (transform) transform->frame = float(owner.frame);
        for (unsigned group = 0; group < animations.size(); ++group)
            for (unsigned index = 0; index < animations[group].size(); ++index)
                animations[group][index]->frame = float(owner.frames[group][index]);
        submit(*owner.model);
    }
    return result;
}
