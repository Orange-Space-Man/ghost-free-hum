#include "resources/dolresources.h"
#include "game/player/playercontactreaction.h"
#include "game/startup/startupresources.h"

#include <cstring>
#include <stdexcept>

namespace {
    float scalar(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        const unsigned bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
}

playeractor::ContactReaction::ContactReaction(const std::vector<std::uint8_t>& executable) :
    threshold(scalar(executable, 0x80219a38)), height(scalar(executable, 0x804db59c)),
    distance(scalar(executable, 0x804db5a0)), feedback(scalar(executable, 0x804db528)),
    turning(scalar(executable, 0x804d2ba4)) {}

void playeractor::ContactReaction::run(Player& player, const mapcollision::State& collision,
    const gameplaycamera::CameraOwner& camera, bool doubledDamage, playerevents::LifeEnvironment& life) const {
    auto& actions = *player.components->actions;
    auto& state = player.state;
    if (player.fieldB0 >= 0 && actions.field310 == 2 && actions.field314 == 4 && actions.fields31C[6] == 6) {
        std::array<float, 3> normal{0, 1, 0};
        collision.polygonNormal(player.fieldB0, normal);
        const auto& direction = state.direction;
        const float product = (normal[0] * direction[0] + normal[1] * direction[1]) + normal[2] * direction[2];
        if (threshold < (product <= 0 ? -product : product)) {
            auto position = player.position;
            position[1] += height;
            player.pulseFeedback(feedback, 5, 10);
            std::array<std::uint32_t, 12> payload{};
            payload[11] = 1;
            if (product <= 0) payload[4] = 2;
            else {
                for (unsigned i = 0; i < 3; ++i) position[i] += distance * direction[i];
                payload[4] = 1;
            }
            actions.request(4, &payload);
            life.effects.create(0x3b, position, normal);
        }
    }
    if (actions.field310 == 2 && actions.field314 == 0x19) {
        state.field1070 = 5;
        state.field1074 = camera.position;
        state.field1074[1] = player.position[1];
        state.field109C = turning;
    }
    if (actions.field310 == 2 && actions.field314 == 0x1f && (player.fieldA0 >= 0 || player.fieldB2 >= 0)) {
        life.effects.create(0x2d, player.position, {});
        life.effects.create(0x2e, player.position, {});
        playerevents::setLife(player, static_cast<std::int16_t>(player.life) - (doubledDamage ? 30 : 15), life);
    }
}
