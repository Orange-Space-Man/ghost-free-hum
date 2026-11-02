#include "game/scene/gameplayresourceowner.h"
#include "game/actors/actortimingsampler.h"

#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }
}

void checkOwnedPlayerFinish(const std::filesystem::path& package, const gamesave::State& saves) {
    gameentry::State entry;
    std::string error;
    if (!gameentry::selectNewFile(saves, 0, entry, error)) throw std::runtime_error(error);
    gameplay::ResourceOwner owner(package, entry);
    auto& managers = owner.managers();
    auto& player = *managers.playerEvents().player(0);
    auto& state = player.state;
    auto& actions = *player.components->actions;
    const actormodel::InitialPose pose(owner.executable());
    const playeractor::FinishFrame finish(owner.executable(), pose);
    const auto resources = managers.preparePlayerDoor(0);
    std::int32_t audioLife = player.life;
    std::vector<unsigned> sounds;
    playerevents::LifeEnvironment life{audioLife, managers.particles().effects(),
        [&](unsigned sound, const auto& position) {
            sounds.push_back(sound);
            managers.playback()->play(sound, position);
        },
        [](const auto&) { throw std::runtime_error("Nonfatal completion unexpectedly requested death audio."); },
        [](const auto&, const auto&, auto, auto, auto, auto) {
            throw std::runtime_error("First-visit player unexpectedly scattered money.");
        }};
    auto& emitters = managers.particles().pool();
    auto* emitter = emitters.emitters.free.take();
    require(emitter != nullptr, "Completion check requires one original emitter slot.");
    emitter->reset();
    emitter->flags = 0xe;
    emitters.destination->append(*emitter);
    for (auto* pool : managers.modelEffects().registered()) {
        if (!pool->owners.empty()) pool->owners[0]->flags |= 1;
    }
    std::string calls;
    bool missionEnabled = true;
    const auto services = managers.preparePlayerFinish(0, life, false,
        [&](bool active) {
            require(bool(emitter->flags & 0x100) != active,
                "Mission scheduling changes before effect pause/resume is published.");
            missionEnabled = !active;
            calls += active ? 'b' : 'e';
        },
        [&](auto& target) {
            require(&target == &player && !(player.flags & 0x40) && !missionEnabled && (emitter->flags & 0x100),
                "Owner notification follows mission/effect pause and player event 0x3a.");
            calls += 'o';
        });
    state.flags10E8[0] = 1;
    state.field1064 = &player;
    finish.run(player, services);
    require(calls == "bo" && !missionEnabled && emitter->flags == 0x10e,
        "Manager-bound completion acquires interaction ownership once.");
    for (const auto* actor : managers.actors().entries()) if (actor)
        require(bool(actor->flags & 0x40) == (actor != &player),
            "Interaction pauses registered actors and then exempts only the initiating player.");
    std::vector<std::pair<actors::MoveActor*, float>> clocks;
    for (auto* actor : managers.actors().entries()) if (actor) clocks.emplace_back(actor, actor->animationFrame);
    const auto playerClock = player.animationFrame;
    managers.updatePoses();
    require(player.animationFrame != playerClock, "The interaction's player keeps advancing his original animation clock.");
    for (const auto& [actor, clock] : clocks) if (actor != &player)
        require(actor->animationFrame == clock, "Paused actors must retain their original animation clocks.");
    for (const auto* pool : managers.modelEffects().registered()) {
        for (const auto& model : pool->owners) if (model->flags & 1)
            require(model->flags & 0x10, "Every active model effect participates in interaction pause.");
    }
    finish.run(player, services);
    state.flags10E8[0] = 0;
    actions.field310 = 2;
    actions.field314 = 0x22;
    finish.run(player, services);
    require(calls == "bo" && !missionEnabled, "Protected interaction retains ownership without repeated callbacks.");
    actions.field310 = 1;
    finish.run(player, services);
    finish.run(player, services);
    require(calls == "boe" && missionEnabled && emitter->flags == 0xe,
        "Interaction release resumes its existing emitter once.");
    for (const auto* actor : managers.actors().entries()) if (actor)
        require(!(actor->flags & 0x40), "Interaction exit clears the registry pause bit.");
    for (const auto* pool : managers.modelEffects().registered()) {
        for (const auto& model : pool->owners) if (model->flags & 1)
            require(!(model->flags & 0x10), "Interaction exit resumes every active model effect.");
    }
    state.field1064 = nullptr;
    const auto& type = resources.catalog.type(player.actorType);
    bool consumed = false;
    for (unsigned animation = 0; animation < type.animations.size() && !consumed; ++animation) {
        const auto timing = resources.models.timing(type.model, type.animations[animation][0]);
        if (actoranimation::sampleTiming(timing.get(), "Ldam_timing", 0) == -10000) continue;
        for (unsigned frame = 1; frame < 600 && !consumed; ++frame) {
            if (actoranimation::timingCrossing(timing.get(), "Ldam_timing", float(frame - 1), float(frame)) <= 0) continue;
            player.animationState = animation;
            player.animationFrame = float(frame);
            player.animationFrameStep = 1;
            state.field112C = 7;
            state.field10C4 = 0;
            const auto previousLife = player.life;
            finish.run(player, services);
            require(player.life == previousLife - 7 && audioLife == player.life && !state.field112C && state.field10C4 > 0,
                "Imported damage timing reaches damage handling and publishes life and invulnerability.");
            require(sounds == std::vector<unsigned>{0x839, 0x2810},
                "Deferred damage submits loss and hurt sounds in original order.");
            finish.run(player, services);
            require(player.life == previousLife - 7 && sounds.size() == 2,
                "Repeated completion must not apply or sound consumed damage again.");
            consumed = true;
        }
    }
    require(consumed, "Imported Luigi animations must expose a deferred-damage marker.");
    bool rejected = false;
    try { managers.preparePlayerFinish(1, life, false, {}, {}); }
    catch (const std::out_of_range&) { rejected = true; }
    require(rejected, "Completion bindings reject a player not owned by the manager.");

    playerinput::PadStatus pad;
    pad.stickX = 87;
    managers.sampleInput(pad, 0);
    managers.updatePlayerInput(0, true, {});
    require(player.components->controls->motions[0].value > 0,
        "Manager input must decode the retained controller against its actual camera.");
    state.field106C = 1;
    managers.updatePlayerInput(0, true, {});
    require(player.components->controls->motions[0].value == 0,
        "Original control lock must suppress input without changing the sampled controller.");
    state.field106C = 0;
    managers.updatePlayerInput(0, true, {});
    require(player.components->controls->motions[0].value > 0,
        "Releasing the original control lock restores the still-held stick.");
    managers.updatePlayerOrientation(0, {}, 0);
    require(player.components->quaternion->field10 == 0,
        "First orientation update initializes the retained angular velocity through direction locking.");
    player.fieldB2 = 0;
    state.fieldFB8 = state.fieldFBC = nullptr;
    state.field1070 = 0;
    state.field10A0 = 0;
    playercomponents::AttachedOutput attachment;
    attachment.limits = {1, 0};
    managers.updatePlayerOrientation(0, attachment, 1);
    require(state.field1070 == 1, "The first attachment limit is not the original attached-turn signal.");
    attachment.limits = {0, 1};
    managers.updatePlayerOrientation(0, attachment, 2);
    require(state.field1070 == 3, "The second attachment limit feeds the original body-turn selector.");
    state.field10A0 = 2;
    managers.updatePlayerOrientation(0, attachment, 3);
    require(state.field1070 == 1, "The original aim delay suppresses attachment-driven turning until expiration.");
}
