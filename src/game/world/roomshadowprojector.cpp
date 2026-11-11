#include "game/world/roomshadowprojector.h"
#include "resources/dolresources.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    constexpr unsigned defaultPresetAddress = 0x802fffe0;
    constexpr unsigned stormPresetsAddress = 0x802ff4e0;
    constexpr unsigned shadowColorAddress = 0x804d00c8;

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

roomshadow::Projector::Projector(const std::vector<std::uint8_t>& executable) {
    const auto preset = [&](unsigned address) {
        return Preset{{read<std::uint16_t>(executable, address), read<std::uint16_t>(executable, address + 2)},
            read<float>(executable, address + 4), read<float>(executable, address + 8),
            read<float>(executable, address + 12), read<int>(executable, address + 16)};
    };
    ordinary = selected = preset(defaultPresetAddress);
    for (unsigned i = 0; i < storms.size(); ++i) storms[i] = preset(stormPresetsAddress + i * 20);
    for (unsigned i = 0; i < 4; ++i) value.color[i] = read<std::uint8_t>(executable, shadowColorAddress + i) / 255.0f;
    opacity = read<std::uint8_t>(executable, shadowColorAddress + 3);
    flashlightNear = read<float>(executable, 0x804da060);
    flashlightFar = read<float>(executable, 0x804da064);
    middle = read<float>(executable, 0x804da040);
    height = read<float>(executable, 0x804da044);
    depth = read<float>(executable, 0x804da048);
    anchorHeight = read<float>(executable, 0x804da058);
}

void roomshadow::Projector::update(const roomlocation::Room& room, unsigned flags,
    const gameplaycamera::CameraOwner& camera, float playerHeight, const actormodel::InitialPose& pose) {
    const auto direction = [&] {
        if (flags & 0x380) return std::array<std::uint16_t, 2>{0x1300, 0x1000};
        const float x = room.bounds[3] - (room.bounds[3] - room.bounds[0]) * middle - camera.target[0];
        const float y = height + playerHeight - camera.target[1];
        const float z = depth + room.bounds[5] - camera.target[2];
        return std::array<std::uint16_t, 2>{pose.angle(std::sqrt(x * x + z * z), y),
            static_cast<std::uint16_t>(std::clamp<int>(static_cast<std::int16_t>(pose.angle(z, x)), -0x1500, 0x1500))};
    };
    frameAngles = selected.angles;
    if (!recovering) frameAngles = selected.angles = direction();
    else {
        if (ticks > 40 && ticks <= 60) {
            opacity = std::max(0, opacity - 10);
            if (opacity < 10) stormPriority = false;
        }
        else if (ticks > 60) {
            selected = ordinary;
            selected.angles = direction();
            opacity += std::clamp(90 - opacity, -3, 3);
            if (opacity == 90) recovering = false;
        }
        ++ticks;
    }
    value.target = camera.target;
    if ((camera.flags & 8) || !(camera.flags & 1)) value.target[1] = playerHeight + anchorHeight;
    publish(pose);
}

void roomshadow::Projector::publish(const actormodel::InitialPose& pose) {
    const auto vertical = pose.horizontalDirection(frameAngles[0]);
    const auto horizontal = pose.horizontalDirection(frameAngles[1]);
    value.position = {value.target[0] + float(double(selected.distance) * vertical[2]) * horizontal[0],
        value.target[1] + float(double(selected.distance) * vertical[0]),
        value.target[2] + float(double(selected.distance) * vertical[2]) * horizontal[2]};
    value.nearPlane = selected.nearPlane;
    value.farPlane = selected.farPlane;
    value.fieldOfView = selected.fieldOfView;
    value.color[3] = opacity / 255.0f;
}

void roomshadow::Projector::strike(unsigned preset, const actormodel::InitialPose& pose) {
    selected = storms.at(preset);
    ticks = 0;
    opacity = 150;
    recovering = true;
    stormPriority = true;
    publish(pose);
}

const s3dshadow::Projection& roomshadow::Projector::projection() const { return value; }

void roomshadow::Projector::selectPlayerLight(const playeractor::View& view) {
    flashlight = view.field8B8 && view.field8A0 == 5 && !stormPriority;
    enabled = flashlight || !view.field895;
}

s3dshadow::Projection roomshadow::Projector::projection(const playeractor::View& view,
    const gameplaycamera::CameraOwner& camera) const {
    auto result = value;
    if (!enabled) result.color[3] = 0;
    if (flashlight) {
        result.spotlight = true;
        result.position = *view.fieldF18;
        auto direction = *view.fieldF1C;
        if (direction == std::array<float, 3>{}) direction = {0, 0, 1};
        for (unsigned i = 0; i < 3; ++i) result.target[i] = result.position[i] + direction[i];
        result.up = camera.up;
        result.fieldOfView = view.fieldF20;
        result.nearPlane = flashlightNear;
        result.farPlane = flashlightFar;
    }
    return result;
}
