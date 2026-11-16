#include "game/actors/actorcapturepose.h"
#include <stdexcept>

actorcapture::PoseBinding actorcapture::resolvePoseBinding(const playermodels::VacuumEntries& entries,
    const actors::MoveActor& actor, const std::function<const Vector*()>& vacuumDirection) {
    PoseBinding binding;
    const auto* entry = entries.findTarget(&actor, 0);
    if (!entry) return binding;
    if (!entry->effect || !entry->effect->model)
        throw std::runtime_error("Original captured-actor motion requires its vacuum effect path.");
    binding.direction = vacuumDirection ? vacuumDirection() : nullptr;
    if (!binding.direction) throw std::runtime_error("Original captured-actor motion requires the player's vacuum direction.");
    binding.path = entry->effect->model.get();
    return binding;
}

void actorcapture::updateCapturedActors(actors::Registry& registry, Motion& motion,
    const playermodels::VacuumEntries& entries, const std::function<const Vector*()>& vacuumDirection,
    actormodel::Matrix& retainedTransform) {
    for (unsigned i = 0; i < registry.count(); ++i) {
        auto& actor = *registry.actor(i);
        if (!(actor.flags & 0x80000)) continue;
        const auto binding = resolvePoseBinding(entries, actor, vacuumDirection);
        actor.updateCapture(motion, binding.path, binding.direction, retainedTransform);
    }
}
