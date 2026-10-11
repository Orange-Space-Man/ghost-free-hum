#include "resources/dolresources.h"
#include "game/player/playerenvironmentframe.h"
#include "game/actors/actortimingsampler.h"
#include "game/player/playerorientationupdate.h"
#include "game/startup/startupresources.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
    std::vector<std::uint32_t> words(const std::vector<std::uint8_t>& executable, unsigned address, unsigned count) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, count * 4, bytes, error)) throw std::runtime_error(error);
        std::vector<std::uint32_t> result(count);
        for (unsigned i = 0; i < count; ++i) result[i] = (unsigned(bytes[i * 4]) << 24) |
            (unsigned(bytes[i * 4 + 1]) << 16) | (unsigned(bytes[i * 4 + 2]) << 8) | bytes[i * 4 + 3];
        return result;
    }
}

playerenvironment::Frame::Frame(const std::vector<std::uint8_t>& executable) {
    const auto values = words(executable, 0x804db508, static_cast<unsigned>(constants.size()));
    for (unsigned i = 0; i < constants.size(); ++i) std::memcpy(&constants[i], &values[i], 4);
    const auto descriptor = words(executable, 0x80359ac0, 3);
    inverted.field8 = descriptor[0];
    const auto record = words(executable, descriptor[1], 5);
    inverted.secondary.emplace();
    std::copy(record.begin(), record.end(), inverted.secondary->begin());
}

void playerenvironment::Frame::run(playeractor::Player& player, const Services& services) const {
    auto& state = player.state;
    auto& actions = *player.components->actions;
    auto& controls = *player.components->controls;
    if (state.fieldF30 != player.actorType) services.refreshType(player);
    state.fieldF6C = state.field1170 == constants[1] ? constants[4] : constants[9];
    state.fieldF68 = state.fieldF6C;
    if (player.fieldA0 < 0 && player.fieldB2 < 0) {
        auto probe = player.position;
        for (unsigned i = 0; i < 3; ++i) probe[i] += state.fieldF54 * state.direction[i];
        probe[1] += constants[2];
        probe[1] = services.collision.floorHeight(probe) + 5;
        state.field10E0 = static_cast<std::int16_t>(services.collision.floorPolygons(probe, 10)[0]);
    }
    if (services.capture.containsSource(&player)) {
        if (!state.field11A0) services.captureSound(true);
        state.field11A0 = services.captureTarget(player);
    } else {
        if (state.field11A0) services.captureSound(false);
        state.field11A0 = nullptr;
    }
    const auto& vacuum = player.components->vacuum->fieldsC;
    state.flagsFC0[1] = (state.field11A0 && ((state.field11A0->flags & 0x40) ||
        (!controls.settings.flags && !controls.flags30[2]))) || (state.field1180 == 1 && state.flagsFC0[2]) ||
        std::any_of(vacuum.begin(), vacuum.end(), [](auto value) { return value != 0; });
    state.field119D = !services.roomDark(player.fieldB4 >> 24);
    const bool wasInverted = state.flags1330[1] != 0;
    state.flags1330[1] = (state.field1324 & 0x20) && !(actions.field310 == 2 && actions.field314 == 0x20);
    if (wasInverted != bool(state.flags1330[1])) {
        const auto angle = static_cast<std::uint16_t>(static_cast<int>((65536.0f * (0.5f * constants[22])) / 360.0f));
        const auto rotation = services.pose.horizontalDirection(angle);
        state.quaternion = playerorientation::multiply({0, 0, rotation[0], rotation[2]}, state.quaternion);
        actions.request(16);
    }
    player.field788 = state.flags1330[1] ? &inverted : nullptr;
    state.flags1040[0] = float(static_cast<std::int16_t>(player.life)) <
        constants[16] * float(static_cast<std::int16_t>(state.maximumLife));
    if (static_cast<std::int16_t>(player.life) < 1) {
        if (!state.flags1040[1]) {
            const auto& type = services.catalog.type(player.actorType);
            const auto timing = services.models.timing(type.model, type.animations.at(player.animationState)[0]);
            const float previous = static_cast<float>(double(player.animationFrame) - double(player.animationFrameStep));
            if (actoranimation::timingCrossing(timing.get(), "death_timing", previous, player.animationFrame) > constants[1])
                state.flags1040[1] = 1;
        }
    } else state.flags1040[1] = 0;
    state.field7B4 = services.enemies.activeOwner(player);
    unsigned material = 0;
    if (const auto* info = services.collision.polygonInfo()) {
        if (static_cast<std::uint16_t>(player.fieldB2) < info->count())
            info->getValue(static_cast<std::uint16_t>(player.fieldB2), "poly_material", material);
    }
    state.flags134C[1] = material == 1;
    if (static_cast<unsigned>(state.fieldF94) != player.fieldB4) {
        state.fieldF94 = static_cast<std::int32_t>(player.fieldB4);
        services.roomSound(player.fieldB4);
    }
    if (state.flags1330[1] && player.position[1] > constants[23]) {
        auto position = player.position;
        position[1] = constants[24];
        player.setPosition(position);
        player.setVelocity({0, 0, 0});
    }
}
