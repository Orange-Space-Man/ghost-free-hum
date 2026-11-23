#include "resources/dolresources.h"
#include "resources/gameedition.h"
#include "game/scene/gameplaycamera.h"
#include "game/startup/startupresources.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    unsigned word(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> data;
        std::string error;
        if (!dolresources::read(executable, address, 4, data, error)) throw std::runtime_error(error);
        return (unsigned(data[0]) << 24) | (unsigned(data[1]) << 16) | (unsigned(data[2]) << 8) | data[3];
    }

    float value(const std::vector<std::uint8_t>& executable, unsigned address) {
        const auto bits = word(executable, address);
        float result;
        std::memcpy(&result, &bits, 4);
        if (!std::isfinite(result)) throw std::runtime_error("Invalid gameplay camera constant.");
        return result;
    }
}

bool gameplaycamera::initialize(const std::vector<std::uint8_t>& executable, const gameplayspawn::Spawn& spawn,
    s3denvironment::Camera& camera, std::string& error, unsigned map) {
    error.clear();
    try {
        if (map != 1 && map != 2 && map != 3 && map != 6) throw std::runtime_error("Unsupported initial gameplay camera map.");
        const auto edition = gameedition::identify(executable);
        const auto addresses = gameedition::cameraFollow(edition);
        const auto descriptor = word(executable, addresses.descriptors + map * 4);
        const auto index = spawn.roomId >> 24;
        if ((word(executable, descriptor) >> 24) <= index) throw std::runtime_error("Missing initial camera room.");
        const auto room = word(executable, descriptor + 20) + index * 48;
        if (word(executable, room) != spawn.roomId || word(executable, room + 4) != 0)
            throw std::runtime_error("Unsupported initial gameplay camera flags.");
        const float minimumX = static_cast<float>(static_cast<std::int32_t>(word(executable, room + 8)));
        const float minimumY = static_cast<float>(static_cast<std::int32_t>(word(executable, room + 12)));
        const float maximumX = static_cast<float>(static_cast<std::int32_t>(word(executable, room + 20)));
        const float maximumZ = static_cast<float>(static_cast<std::int32_t>(word(executable, room + 28)));
        const float depth = value(executable, addresses.presets), margin = value(executable, addresses.presets + 4);
        const float targetHeight = value(executable, addresses.presets + 8), eyeHeight = value(executable, addresses.presets + 12);
        const auto originalValue = [&](unsigned original) {
            return value(executable, gameedition::cameraFollowConstant(edition, original));
        };
        const float inset = originalValue(0x804d0098), threshold = originalValue(0x804d9cec);
        auto position = spawn.position;
        for (float component : position) if (!std::isfinite(component)) throw std::runtime_error("Invalid camera owner position.");
        position[1] += originalValue(0x804d9ab8);
        s3denvironment::Camera result;
        result.aspect = originalValue(0x804d9c80);
        result.fieldOfView = originalValue(0x804d9c84);
        result.nearPlane = originalValue(0x804d9c78);
        result.farPlane = originalValue(0x804d9c7c);
        result.target = position;
        if (result.target[0] < minimumX + margin) result.target[0] = minimumX + margin;
        else if (maximumX - margin < result.target[0]) result.target[0] = maximumX - margin;
        result.target[1] = minimumY + targetHeight;
        if (threshold < position[1] - result.target[1]) result.target[1] = position[1] + targetHeight;
        if (maximumZ - inset < result.target[2]) result.target[2] = maximumZ - inset;
        result.position = {result.target[0], result.target[1] + eyeHeight, maximumZ + depth};
        if (position[2] < maximumZ - inset) result.position[2] -= (maximumZ - inset) - position[2];
        camera = result;
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}
