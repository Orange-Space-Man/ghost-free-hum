#include "playerlatestate.h"
#include "game/player/playerorientationupdate.h"
#include "resources/dolresources.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    float constant(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        const auto bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    constexpr unsigned distanceNearAddress = 0x804d2bfc, distanceFarAddress = 0x804d2c00;
    constexpr unsigned scannerDistanceAddress = 0x804dabe0, scannerScaleAddress = 0x8020c2b8;
    constexpr unsigned warpScaleAddress = 0x804d20a0, warpDepthScaleAddress = 0x804d20a4;
    constexpr unsigned warpVerticalAddress = 0x804d20a8, warpDistanceAddress = 0x804d20ac;

    std::array<float, 3> normalize(std::array<float, 3> value) {
        const float length = std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
        if (length >= 0.0001f) {
            const float inverse = float(1.0 / double(length));
            for (auto& component : value) component *= inverse;
        } else value.fill(0);
        return value;
    }
}

playeractor::LateState::LateState(const std::vector<std::uint8_t>& executable) :
    distanceNear(constant(executable, distanceNearAddress)), distanceFar(constant(executable, distanceFarAddress)),
    scannerDistance(constant(executable, scannerDistanceAddress)), warpVertical(constant(executable, warpVerticalAddress)),
    warpDistance(constant(executable, warpDistanceAddress)),
    scannerScale{constant(executable, scannerScaleAddress), constant(executable, scannerScaleAddress + 4),
        constant(executable, scannerScaleAddress + 8)},
    warpScale{constant(executable, warpScaleAddress), constant(executable, warpScaleAddress), constant(executable, warpDepthScaleAddress)} {}

void playeractor::LateState::begin(Player& player, const LateStateServices& services) const {
    auto& state = player.state;
    const auto& camera = *state.camera;
    std::array<float, 3> direction, displacement;
    for (unsigned i = 0; i < 3; ++i) {
        direction[i] = camera.target[i] - camera.position[i];
        displacement[i] = camera.position[i] - player.position[i];
    }
    state.fieldF80 = normalize(direction);
    const float distance = std::sqrt((displacement[0] * displacement[0] + displacement[1] * displacement[1]) +
        displacement[2] * displacement[2]);
    state.fieldF8C = std::clamp(float(double(distance) - distanceNear) / float(double(distanceFar) - distanceNear), 0.0f, 1.0f);
    if (state.flags1140[2] && (state.field117C != 0 || state.field1058 || (!(camera.flags & 8) && (camera.flags & 1))))
        services.scannerTransition(player, false);
    const std::array<actors::MoveActor*, 4> actors{&player, state.field7A4, state.field7A8, state.field7AC};
    for (auto* actor : actors) if (actor) {
        if (services.hidePlayers) actor->flags |= 0x100;
        else actor->flags &= ~0x100u;
        actor->flags |= 2;
    }
    const auto hide = [](actors::MoveActor* actor, bool hidden) {
        if (!actor) return;
        if (hidden) actor->flags |= 0xc000;
        else actor->flags &= ~0xc000u;
    };
    if (player.flags & 0xc000) {
        hide(state.field7A4, true);
        hide(state.field7A8, true);
        hide(state.field7AC, true);
    } else if (!(state.flags792 & 8)) {
        hide(state.field7A4, !(state.flags792 & 4) || state.flags1330[2]);
        hide(state.field7A8, true);
        hide(state.field7AC, true);
    } else {
        hide(state.field7A4, false);
        hide(state.field7A8, false);
        hide(state.field7AC, state.field7B0 != 0);
    }
    const bool hideVacuum = !(state.flags792 & 8) || state.flags134C[0];
    if (player.actorType == 0x27) player.setMaterialFlags(0, 2, hideVacuum);
    else if (player.actorType < 2) player.setMaterialFlags(1, 2, hideVacuum);
}

void playeractor::LateState::scanner(playermodels::ScannerModels& owner, const playermodels::SharedModels& shared) const {
    if (!owner.state[0] && !owner.state[2]) return;
    std::array<float, 3> direction;
    for (unsigned i = 0; i < 3; ++i) direction[i] = owner.camera->position[i] - owner.camera->target[i];
    auto matrix = playerorientation::headingMatrix(normalize(direction), false);
    if (owner.state[0]) {
        auto& animation = *shared.animation(4);
        animation.frame = float(animation.frameCount) - owner.field10;
        owner.scanner->scale = scannerScale;
        for (unsigned i = 0; i < 3; ++i)
            matrix[i * 4 + 3] = owner.camera->position[i] + matrix[i * 4 + 2] * scannerDistance;
        owner.scanner->matrix = matrix;
    }
    if (owner.state[2]) {
        shared.animation(5)->frame = owner.field4;
        for (unsigned i : {6u, 7u}) {
            auto& animation = *shared.animation(i);
            const auto duration = static_cast<unsigned>(static_cast<std::int32_t>(animation.frameCount));
            if (!duration) throw std::runtime_error("Warp texture animation has zero duration.");
            animation.frame = float(static_cast<std::uint32_t>(owner.field4) % duration);
        }
        shared.animation(8)->frame = owner.field4;
        owner.warp->scale = warpScale;
        for (unsigned i = 0; i < 3; ++i)
            matrix[i * 4 + 3] = (owner.camera->position[i] + matrix[i * 4 + 1] * warpVertical) +
                matrix[i * 4 + 2] * -warpDistance;
        owner.warp->matrix = matrix;
    }
}
