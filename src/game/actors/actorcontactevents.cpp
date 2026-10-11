#include "game/actors/actorregistry.h"
#include "game/player/playerresourcepools.h"

#include <cmath>
#include <stdexcept>

void actors::MoveActor::updateActorContacts(const Registry& registry, enemies::Manager& manager,
    const actorcollision::Manager& collision, const playerresources::RecordPool44& pool) {
    auto* owner = manager.activeOwner(*this);
    const auto enabled = [&](const MoveActor& actor) {
        return (construction.catalog.type(actor.actorType).animations.at(actor.animationState)[5] & 0x400) != 0;
    };
    for (unsigned i = 0; i < registry.count(); ++i) {
        auto& other = *registry.actor(i);
        auto* otherOwner = manager.activeOwner(other);
        if (i == actorId || !enabled(*this) || !enabled(other)) continue;
        if (construction.catalog.type(actorType).field4 == 0 && otherOwner && !otherOwner->event(27)) continue;
        if (construction.catalog.type(other.actorType).field4 == 0 && owner && !owner->event(27)) continue;
        if (!resolveActorContact(other, collision)) continue;
        if (otherOwner && otherOwner->event(28)) {
            auto source = other.position;
            contactEvent(registry, manager, source);
        }
        if (owner && owner->event(28)) {
            auto source = position;
            other.contactEvent(registry, manager, source);
        }
    }
    if (!enabled(*this)) return;
    const auto& element = *collision.elements.at(actorType);
    const actorcollision::Volume volume = field788 ? *field788 : actorcollision::Volume{element.field8, element.secondary, element.extended};
    std::array<float, 3> correction;
    if (pool.contact(correction, volume, position)) {
        for (unsigned i = 0; i < 3; ++i) position[i] += correction[i];
    }
}

void actors::MoveActor::contactEvent(const Registry&, enemies::Manager&, std::array<float, 3>& source) {
    const float inverse = 1.0f / std::sqrt((source[0] * source[0] + source[1] * source[1]) + source[2] * source[2]);
    for (auto& value : source) value *= inverse;
}

void playeractor::Player::contactEvent(const actors::Registry& registry, enemies::Manager& manager, std::array<float, 3>&) {
    const auto& actions = *components->actions;
    if (actions.field310 == 2 && actions.field314 == 4) return;
    auto* contact = registry.actor(static_cast<unsigned>(field9C));
    if (state.field11A0) {
        for (auto** entry = state.field11A0->actor; entry && *entry; ++entry) {
            if (*entry == contact) return;
        }
    }
    auto* owner = manager.activeOwner(*contact);
    if (!owner) throw std::runtime_error("Player contact requires the contacted actor's original owner.");
    characterevent::Event event(12);
    event.fields[1] = static_cast<std::uint32_t>(state.fieldF34);
    owner->event(event);
}

void playeractor::Alternate::contactEvent(const actors::Registry&, enemies::Manager&, std::array<float, 3>&) {}
