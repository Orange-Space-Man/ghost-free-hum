#include "game/player/playerreactioneffects.h"
#include "game/actors/actortimingsampler.h"
#include "game/world/dooranimationpose.h"

namespace {
    std::array<float, 3> jointPosition(const playeractor::Player& player, unsigned joint) {
        if (!player.model) return {};
        const auto matrix = dooranimation::concatenate(player.matrix, player.model->matrix(0, joint));
        return {matrix[3], matrix[7], matrix[11]};
    }

    void stop(particles::Emitter*& emitter) {
        if (!emitter) return;
        emitter->stop();
        emitter = nullptr;
    }
}

void playeractor::updateReactionEffects(Player& player, const actormodel::Catalog& catalog,
    const actormodel::Resources& resources, particles::EffectFactory& particles, modeleffects::Factory& models,
    const std::function<void(unsigned, const std::array<float, 3>&)>& sound) {
    auto& state = player.state;
    const auto& actions = *player.components->actions;
    const auto first = player.actorType ? player.position : jointPosition(player, 12);
    const auto second = player.actorType ? player.position : jointPosition(player, 58);
    const auto poison = player.actorType ? player.position : jointPosition(player, 53);
    if (actions.fields31C[4] == 10) {
        if (!state.burningEffects[1]) state.burningEffects[1] = particles.create(0x34, first, {});
        if (!state.burningEffects[3]) state.burningEffects[3] = particles.create(0x36, second, {});
        if (!state.burningEffects[0]) state.burningEffects[0] = particles.create(0x33, first, {});
        if (!state.burningEffects[2]) state.burningEffects[2] = particles.create(0x35, second, {});
    } else {
        stop(state.burningEffects[0]);
        stop(state.burningEffects[1]);
    }
    if (actions.fields31C[4] != 10 && (actions.field310 != 2 || actions.field314 != 0x16 || player.animationFrame >= 25)) {
        stop(state.burningEffects[2]);
        stop(state.burningEffects[3]);
    }
    for (unsigned i = 0; i < state.burningEffects.size(); ++i)
        if (state.burningEffects[i]) state.burningEffects[i]->setAttachmentPosition(i < 2 ? first : second, {});
    if (actions.fields31C[4] == 13 || (actions.field310 == 2 && actions.field314 == 0x1c && player.animationFrame < 3)) {
        if (!state.iceEffect) state.iceEffect = models.create(0xb, player.matrix);
        else state.iceEffect->setMatrix(player.matrix);
    } else if (state.iceEffect) {
        state.iceEffect->release();
        state.iceEffect = nullptr;
        particles.create(0x37, player.position, {});
        particles.create(0x38, player.position, {});
        sound(0x813, player.position);
    }
    if (player.components->reactions->state == 4) {
        if (!state.poisonEffect) state.poisonEffect = particles.create(0xe, poison, {});
        else state.poisonEffect->setAttachmentPosition(poison, {});
    } else stop(state.poisonEffect);
    const auto& type = catalog.type(player.actorType);
    const auto timing = resources.timing(type.model, type.animations.at(player.animationState)[0]);
    const float previous = static_cast<float>(double(player.animationFrame) - double(player.animationFrameStep));
    if (actoranimation::timingCrossing(timing.get(), "Lhand_timing", previous, player.animationFrame) <= 0 ||
        actions.field310 != 2 || actions.field314 != 8) return;
    int firstHand = -1, secondHand = -1;
    const auto hand = actions.fields31C[7];
    if (hand == 3) {
        firstHand = player.actorType ? 0 : 0x26;
        secondHand = player.actorType ? 0 : 0x19;
    } else if (hand == 1 || hand == 4) secondHand = player.actorType ? 0 : 0x17;
    for (const int joint : {firstHand, secondHand}) if (joint >= 0) {
        const auto position = jointPosition(player, static_cast<unsigned>(joint));
        particles.create(0x39, position, {});
        particles.create(0x3a, position, {});
    }
}
