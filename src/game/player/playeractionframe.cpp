#include "game/player/playeractionframe.h"
#include "game/actors/actorpathsampler.h"

#include <stdexcept>

playeractions::Frame::Frame(const std::vector<std::uint8_t>& executable, const actormodel::InitialPose& pose) :
    context(executable), update(executable, pose), fallback(executable, 0) {}

unsigned playeractions::Frame::fallbackDuration(unsigned entry, const actormodel::Resources& models) const {
    const auto key = models.key(0, fallback.entries.at(entry));
    if (!key) throw std::runtime_error("Player action fallback requires the original Luigi animation key.");
    return (unsigned(key->bytes.at(4)) << 8) | key->bytes.at(5);
}

void playeractions::Frame::run(playeractor::Player& player, const mapcollision::State& collision, const FrameServices& services) const {
    auto& actions = *player.components->actions;
    const auto input = context.build(player, collision, services.catalog, services.models);
    const auto previousMode = actions.fields31C[4];
    const UpdateServices updates{services.flags, services.catalog, services.globalSound,
        [&](unsigned entry) { return fallbackDuration(entry, services.models); }};
    const auto animation = update.run(actions, input, updates);
    apply(player, animation, previousMode, services);
}

void playeractions::Frame::apply(playeractor::Player& player, unsigned animation, unsigned previousMode, const FrameServices& services) const {
    auto& state = player.state;
    const auto& actions = *player.components->actions;
    if (previousMode != actions.fields31C[4] && actions.fields31C[4] == 6) {
        if (!services.positionalSound) throw std::runtime_error("Player action transition requires positional sound playback.");
        services.positionalSound(0x281c, player.position);
    }
    if (!state.fieldF98 && animation != player.animationState) player.selectAnimation(animation);
    if (state.flags134C[1]) {
        player.flags |= 0x1000;
        if (actions.field310 == 1) {
            if (state.fieldFB8 && state.fieldFB8->value > 0) state.fieldF68 = state.fieldF68 * 0.6f * state.fieldFB8->value;
            else if (state.fieldFBC && state.fieldFBC->value > 0) state.fieldF68 = state.fieldF68 * 0.3f * state.fieldFBC->value;
        }
    }
    if (actions.fields304[2] && actions.field300) {
        player.flags |= 0x1000;
        state.fieldF68 *= 0.25f;
    }
    if (!actions.fields304[1] && actions.fields304[0]) player.blendFramesRemaining = 1;
    if (actions.field310 != 2 || (actions.field314 != 4 && actions.field314 != 0xe &&
        actions.field314 != 0x13 && actions.field314 != 0x1d)) return;
    std::array<float, 3> displacement;
    const auto sample = [&](unsigned type, unsigned animation) {
        const auto& info = services.catalog.type(type);
        const auto& selected = info.animations.at(animation);
        return actoranimation::pathDisplacement(services.models, info.model, selected[0], player.animationFrame - player.animationFrameStep,
            player.animationFrame, (selected[5] & 2) != 0, displacement);
    };
    if (!sample(player.actorType, player.animationState) && !sample(0, fallback.entries.at(actions.field60))) return;
    const auto rotation = playerorientation::toMatrix(state.quaternion);
    state.fieldF70 = 1;
    for (unsigned i = 0; i < 3; ++i) state.fieldF74[i] = rotation[i * 4] * displacement[0] +
        rotation[i * 4 + 1] * displacement[1] + rotation[i * 4 + 2] * displacement[2];
}
