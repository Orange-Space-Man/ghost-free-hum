#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/actors/charactercreationstate.h"
#include "game/startup/startupresources.h"

#include <stdexcept>

charactercreation::Properties::Properties(const std::vector<std::uint8_t>& executable,
    const enemycreation::Catalog& catalog, const enemytypes::Manager& parameters) : catalog(catalog), parameters(parameters) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, glme01::characterSavedTypes, savedTypes.size() * 4, bytes, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < savedTypes.size(); ++i) {
        savedTypes[i] = (unsigned(bytes[i * 4]) << 24) | (unsigned(bytes[i * 4 + 1]) << 16) |
            (unsigned(bytes[i * 4 + 2]) << 8) | bytes[i * 4 + 3];
    }
}

int charactercreation::Properties::group(unsigned type) const { return catalog.group(type); }

int charactercreation::Properties::savedIndex(unsigned type) const {
    for (unsigned i = 0; i < savedTypes.size(); ++i) if (savedTypes[i] == type) return static_cast<int>(i);
    return -1;
}

std::uint32_t charactercreation::Properties::life(unsigned type, const playerrecord::Record& player) const {
    if (catalog.group(type) == 3) {
        const int index = savedIndex(type);
        if (index >= 0) {
            const auto value = static_cast<std::int16_t>((unsigned(player[4 + index * 2]) << 8) | player[5 + index * 2]);
            return static_cast<std::uint32_t>(static_cast<std::int32_t>(value));
        }
    }
    return parameters.parameters(type).entries().at(0).bits;
}

void charactercreation::Properties::initializeLife(actors::MoveActor& actor, characterowner::State& state,
    const playerrecord::Record& player) const {
    const auto value = life(actor.actorType, player);
    actor.life = static_cast<std::uint16_t>(value);
    state.life = value;
}

std::uint32_t charactercreation::Properties::color(unsigned type) const {
    return parameters.parameters(type).entries().at(25).bits;
}

void charactercreation::Properties::writeColor(actors::MoveActor& actor, std::uint32_t value) const {
    actor.fieldsE9 = {static_cast<std::uint8_t>(value >> 24), static_cast<std::uint8_t>(value >> 16),
        static_cast<std::uint8_t>(value >> 8)};
    actor.fieldEC = static_cast<std::uint8_t>(value);
}

void charactercreation::Properties::updateEffects(const actors::MoveActor& actor, roomlighting::Pool& lights) const {
    if (catalog.group(actor.actorType) != 3) return;
    const float intensity = 0.003921568859368563f * static_cast<float>(actor.fieldE0 & 255);
    if (!(0 < intensity)) return;
    const auto color = parameters.parameters(actor.actorType).entries().at(24).bits;
    modellighting::RoomLight light;
    light.position = actor.position;
    for (unsigned i = 0; i < 3; ++i) {
        const auto component = (color >> (24 - i * 8)) & 255;
        light.color[i] = static_cast<std::uint8_t>(static_cast<int>(static_cast<float>(component) * intensity));
    }
    light.color[3] = 255;
    light.referenceDistance = 40;
    light.brightnessPercent = 80;
    lights.add(0, light);
}
