#include "game/actors/charactereffectresources.h"

#include <stdexcept>

charactereffects::Resources::Resources(modeleffects::Factory& models, particles::EffectFactory& particles,
    const unsigned& variant) : models(&models), particles(particles), variant(variant) {}

charactereffects::Resources::Resources(particles::EffectFactory& particles, const unsigned& variant) :
    particles(particles), variant(variant) {}

void charactereffects::Resources::bindModels(modeleffects::Factory& value) { models = &value; }

void charactereffects::Resources::bind(enemies::Manager& value, actors::Registry& registry) {
    manager = &value;
    actors = &registry;
}

characterownership::Resource* charactereffects::Resources::createPrimary(unsigned type, const std::array<float, 3>& position) {
    if (!models) throw std::runtime_error("Character model effects require the original model-effect manager.");
    return models->create(type, position);
}

characterownership::Resource* charactereffects::Resources::createSecondary(unsigned type, const std::array<float, 3>& position,
    const std::array<float, 3>& vector) {
    return particles.create(type, position, vector);
}

std::int32_t charactereffects::Resources::playerValue() {
    if (!manager || !actors) throw std::runtime_error("Character effect resources require the original actor managers.");
    auto* player = dynamic_cast<playeractor::Player*>(actors->actor(manager->appearPoint(0).actor));
    if (!player) throw std::runtime_error("Character effect selection requires the original Player view.");
    return static_cast<std::int32_t>(player->view.fieldsED4[0]);
}

unsigned charactereffects::Resources::gameVariant() { return variant; }
