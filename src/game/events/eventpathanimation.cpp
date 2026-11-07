#include "game/events/eventpathanimation.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
    unsigned half(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (unsigned(data.at(offset)) << 8) | data.at(offset + 1);
    }

    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (half(data, offset) << 16) | half(data, offset + 2);
    }

    std::array<float, 3> cross(const std::array<float, 3>& a, const std::array<float, 3>& b) {
        return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    }
}

gameevent::CameraPath::CameraPath(const std::vector<std::uint8_t>& resource) : frames(half(resource, 0)) {
    for (unsigned i = 0; i < channels.size(); ++i)
        channels[i] = s3denvironment::readCurve(resource, 0x44, 4 + i * 6, false);
}

void gameevent::CameraPath::sample(float frame, gameplaycamera::CameraOwner& camera) const {
    if (!std::isfinite(frame)) throw std::runtime_error("Invalid event camera frame.");
    for (unsigned i = 0; i < 3; ++i) {
        camera.position[i] = channels[i].sample(frame);
        camera.target[i] = channels[i + 3].sample(frame);
    }
    std::array<float, 3> direction;
    for (unsigned i = 0; i < 3; ++i) direction[i] = camera.position[i] - camera.target[i];
    if (direction[0] == 0 && direction[2] == 0) direction = {1, 0, 0};
    const auto up = cross(direction, cross({0, 1, 0}, direction));
    const float length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
    for (auto& value : direction) value /= length;
    const auto perpendicular = cross(direction, up);
    const float angle = 0.017453292f * channels[6].sample(frame);
    for (unsigned i = 0; i < 3; ++i) camera.up[i] = up[i] * std::cos(angle) + perpendicular[i] * std::sin(angle);
    const auto fieldOfView = channels[7].sample(frame);
    if (fieldOfView >= 10) camera.fieldOfView = fieldOfView;
}

unsigned gameevent::CameraPath::duration() const { return frames; }

gameevent::LightPath::LightPath(const std::vector<std::uint8_t>& resource) {
    const auto records = word(resource, 4), floats = word(resource, 8), integers = word(resource, 12);
    for (unsigned index = 0; index < half(resource, 2); ++index) {
        const auto record = records + index * 0x5c;
        Light light{half(resource, record), half(resource, record + 2), half(resource, record + 4)};
        for (unsigned i = 0; i < 12; ++i) {
            const std::array<unsigned, 9> floatOffsets{8, 14, 20, 26, 32, 38, 44, 56, 62};
            const unsigned offset = i < 9 ? floatOffsets[i] : 0x44 + (i - 9) * 6;
            light.channels[i] = s3denvironment::readCurve(resource, i < 9 ? floats : integers, record + offset, i >= 9);
        }
        lights.push_back(std::move(light));
    }
}

void gameevent::LightPath::sample(float frame, roomlighting::Pool& pool) const {
    for (const auto& source : lights) {
        modellighting::RoomLight light;
        for (unsigned i = 0; i < 3; ++i) {
            light.position[i] = source.channels[i].sample(frame);
            light.direction[i] = source.channels[i + 3].sample(frame) - light.position[i];
            light.color[i] = static_cast<std::uint8_t>(std::clamp(static_cast<int>(source.channels[i + 9].sample(frame)), 0, 255));
        }
        light.color[3] = 255;
        light.referenceDistance = static_cast<std::uint16_t>(source.distance);
        light.brightnessPercent = static_cast<std::uint16_t>(source.brightness);
        if (source.flags & 4) {
            light.cutoff = static_cast<std::uint8_t>(std::clamp(source.channels[6].sample(frame) * 0.5f, 0.0f, 90.0f));
            const float length = std::sqrt(light.direction[0] * light.direction[0] + light.direction[1] * light.direction[1] +
                light.direction[2] * light.direction[2]);
            if (length == 0) light.direction = {0, -1, 0};
            else for (auto& value : light.direction) value /= length;
        }
        pool.add(0, light);
    }
}
