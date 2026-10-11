#include "game/effects/particlechildren.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
    float floating(std::uint32_t value) {
        float result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    std::uint8_t byte(float value) {
        const auto integer = !std::isfinite(value) || value < -2147483648.0 || value >= 2147483648.0 ?
            std::numeric_limits<std::int32_t>::min() : static_cast<std::int32_t>(value);
        return static_cast<std::uint8_t>(integer);
    }
}

void particles::updateChildren(Emitter& emitter, std::uint32_t& randomState) {
    const auto& parameters = emitter.parameters;
    for (auto* parent = emitter.nodes.sentinel.next; parent != &emitter.nodes.sentinel; parent = parent->next) {
        if (!emitter.shortNodePool) throw std::runtime_error("Original child particle pool is unavailable.");
        if (!emitter.shortNodePool->count) break;
        const unsigned age = unsigned(parent->fields30[1]) + 1;
        const auto interval = parameters.fields160[1];
        if (!interval) throw std::runtime_error("Original child particle interval divides by zero.");
        if (age % interval) continue;
        auto* child = emitter.shortNodePool->take();
        if (!child) throw std::runtime_error("Original child particle free list is inconsistent.");
        emitter.shortNodes.append(*child);
        for (unsigned i = 0; i < 3; ++i) child->fields0[i + 3] = floating(parent->fields24[i]);
        for (unsigned i = 0; i < 3; ++i) {
            randomState = randomState * 0x5eedf715u + 0x1b0cb173u;
            const float spread = floating(parameters.fields150[2]);
            const float offset = (0x1p-32f * static_cast<float>(randomState)) * (spread + spread) - spread;
            child->fields0[i] = parent->fields0[i] + offset;
        }
        const float size = (floating(parameters.fields150[0]) * parent->fields38[1]) * parent->fields38[0];
        std::uint32_t bits;
        std::memcpy(&bits, &size, sizeof(bits));
        for (unsigned i = 0; i < 4; ++i) child->fields1A[i + 2] = static_cast<std::uint8_t>(bits >> (24 - i * 8));
        if (parameters.flags & 0x4000) {
            for (unsigned i = 0; i < 4; ++i) child->fields20[i] = parent->fields54[i];
            child->fields20[3] = byte(floating(parameters.fields150[1]) * static_cast<float>(child->fields20[3]));
        } else {
            for (unsigned i = 0; i < 4; ++i) {
                child->fields20[i] = static_cast<std::uint8_t>(parameters.fields150[3] >> (24 - i * 8));
            }
            child->fields20[3] = byte(255.0f * floating(parameters.fields150[1]));
        }
        child->fields18[0] = parameters.fields160[0];
    }
    auto* child = emitter.shortNodes.sentinel.next;
    while (child != &emitter.shortNodes.sentinel) {
        auto* next = child->next;
        const unsigned age = unsigned(child->fields18[1]) + 1;
        if (age < child->fields18[0]) child->fields18[1] = static_cast<std::uint8_t>(age);
        else {
            if (!emitter.shortNodePool) throw std::runtime_error("Original child particle retirement pool is unavailable.");
            emitter.shortNodes.remove(*child);
            emitter.shortNodePool->append(*child);
            child->fields0.fill(0);
            child->fields20.fill(0);
            child->fields18.fill(0);
        }
        child = next;
    }
}
