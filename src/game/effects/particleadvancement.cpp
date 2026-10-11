#include "game/effects/particleadvancement.h"
#include "game/effects/particlechildren.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    float floating(std::uint32_t bits) {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
}

unsigned particles::advance(Emitter& emitter, const Forces& forces, std::uint32_t& randomState) {
    const auto& parameters = emitter.parameters;
    auto value = [&](unsigned index) { return floating(parameters.fields10C[index]); };
    randomState = randomState * 0x5eedf715u + 0x1b0cb173u;
    const float damping = value(1) - (value(2) * 0x1p-32f) * static_cast<float>(randomState);
    const float sizeIn = value(4) == 0 ? 1 : (1.0f - value(6)) / value(4);
    const float sizeOut = value(5) == 1 ? 1 : (1.0f - value(7)) / (1.0f - value(5));
    const float alphaIn = value(10) == 0 ? 1 : 1.0f / value(10);
    const float alphaOut = value(11) == 1 ? 1 : 1.0f / (1.0f - value(11));
    if (emitter.fieldC4) emitter.fieldC4->advance(emitter);
    auto* node = emitter.nodes.sentinel.next;
    while (node != &emitter.nodes.sentinel) {
        if (unsigned(node->fields30[1]) + 1 < node->fields30[0]) {
            auto& motion = node->fields0;
            if (!node->fields54[4]) {
                motion[6] = motion[7] = motion[8] = 0;
                if (parameters.field1E4 <= node->fields30[1]) forces.apply(parameters, *node, randomState);
            }
            for (unsigned i = 0; i < 3; ++i) motion[i + 3] *= damping;
            for (unsigned i = 0; i < 3; ++i) motion[i + 3] += motion[i + 6];
            if (parameters.flags & 0x10) {
                const float square = std::fma(motion[5], motion[5], motion[3] * motion[3]) + motion[4] * motion[4];
                if (value(3) * value(3) < square) {
                    const float estimate = 1.0f / std::sqrt(square);
                    const float inverse = (3.0f - (estimate * estimate) * square) * (estimate * 0.5f);
                    for (unsigned i = 3; i < 6; ++i) motion[i] = (motion[i] * inverse) * value(3);
                }
            }
            for (unsigned i = 0; i < 3; ++i) motion[i] += motion[i + 3];
            const float age = static_cast<float>(node->fields30[1]) / static_cast<float>(node->fields30[0]);
            if (value(4) <= age) node->fields38[0] = value(5) <= age ? value(7) + (1.0f - age) * sizeOut : 1;
            else node->fields38[0] = value(6) + age * sizeIn;
            if (value(10) <= age) node->fields38[2] = value(11) <= age ? (1.0f - age) * alphaOut : 1;
            else node->fields38[2] = age * alphaIn;
            randomState = randomState * 0x5eedf715u + 0x1b0cb173u;
            node->fields38[2] *= 1.0f - value(12) * (0x1p-32f * static_cast<float>(randomState));
            node->fields44[0] = static_cast<std::uint16_t>(node->fields44[0] + node->fields44[1]);
            ++node->fields30[1];
            if (emitter.fieldC8) emitter.fieldC8->advance(emitter, *node);
            node = node->next;
        } else {
            auto* next = node->next;
            if (!emitter.nodePool) throw std::runtime_error("Original particle retirement pool is unavailable.");
            emitter.nodes.remove(*node);
            emitter.nodePool->append(*node);
            node->reset();
            node = next;
        }
    }
    if (parameters.flags & 0x2000) updateChildren(emitter, randomState);
    ++emitter.fieldBC;
    if (static_cast<std::int16_t>(parameters.field204) <= static_cast<int>(emitter.fieldBC)) {
        emitter.fieldBC = 0;
        ++emitter.fieldC0;
        if (parameters.fields206[0] && emitter.fieldC0 == parameters.fields206[0]) emitter.release();
    }
    return emitter.nodes.count + emitter.shortNodes.count;
}
