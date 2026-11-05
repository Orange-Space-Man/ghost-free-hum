#include "game/player/playermainframe.h"
#include "game/player/playerreactionframe.h"

#include <stdexcept>

playeractor::MainFrame::MainFrame(const std::vector<std::uint8_t>& executable) :
    environment(executable), capture(executable), door(executable), ladder(executable), presentation(executable),
    equipment(executable), energy(executable), timers(executable), controls(executable), contact(executable) {}

void playeractor::MainFrame::run(Player& player, MainFrameServices& services) const {
    if (!services.owner || !services.input || !services.furniture || !services.motion)
        throw std::invalid_argument("Player main update requires its original owner, input, furniture and motion bindings.");
    auto& state = player.state;
    const auto& actions = *player.components->actions;
    const auto& input = *player.components->controls;
    services.owner();
    environment.run(player, services.environment);
    capture.run(player, services.events, services.timers.life.effects);
    services.input();
    door.run(player, services.door);
    ladder.run(player, services.ladder);
    presentation.run(player, services.presentation);
    bool warpRequested = false;
    const playerscanner::Input scanInput{state.flags1140[2] && actions.field310 != 2 && input.flagsC[5],
        actions.field310 == 2 || player.components->attached->field2C0 != 0, *state.camera};
    state.flags1140[3] = playerscanner::update(*player.components->scanner, scanInput, warpRequested, services.scanner);
    if (state.flags1140[3] && warpRequested && state.field1150 == 0 && state.field1130 == 0) {
        if (!services.beginWarp) throw std::runtime_error("Scanner warp requires its original scene binding.");
        services.beginWarp(player);
    }
    if (state.field1150 > 0) {
        if (!services.warp) throw std::runtime_error("Active warp requires its original transition update.");
        services.warp(player);
    }
    services.furniture();
    equipment.run(player, services.sound);
    bool vacuumUpdated = false;
    if (state.field1180 == 1) {
        if (!services.vacuum) throw std::runtime_error("Active vacuum requires its original capture update.");
        vacuumUpdated = services.vacuum(player);
    }
    energy.run(player, services.environment.catalog, vacuumUpdated);
    if (state.field1180 == 6 && actions.query74968()) {
        if (!services.elemental) throw std::runtime_error("Elemental equipment requires its original emission update.");
        services.elemental(player);
    }
    if ((state.flags792 & 8) && input.flags30[1]) {
        if (!services.elementalShot) throw std::runtime_error("Elemental input requires its original shot update.");
        services.elementalShot(player);
    }
    std::uint8_t previousReaction;
    if (energy.beginCaptureReaction(player, services.environment.capture, previousReaction)) {
        if (!services.captureReaction) throw std::runtime_error("Active capture requires its original player reaction update.");
        services.captureReaction(player, previousReaction);
    }
    timers.run(player, services.timers);
    auto& life = services.timers.life;
    controls.charge(player, life);
    controls.attack(player);
    controls.blink(player, services.timers.frame);
    controls.special(player, services.timers.frame, services.timers.doubledDamage, services.specialActive, life);
    playeractions::updateState(player, life);
    contact.run(player, services.environment.collision, *state.camera, life, services.timers.doubledDamage,
        [&](unsigned type, const auto& position, const auto& direction) { life.effects.create(type, position, direction); });
    services.motion();
}
