#include "game/effects/particlescene.h"
#include "game/actors/actorregistry.h"
#include "game/world/dooranimationpose.h"

#include <cstring>
#include <stdexcept>

namespace {
    unsigned word(const std::vector<std::uint8_t>& data, unsigned offset) {
        return (unsigned(data.at(offset)) << 24) | (unsigned(data.at(offset + 1)) << 16) |
            (unsigned(data.at(offset + 2)) << 8) | data.at(offset + 3);
    }

    float number(const std::vector<std::uint8_t>& data, unsigned offset) {
        const auto bits = word(data, offset);
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }
}

std::vector<particles::Billboard> particles::Scene::prepareActorGlows(const actors::Registry& actors,
    const roomtraversal::State& visibility, const std::array<float, 12>& view, unsigned buffer) const {
    std::vector<Billboard> result;
    for (const auto* actor : actors.entries()) {
        if (!actor || !actor->model || (actor->flags & 0xa000) ||
            (!(actor->flags & 0x200) && (actor->room() >> 24 == 255 ||
                !visibility.marked(actor->room() >> 24, buffer, 0)))) continue;
        const auto resource = actor->glow();
        if (!resource) continue;
        const auto& bytes = resource->bytes;
        if (bytes.size() < 4) throw std::runtime_error("Truncated original actor glow header.");
        const auto count = word(bytes, 0);
        if (count > (bytes.size() - 4) / 100)
            throw std::runtime_error("Truncated original actor glow geometry.");
        const float scale = actor->field160 * actor->scale[0];
        for (unsigned index = 0; index < count; ++index) {
            const auto record = 4 + index * 100;
            const auto joint = word(bytes, record) & 65535;
            const auto matrix = dooranimation::concatenate(actor->matrix, actor->model->matrix(0, joint));
            std::array<float, 3> center{};
            for (unsigned axis = 0; axis < 3; ++axis) {
                center[axis] = matrix[axis * 4 + 3];
                for (unsigned component = 0; component < 3; ++component)
                    center[axis] += matrix[axis * 4 + component] * number(bytes, record + 88 + component * 4);
            }
            Billboard item;
            for (unsigned channel = 0; channel < 4; ++channel)
                item.color[channel] = static_cast<std::uint8_t>(actor->fieldE0 >> ((3 - channel) * 8));
            item.color[3] = static_cast<std::uint8_t>((unsigned(item.color[3]) * bytes.at(record + 7)) / 255);
            item.texture = resources.get(0, "hakko.bti");
            if (!item.texture) throw std::runtime_error("Original actor glow texture is absent.");
            item.textureScale = 2;
            item.textureWrap = {item.texture->at(6), item.texture->at(7)};
            item.mode = 4;
            item.blend = 0x14;
            item.depth = 0x09;
            item.cull = 0;
            for (unsigned corner = 0; corner < 4; ++corner)
                for (unsigned axis = 0; axis < 3; ++axis) {
                    item.positions[corner][axis] = center[axis];
                    for (unsigned component = 0; component < 3; ++component)
                        item.positions[corner][axis] += view[component * 4 + axis] *
                            (number(bytes, record + 8 + corner * 12 + component * 4) * scale);
                }
            result.push_back(std::move(item));
        }
    }
    return result;
}
