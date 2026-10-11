#include "game/models/s3dshadow.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (unsigned(data.at(offset)) << 8) | data.at(offset + 1);
    }

    unsigned dword(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (word(data, offset) << 16) | word(data, offset + 2);
    }

    float number(const std::vector<std::uint8_t>& data, std::size_t offset) {
        const auto bits = dword(data, offset);
        float value;
        std::memcpy(&value, &bits, 4);
        if (!std::isfinite(value)) throw std::runtime_error("Invalid shadow projection value.");
        return value;
    }

    s3denvironment::Curve curve(const std::vector<std::uint8_t>& scene, std::size_t descriptor, bool integer) {
        s3denvironment::Curve result;
        const auto count = word(scene, descriptor), first = word(scene, descriptor + 2);
        result.stride = word(scene, descriptor + 4);
        if (result.stride != 1 && result.stride != 3 && result.stride != 4) throw std::runtime_error("Invalid shadow curve stride.");
        if (result.stride != 1 && !count) throw std::runtime_error("Empty shadow curve.");
        const auto table = dword(scene, integer ? 28 : 24);
        const auto elements = result.stride == 1 ? 1 : count * result.stride;
        for (unsigned i = 0; i < elements; ++i) {
            const auto offset = std::size_t(table) + (first + i) * (integer ? 2 : 4);
            result.keys.push_back(integer ? float(static_cast<std::int16_t>(word(scene, offset))) : number(scene, offset));
        }
        for (unsigned i = 1; i < count && result.stride != 1; ++i)
            if (result.keys[i * result.stride] <= result.keys[(i - 1) * result.stride]) throw std::runtime_error("Unordered shadow curve.");
        return result;
    }
}

bool s3dshadow::Animation::read(const std::vector<std::uint8_t>& scene, const std::vector<std::uint8_t>& settings, std::string& error) {
    error.clear();
    try {
        Animation result;
        constexpr unsigned start = 0x22c;
        result.enabled = (word(settings, start + 2) & 1) != 0;
        if (result.enabled) {
            for (unsigned axis = 0; axis < 3; ++axis) {
                result.base.position[axis] = number(settings, start + 4 + axis * 4);
                result.base.target[axis] = number(settings, start + 16 + axis * 4);
            }
            result.base.fieldOfView = number(settings, start + 28);
            result.base.nearPlane = number(settings, start + 32);
            result.base.farPlane = number(settings, start + 36);
            for (unsigned channel = 0; channel < 4; ++channel)
                result.color[channel] = static_cast<std::int16_t>(word(settings, start + 40 + channel * 2));
            const int index = static_cast<std::int16_t>(word(settings, start));
            if (index >= 0 && unsigned(index) < word(scene, 4)) {
                const auto record = std::size_t(dword(scene, 16)) + index * 0x68;
                for (unsigned channel = 0; channel < 10; ++channel)
                    result.channels[channel] = curve(scene, record + (channel < 7 ? 0x14 + channel * 6 : 0x50 + (channel - 7) * 6), channel >= 7);
            } else if (index < -1) throw std::runtime_error("Invalid shadow animation selector.");
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

s3dshadow::Projection s3dshadow::Animation::sample(float frame) const {
    if (!std::isfinite(frame)) throw std::runtime_error("Invalid shadow projection frame.");
    auto result = base;
    if (!enabled) return result;
    for (unsigned axis = 0; axis < 3; ++axis) {
        result.position[axis] += channels[axis].sample(frame);
        result.target[axis] += channels[axis + 3].sample(frame);
    }
    if (result.position[0] == result.target[0] && result.position[2] == result.target[2]) result.target[2] += 10;
    result.fieldOfView = std::clamp(base.fieldOfView + channels[6].sample(frame), 1.0f, 179.0f);
    for (unsigned channel = 0; channel < 4; ++channel) {
        const auto animated = channel < 3 ? static_cast<int>(channels[channel + 7].sample(frame)) : 0;
        result.color[channel] = std::clamp(int(static_cast<std::int16_t>(color[channel] + animated)), 0, 255) / 255.0f;
    }
    return result;
}
