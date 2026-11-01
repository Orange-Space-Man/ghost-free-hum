#include "game/player/playerfinishframe.h"
#include "game/actors/actortimingsampler.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
    std::uint32_t word(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }

    float number(const std::vector<std::uint8_t>& executable, unsigned address) {
        const auto bits = word(executable, address);
        float result;
        std::memcpy(&result, &bits, 4);
        return result;
    }

    std::int32_t integer(float value) {
        if (!std::isfinite(value) || double(value) < -2147483648.0 || double(value) >= 2147483648.0)
            return std::numeric_limits<std::int32_t>::min();
        return static_cast<std::int32_t>(value);
    }
}

playeractor::FinishFrame::FinishFrame(const std::vector<std::uint8_t>& executable, const actormodel::InitialPose& pose) : pose(pose) {
    concealDuration = static_cast<std::int32_t>(word(executable, glme01::playerCaptureConcealFrames));
    powerDuration = static_cast<std::int32_t>(word(executable, glme01::playerPowerScaleDuration));
    recoveryDuration = static_cast<std::int32_t>(word(executable, glme01::playerPowerRecoveryDuration));
    for (unsigned i = 0; i < angleParameters.size(); ++i)
        angleParameters[i] = number(executable, glme01::playerScaleAngleParameters + i * 4);
    cosineOffset = number(executable, glme01::playerScaleCosineOffset);
    amplitude = number(executable, glme01::playerScaleAmplitude);
}

float playeractor::FinishFrame::bodyScale(const State& state) const {
    float scale = state.fieldF60;
    if (state.field105C > 0)
        scale = static_cast<float>(double(scale) * double(1.0f - state.field105C * (1.0f / float(concealDuration))));
    const auto wave = [&](std::int32_t duration) {
        const float angle = angleParameters[0] * angleParameters[1] * angleParameters[2] * state.field1174 *
            (1.0f / float(duration)) / angleParameters[3];
        const auto index = static_cast<std::uint16_t>(integer(angle));
        return (pose.horizontalDirection(index)[2] - cosineOffset) * amplitude;
    };
    if (state.field1170 == 0) {
        if (state.field1174 > 0)
            scale = static_cast<float>(double(scale) * double(state.field1178 + wave(recoveryDuration) * (1.0f - state.field1178)));
    } else if (state.field1174 <= 0) scale = static_cast<float>(double(scale) * double(state.field1178));
    else scale = static_cast<float>(double(scale) * double(1.0f + wave(powerDuration) * (state.field1178 - 1.0f)));
    return scale;
}

void playeractor::FinishFrame::run(Player& player, const FinishServices& services) const {
    auto& state = player.state;
    const auto& actions = *player.components->actions;
    const auto room = player.fieldB4 >> 24;
    state.flags1330[0] = room != state.field132C;
    state.field132C = room;
    player.animationSpeed = state.fieldF68;
    if (static_cast<std::int32_t>(state.field112C) > 0) {
        const auto& type = services.catalog.type(player.actorType);
        const auto timing = services.models.timing(type.model, type.animations.at(player.animationState)[0]);
        const float previous = static_cast<float>(double(player.animationFrame) - double(player.animationFrameStep));
        if (actoranimation::timingCrossing(timing.get(), "Ldam_timing", previous, player.animationFrame) > 0) {
            if (!services.damage) throw std::runtime_error("Player frame completion requires deferred damage dispatch.");
            playerevents::Payload payload;
            payload.word(0x18, state.field112C);
            payload.word(0x20, actions.field310 == 2 && actions.field314 == 0x20 ? 0 : 1);
            services.damage(player, payload);
            state.field112C = 0;
        }
    }
    const float scale = bodyScale(state);
    player.scale = {scale, static_cast<float>(double(scale) * double(state.fieldF64)), scale};
    const bool active = state.flags10E8[0] || (actions.field310 == 2 && actions.field314 == 0x22);
    if (active != bool(state.flags1388[2])) {
        if (!services.interactionActive) throw std::runtime_error("Player frame completion requires interaction ownership.");
        state.flags1388[2] = active;
        services.interactionActive(active);
        if (active) {
            if (!services.interactionEvent) throw std::runtime_error("Player frame completion requires event 0x3a.");
            services.interactionEvent(player);
            if (state.field1064) {
                if (!services.interactionOwner) throw std::runtime_error("Player frame completion requires its interaction owner.");
                services.interactionOwner(*state.field1064);
            }
        }
    }
    state.flags1388[1] = 0;
    player.refreshView();
}
