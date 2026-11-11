#include "game/world/roomstorm.h"
#include "resources/dolresources.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    constexpr unsigned flashKeysAddress = 0x802ff418;
    constexpr unsigned stormDirectionsAddress = 0x802ff4e0;
    constexpr unsigned probabilityAddress = 0x804d9b30;

    template<class T> T read(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, sizeof(T), bytes, error)) throw std::runtime_error(error);
        std::reverse(bytes.begin(), bytes.end());
        T result;
        std::memcpy(&result, bytes.data(), sizeof(result));
        return result;
    }
}

roomlighting::Storm::Storm(const std::vector<std::uint8_t>& executable, const mission::Resources& mission,
    const actormodel::InitialPose& pose) : pose(pose),
    distance(read<float>(executable, 0x804d9b24)), height(read<float>(executable, 0x804d9b28)),
    anchorHeight(read<float>(executable, 0x804da058)), randomDivisor(read<float>(executable, 0x804d9a48)),
    roomMiddle(read<float>(executable, 0x804da040)), roomHeight(read<float>(executable, 0x804da044)),
    roomDepth(read<float>(executable, 0x804da048)), threshold(read<double>(executable, probabilityAddress)) {
    if (const auto* table = mission.getJmpResource("RoomInfo")) {
        for (unsigned row = 0; row < table->count(); ++row) {
            std::uint32_t room, value;
            if (!table->getValue(row, "RoomNo", room) || !table->getValue(row, "Thunder", value))
                throw std::runtime_error("Room storm requires the original RoomInfo fields.");
            if (value >= angles.size()) throw std::runtime_error("Room storm direction is outside the original table.");
            settings.at(room) = static_cast<std::uint8_t>(value);
        }
    }
    for (unsigned i = 0; i < angles.size(); ++i)
        for (unsigned j = 0; j < 2; ++j) angles[i][j] = read<std::uint16_t>(executable, stormDirectionsAddress + i * 20 + j * 2);
    for (unsigned i = 0; i < keys.size(); ++i) {
        for (unsigned j = 0; j < 6; ++j) keys[i].values[j] = read<std::uint16_t>(executable, flashKeysAddress + i * 14 + j * 2);
        keys[i].interpolate = read<std::uint8_t>(executable, flashKeysAddress + i * 14 + 12);
        keys[i].duration = read<std::uint8_t>(executable, flashKeysAddress + i * 14 + 13);
    }
}

void roomlighting::Storm::update(unsigned room, const roomlocation::Room& bounds, unsigned roomFlags,
    const gameplaycamera::CameraOwner& camera, float playerHeight, Pool& lights,
    const std::function<std::uint32_t()>& random, const std::function<void()>& thunder) {
    if (!ownerActive) {
        if (!(roomFlags & 0x380)) {
            const float x = bounds.bounds[3] - (bounds.bounds[3] - bounds.bounds[0]) * roomMiddle - camera.target[0];
            const float y = roomHeight + playerHeight - camera.target[1];
            const float z = roomDepth + bounds.bounds[5] - camera.target[2];
            direction[0] = pose.angle(std::sqrt(x * x + z * z), y);
            direction[1] = static_cast<std::uint16_t>(std::clamp<int>(static_cast<std::int16_t>(pose.angle(z, x)), -0x1500, 0x1500));
        } else direction = {0x1300, 0x1000};
    } else if (++ownerTicks > 60) ownerActive = false;
    const auto frameDirection = direction;
    if (!playing && setting(room) && double(float(random()) / randomDivisor) > threshold) {
        playing = ownerActive = true;
        key = tick = ownerTicks = 0;
        direction = angles[setting(room)];
        ++strikeCount;
        thunder();
    }
    if (!playing) return;
    if (!keys[key].duration || keys[key].duration < tick) {
        ++key;
        if (!keys[key].duration) key = 0;
        tick = 0;
    }
    const auto next = keys[key + 1].duration ? key + 1 : 0;
    auto values = keys[key].values;
    if (keys[key].interpolate == 1) {
        for (unsigned i = 0; i < values.size(); ++i)
            values[i] = static_cast<std::uint16_t>(values[i] + static_cast<int>(
                ((float(keys[next].values[i]) - float(values[i])) / float(keys[key].duration)) * float(tick)));
    }
    const auto vertical = pose.horizontalDirection(frameDirection[0]);
    const auto horizontal = pose.horizontalDirection(frameDirection[1]);
    auto anchor = camera.target;
    if ((camera.flags & 8) || !(camera.flags & 1)) anchor[1] = playerHeight + anchorHeight;
    modellighting::RoomLight light;
    light.position = {anchor[0] + float(double(distance) * vertical[2]) * horizontal[0],
        anchor[1] + float(double(distance) * vertical[0]) + height,
        anchor[2] + float(double(distance) * vertical[2]) * horizontal[2]};
    light.color = {static_cast<std::uint8_t>(values[0]), static_cast<std::uint8_t>(values[1]),
        static_cast<std::uint8_t>(values[2]), 255};
    light.cutoff = static_cast<std::uint8_t>(values[3]);
    light.referenceDistance = values[4];
    light.brightnessPercent = values[5];
    lights.add(7, light);
    ++tick;
    if (!keys[key + 1].duration && keys[key].duration < tick) playing = false;
}

bool roomlighting::Storm::active() const { return playing; }
unsigned roomlighting::Storm::strikes() const { return strikeCount; }
unsigned roomlighting::Storm::setting(unsigned room) const { return settings[room & 255]; }
