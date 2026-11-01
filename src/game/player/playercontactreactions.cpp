#include "game/player/playercontactreactions.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    float scalar(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
}

playeractions::ContactFrame::ContactFrame(const std::vector<std::uint8_t>& executable) :
    wallAlignment(scalar(executable, glme01::playerWallReactionAlignment)),
    impactHeight(scalar(executable, glme01::playerWallImpactHeight)),
    frontOffset(scalar(executable, glme01::playerWallImpactForwardOffset)),
    turnBlend(scalar(executable, glme01::playerTargetTurnBlend)) {}

void playeractions::ContactFrame::run(playeractor::Player& player, const mapcollision::State& collision,
    const gameplaycamera::CameraOwner& camera, playerevents::LifeEnvironment& life, bool doubledDamage,
    const std::function<void(unsigned, const std::array<float, 3>&, const std::array<float, 3>&)>& effect) const {
    auto& state = player.state;
    auto& actions = *player.components->actions;
    const auto action = [&](unsigned value) { return actions.field310 == 2 && actions.field314 == value; };
    if (player.fieldB0 >= 0 && action(4) && actions.fields31C[6] == 6) {
        std::array<float, 3> normal{};
        if (!collision.polygonNormal(player.fieldB0, normal)) throw std::runtime_error("Player wall reaction requires its contact polygon.");
        const auto& direction = state.direction;
        const float alignment = (normal[0] * direction[0] + normal[1] * direction[1]) + normal[2] * direction[2];
        if (std::abs(alignment) > wallAlignment) {
            auto position = player.position;
            position[1] += impactHeight;
            player.pulseFeedback(0.5, 5, 10);
            std::array<std::uint32_t, 12> payload{};
            payload[11] = 1;
            payload[4] = alignment <= 0 ? 2 : 1;
            if (alignment > 0) for (unsigned i = 0; i < 3; ++i) position[i] += direction[i] * frontOffset;
            actions.request(4, &payload);
            effect(0x3b, position, normal);
        }
    }
    if (action(0x19)) {
        state.field1070 = 5;
        state.field1074 = camera.position;
        state.field1074[1] = player.position[1];
        state.field109C = turnBlend;
    }
    if (action(0x1f) && (player.fieldA0 >= 0 || player.fieldB2 >= 0)) {
        effect(0x2d, player.position, {});
        effect(0x2e, player.position, {});
        playerevents::setLife(player, static_cast<std::int16_t>(player.life) - (doubledDamage ? 30 : 15), life);
    }
}
