#include "game/player/playerattractionevent.h"

#include <stdexcept>

const std::array<float, 3>* playerevents::resolvePosition(const actors::Registry& actors,
    const std::array<float, 3>* position, std::int32_t actor) {
    if (actor < 0) return position;
    const auto* target = actors.actor(unsigned(actor));
    if (!target) throw std::runtime_error("Original player event references an absent actor position.");
    return &target->position;
}

bool playerevents::beginAttraction(playeractor::Player& player, const actors::Registry& actors,
    const std::array<float, 3>* position, std::int32_t actor, std::int32_t duration) {
    if (!player.state.field1180) return false;
    const auto* target = resolvePosition(actors, position, actor);
    if (!target) return false;
    player.state.field10A0 = static_cast<float>(duration);
    player.state.field10A4 = *target;
    player.components->quaternion->quaternion = player.state.quaternion;
    return true;
}
