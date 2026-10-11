#include "game/player/playervelocityupdate.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    std::vector<std::uint8_t> slope() {
        std::vector<std::uint8_t> bytes(104);
        const auto word = [&](unsigned offset, std::uint32_t value) {
            for (unsigned i = 0; i < 4; ++i) bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (24 - i * 8));
        };
        word(0x28, 64);
        word(0x2c, 80);
        word(64, 0);
        float y = 0.8f, z = 0.6f;
        std::uint32_t bits;
        std::memcpy(&bits, &y, 4); word(68, bits);
        std::memcpy(&bits, &z, 4); word(72, bits);
        return bytes;
    }
}

int main() {
    try {
        mission::Resources resources;
        mapcollision::State collision(resources);
        playermotion::VelocityInput input;
        input.maximumSpeed = 100;
        input.floor = 0xfffe;
        input.applyReflection = true;
        input.drag = 0.5f;
        input.idleFriction = 0.5f;
        input.gravity = -1;
        input.emitSound = true;
        input.position = {1, 2, 3};
        unsigned sounds = 0;
        const auto sound = [&](std::uint32_t id, const playermotion::Vector& position) {
            require(id == 0x845 && position == input.position, "Floor impact retains its original positional sound request.");
            ++sounds;
        };
        auto result = playermotion::updateVelocity({8, -8, 0}, input, collision, sound);
        require(result == playermotion::Vector{2, -1, 0} && sounds == 1,
            "Contact response precedes global drag, tangential friction and gravity.");
        playermotion::updateVelocity({0, -4, 0}, input, collision, sound);
        require(sounds == 1, "The original floor sound threshold is strict.");
        input.floor = 0xffff;
        result = playermotion::updateVelocity({8, -8, 0}, input, collision, sound);
        require(result == playermotion::Vector{4, -5, 0} && sounds == 1,
            "Missing ground retains gravity and drag without treating the fallback up vector as contact.");
        input = {};
        input.applyGravity = false;
        input.maximumSpeed = 5;
        require(playermotion::updateVelocity({6, 8, 0}, input, collision, {}) == playermotion::Vector{3, 8, 0},
            "The original speed cap measures all three components but only scales horizontal velocity.");
        input.maximumSpeed = 100;
        input.applyGravity = true;
        input.inverted = true;
        input.gravity = 3;
        require(playermotion::updateVelocity({}, input, collision, {}) == playermotion::Vector{0, -3, 0},
            "Inverted movement reverses the gravity increment.");
        input.applyGravity = false;
        input.floor = 0xfffe;
        playercontrols::Motion motion{0.5f, {}, {1, 0, 0}};
        input.motion = &motion;
        input.applyMotion = true;
        input.threshold = 0.5f;
        input.acceleration = 4;
        input.reversal = 3;
        require(playermotion::updateVelocity({}, input, collision, {}) == playermotion::Vector{2, 0, 0},
            "Motion begins at threshold equality and uses the world-space direction channel.");
        require(playermotion::updateVelocity({-3, 0, 0}, input, collision, {}) == playermotion::Vector{3, 0, 0},
            "Opposing velocity selects the original reversal multiplier.");
        input.threshold = 0.6f;
        require(playermotion::updateVelocity({}, input, collision, {}) == playermotion::Vector{},
            "Motion below the selected threshold does not accelerate.");
        collision.load(slope());
        input.threshold = 0;
        input.floor = 0;
        input.applyFriction = false;
        motion.second = {0, 0, 1};
        result = playermotion::updateVelocity({}, input, collision, {});
        require(std::abs(result[1] + 0.96f) < 0.00001f && std::abs(result[2] - 1.28f) < 0.00001f,
            "Slope motion is projected into the contact plane and scaled by the normal's vertical component.");
        playermotion::Vector direction{1, 7, 0};
        input.applyMotion = false;
        input.applyDirection = true;
        input.direction = &direction;
        input.directionalForce = 3;
        result = playermotion::updateVelocity({}, input, collision, {});
        require(result == playermotion::Vector{3, 0, 0}, "Directional force ignores its input Y and follows the floor tangent.");
        std::cout << "Original player velocity stages verified.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
