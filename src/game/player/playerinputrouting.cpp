#include "game/player/playerinputrouting.h"
#include "game/world/dooranimationpose.h"

#include <cmath>
#include <stdexcept>

namespace {
    using Vector = std::array<float, 3>;

    Vector normalize(Vector value) {
        const float length = std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
        if (length >= 0.0001f) {
            const float inverse = 1 / length;
            for (auto& component : value) component *= inverse;
        } else value.fill(0);
        return value;
    }

    actormodel::Matrix view(const gameplaycamera::CameraOwner& camera) {
        const auto backward = normalize({camera.position[0] - camera.target[0], camera.position[1] - camera.target[1],
            camera.position[2] - camera.target[2]});
        const auto right = normalize({backward[2], 0, -backward[0]});
        const Vector up{backward[1] * right[2] - backward[2] * right[1],
            backward[2] * right[0] - backward[0] * right[2], backward[0] * right[1] - backward[1] * right[0]};
        const std::array<Vector, 3> axes{right, up, backward};
        actormodel::Matrix result{};
        for (unsigned row = 0; row < 3; ++row) for (unsigned i = 0; i < 3; ++i) {
            result[row * 4 + i] = axes[row][i];
            result[row * 4 + 3] -= axes[row][i] * camera.position[i];
        }
        return result;
    }
}

void playerinput::route(playeractor::Player& player, const Decoder& decoder, const RouteServices& services) {
    auto& state = player.state;
    auto& controls = *player.components->controls;
    const auto& actions = *player.components->actions;
    Context context;
    context.view = view(services.camera);
    context.orientation = state.quaternion;
    if (state.field106C == 0 && static_cast<std::int16_t>(player.life) > 0 && !state.flags1040[2] &&
        !state.field1058 && !(state.field105C > 0)) context.pad = static_cast<const Pad*>(state.field794);
    context.edges = !state.flags1388[1];
    context.suppress = state.field1338 == 4;
    decoder.update(controls, context);
    bool scanner = state.flags1140[2] != 0;
    bool enabled = false;
    if ((state.flags792 & 8) && (actions.field310 != 2 || (actions.field314 != 0x20 && actions.field314 != 0x21)) &&
        static_cast<std::int16_t>(player.life) > 0 && state.field117C == 0 &&
        ((services.camera.flags & 8) || !(services.camera.flags & 1)) && !state.field11A0) {
        if (services.inputAvailable && !state.field1060 && state.field1170 == 0 && !state.flags1140[3] && services.mapMode != 3 &&
            ((scanner && controls.flags0[2]) || controls.flagsC[2])) scanner = !scanner;
        enabled = scanner || controls.flagsC[3];
    }
    if (enabled != bool(state.flags1140[2])) {
        if (!services.scannerTransition) throw std::runtime_error("Player scanner transition requires its original scene services.");
        services.scannerTransition(player, enabled);
    }
    state.flags1140[2] = enabled;
    const auto matrix = dooranimation::concatenate(player.matrix, player.model->matrix(0, player.actorType == 0 ? 49 : 0));
    state.field1144 = {matrix[2], matrix[6], matrix[10]};
    bool strafe = false;
    const auto flags = controls.settings.flags;
    if (!(flags & 4)) {
        strafe = (actions.field30D && (!(flags & 0x40) || state.field1180 != 5) && !state.field11A0 && state.field1114 <= 0) ||
            state.flags1140[2] || controls.motions[4].value > 0;
    }
    if (!state.field11A0 && !state.flags134C[2]) state.fieldF9C = controls.motions[0];
    else {
        for (unsigned i = 0; i < 3; ++i) {
            state.fieldF9C.first[i] = controls.motions[0].first[i] + controls.motions[3].first[i];
            state.fieldF9C.second[i] = controls.motions[0].second[i] + controls.motions[3].second[i];
        }
        state.fieldF9C.first = normalize(state.fieldF9C.first);
        state.fieldF9C.second = normalize(state.fieldF9C.second);
        const float magnitude = controls.motions[0].value + controls.motions[3].value;
        state.fieldF9C.value = magnitude < 1 ? magnitude : 1;
    }
    state.fieldFBC = &controls.motions[2];
    if (strafe) {
        state.fieldFB8 = &controls.motions[flags & 2 ? 0 : 4];
        if (controls.settings.fields[52] != 0) {
            if (!controls.fieldC4) state.fieldFBC = nullptr;
            else state.fieldFB8 = nullptr;
        }
    } else state.fieldFB8 = &controls.motions[0];
    state.flagsFC0[0] = strafe;
    if (state.flags1140[2]) {
        if (!state.fieldFBC || state.fieldFBC->value == 0) state.fieldFBC = &controls.motions[1];
        state.fieldFB8 = nullptr;
    }
    if (state.flags1140[3]) state.fieldFBC = nullptr;
}
