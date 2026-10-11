#include "game/player/playerattachedstate.h"
#include "game/actors/moveactor.h"
#include "game/player/playerorientationupdate.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    float number(std::uint32_t bits) {
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }

    unsigned axis(unsigned value) {
        if (value == 'Y' || value == 'y') return 1;
        if (value == 'Z' || value == 'z') return 2;
        return 0;
    }
}

playercomponents::AttachedOutput playercomponents::AttachedState::update(const AttachedInput& value,
    const actormodel::Catalog& catalog, const actormodel::Resources& resources, const actormodel::InitialPose& pose) {
    if (!actor) throw std::runtime_error("Attached animation update requires its original actor owner.");
    input = value;
    field36C = input.enabled;
    field248 = input.enabled ? input.selection : 0;
    AttachedOutput output;
    const unsigned row = 5 + field248 * 4;
    const auto integer = [&](unsigned index) { return static_cast<float>(static_cast<std::int32_t>(parameters.at(index))); };
    const auto real = [&](unsigned index) { return number(parameters.at(index)); };
    const float first = integer(row + 1), last = integer(row + 2), neutral = integer(row + 3);
    const float neutralFirst = (neutral - first) / (last - first);
    const float neutralSecond = (real(43) - real(41)) / (real(42) - real(41));
    const auto set = [&](float first, float second) { output.limits = setBlend(first, second); };
    if (!input.enabled && !input.aim) {
        if (input.mode == 1) setBlend(neutralFirst, neutralSecond);
    } else if (!fields2B8[4]) {
        const float x = input.horizontal, y = input.vertical;
        const float xScale = input.horizontalScale, yScale = input.verticalScale;
        switch (input.mode) {
        case 1:
            setBlend(neutralFirst, neutralSecond);
            break;
        case 2:
        case 5:
            set(field240 + xScale * real(44) * y, field244 + yScale * real(45) * x);
            if (input.mode == 5) field240 = std::clamp(field240, neutralFirst, neutralFirst + 0.4f * (1 - neutralFirst));
            break;
        case 3:
            set(field240 + xScale * real(44) * y, field244 + (neutralSecond - field244) * std::min(real(49), 1.0f));
            output.limits[1] = x > 0 ? 1 : x < 0 ? -1 : 0;
            break;
        case 4:
            set(field240 + (neutralFirst - field240) * std::min(real(48), 1.0f), field244 + yScale * real(45) * x);
            break;
        case 6:
            set(field240 + xScale * real(46) * ((y + 1) * 0.5f - field240),
                field244 + yScale * real(47) * ((x + 1) * 0.5f - field244));
            break;
        case 7: {
            const float target = y <= 0 ? neutralFirst + -y * (0 - neutralFirst) : neutralFirst + y * (1 - neutralFirst);
            set(field240 + xScale * real(48) * (target - field240), field244 + yScale * real(49) * ((x + 1) * 0.5f - field244));
            break;
        }
        case 8:
            fields2B8[4] = 1;
            break;
        case 9:
            set(field240 + (neutralFirst - field240) * std::min(real(48), 1.0f),
                field244 + (neutralSecond - field244) * std::min(real(49), 1.0f));
            break;
        case 10:
        case 11: {
            const auto source = pose.angles(playerorientation::toMatrix(playerorientation::heading(input.sourceDirection, false)));
            const auto target = pose.angles(playerorientation::toMatrix(playerorientation::heading(input.targetDirection, false)));
            if (source[2] == 0 && target[2] == 0) {
                std::array<float, 2> difference;
                for (unsigned i = 0; i < 2; ++i) {
                    difference[i] = (float(static_cast<std::uint16_t>(target[i])) - float(static_cast<std::uint16_t>(source[i]))) / 32768.0f;
                    if (input.inverted) difference[i] = -difference[i];
                    if (difference[i] >= 1) difference[i] -= 2;
                    if (difference[i] <= -1) difference[i] += 2;
                }
                if (input.mode == 10) set(field240 - 0.5f * difference[0], field244 + 0.5f * difference[1]);
                else {
                    set(field240 - 0.5f * difference[0], field244 + (neutralSecond - field244) * std::min(real(49), 1.0f));
                    output.limits[1] = difference[1] > 0 ? 1 : difference[1] < 0 ? -1 : 0;
                }
            }
            break;
        }
        }
    } else {
        set(field240 + real(48) * (neutralFirst - field240), field244 + real(49) * (neutralSecond - field244));
        if (std::abs(neutral - (first + field240 * (last - first))) < 0.05f * (last - first) &&
            std::abs(real(43) - (real(41) + field244 * (real(42) - real(41)))) < 0.05f * (real(42) - real(41)))
            fields2B8[4] = 0;
    }

    const auto& type = catalog.type(actor->actorType);
    const auto& entry = resources.entry(type.model);
    const auto clearSound = [&] {
        if (field2DC == -1) return;
        field2DC = -1;
        sound.bind(nullptr, 1, 0);
        field368 = 1;
    };
    const auto bind = [&](unsigned index, unsigned key) {
        if (!entry.archive) throw std::runtime_error("Attached animation requires its actor model archive.");
        const auto resource = entry.archive->keys.at(key);
        auto& effect = effects[index];
        if (!fields2B8[index] || effect.field4 != resource) {
            effect.field4 = resource;
            effect.field74 = effect.field76 = 6;
            model.resetCursors();
        }
    };
    if (input.enabled) {
        if (!field2C0) {
            clearSound();
            bind(0, parameters.at(row) >> 16);
            effects[0].field8 = first + field240 * (last - first);
        }
        if (!field2D0) {
            if (field248 > 5 && field248 < 8) {
                bind(1, parameters.at(row) >> 16);
                effects[1].field8 = first + field240 * (last - first);
            } else {
                bind(1, type.animations.at(actor->animationState)[0]);
                effects[1].field8 = input.frame;
            }
        }
    }
    updateOverrides();
    if (!input.aim) {
        if (fields2B8[2]) {
            effects[2].flags &= ~4u;
            fields2B8[2] = 0;
        }
    } else {
        if (!fields2B8[2]) {
            effects[2].flags |= 4;
            fields2B8[2] = 1;
        }
        const float degrees = real(41) + field244 * (real(42) - real(41));
        auto direction = pose.horizontalDirection(static_cast<std::uint16_t>(static_cast<std::int32_t>((65536.0f * degrees) / 360.0f)));
        for (auto& component : direction) component *= 1000000;
        const auto basis = playerorientation::toMatrix(playerorientation::heading(direction, false));
        const unsigned sign = (parameters[4] >> 24) & 255, index = axis((parameters[4] >> 16) & 255);
        std::array<float, 3> forward{basis[index], basis[index + 4], basis[index + 8]};
        const float length = std::sqrt((forward[0] * forward[0] + forward[1] * forward[1]) + forward[2] * forward[2]);
        const float magnitude = std::sqrt((direction[0] * direction[0] + direction[1] * direction[1]) + direction[2] * direction[2]);
        for (auto& component : forward) component = length >= 0.0001f ? component * (1.0f / length) * magnitude * (sign == '-' ? -1 : 1) : 0;
        effects[2].field10 = forward;
    }
    output.morphs = {static_cast<std::int32_t>(fields270.at(field2C0)), static_cast<std::int32_t>(fields270.at(field2D0))};
    output.textures = {static_cast<std::int32_t>(fields294.at(field2C0)), static_cast<std::int32_t>(fields294.at(field2D0))};
    return output;
}
