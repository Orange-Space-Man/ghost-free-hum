#include "game/models/s3danimation.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        require(offset <= data.size() && data.size() - offset >= 2, "Truncated S3D key animation.");
        return (data[offset] << 8) | data[offset + 1];
    }

    unsigned dword(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (word(data, offset) << 16) | word(data, offset + 2);
    }

    float number(const std::vector<std::uint8_t>& data, std::size_t offset) {
        const auto bits = dword(data, offset);
        float result;
        std::memcpy(&result, &bits, 4);
        require(std::isfinite(result), "Non-finite S3D animation key.");
        return result;
    }
}

bool s3danimation::Animation::read(const std::vector<std::uint8_t>& data, std::string& error) {
    error.clear();
    try {
        require(data.size() >= 32 && word(data, 0) == 0, "Unsupported S3D animation header.");
        const auto count = word(data, 2);
        Animation result;
        result.frames = word(data, 4);
        result.flags = dword(data, 8);
        require(count && result.frames, "Empty S3D animation.");
        const std::array<std::size_t, 6> offsets{dword(data, 12), dword(data, 16), dword(data, 20),
            dword(data, 24), dword(data, 28), data.size()};
        require(offsets[0] >= 32, "Invalid S3D animation section.");
        for (unsigned i = 0; i < 5; ++i) require(offsets[i] <= offsets[i + 1], "Invalid S3D animation section order.");
        require(count <= (offsets[4] - offsets[3]) / 36 && count <= (offsets[5] - offsets[4]) / 18,
            "Truncated S3D animation channels.");
        result.channels.resize(count);
        for (unsigned node = 0; node < count; ++node) {
            for (unsigned component = 0; component < 9; ++component) {
                auto& channel = result.channels[node][component];
                const auto descriptor = word(data, offsets[4] + node * 18 + component * 2);
                const auto keyCount = descriptor & 0x7fff;
                channel.splitTangents = (descriptor & 0x8000) != 0;
                const auto index = dword(data, offsets[3] + node * 36 + component * 4);
                if (!keyCount) continue;
                const unsigned table = component / 3, elementSize = table == 1 ? 2 : 4;
                const unsigned stride = channel.splitTangents ? 4 : 3;
                const std::size_t elements = keyCount == 1 ? 1 : keyCount * stride;
                require(index <= (offsets[table + 1] - offsets[table]) / elementSize &&
                    elements <= (offsets[table + 1] - offsets[table]) / elementSize - index, "Invalid S3D animation key range.");
                for (std::size_t key = 0; key < elements; ++key) {
                    const auto position = offsets[table] + (index + key) * elementSize;
                    channel.keys.push_back(table == 1 ? static_cast<float>(static_cast<std::int16_t>(word(data, position))) : number(data, position));
                }
                if (keyCount > 1) {
                    for (unsigned key = 1; key < keyCount; ++key) require(channel.keys[key * stride] > channel.keys[(key - 1) * stride],
                        "Unordered S3D animation times.");
                }
            }
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

float s3danimation::Animation::value(const Channel& channel, float frame, unsigned component) {
    const bool rotation = component >= 3 && component < 6;
    const auto& keys = channel.keys;
    if (keys.empty()) return component < 3 ? 1.0f : 0.0f;
    if (keys.size() == 1) return rotation ? static_cast<float>(static_cast<std::int16_t>(static_cast<int>(keys[0]) * 16)) : keys[0];
    const unsigned stride = channel.splitTangents ? 4 : 3;
    const auto last = keys.size() - stride;
    if (frame <= keys[0] || frame >= keys[last]) {
        const float endpoint = keys[frame <= keys[0] ? 1 : last + 1];
        return rotation ? static_cast<float>(static_cast<std::int16_t>(static_cast<int>(endpoint) * 16)) : endpoint;
    }
    std::size_t index = 0;
    while (index < last && frame > keys[index + stride]) index += stride;
    const auto next = index + stride;
    const float seconds = 0.016666668f;
    const float time = frame * seconds - keys[index] * seconds;
    const float inverse = 1.0f / (keys[next] * seconds - keys[index] * seconds);
    const float squared = time * time;
    const float inverseSquared = inverse * inverse;
    const float cubic = squared * time * inverseSquared;
    const float blend = 2.0f * squared * time * inverseSquared * inverse - 3.0f * squared * inverseSquared;
    const float tangent = keys[index + (channel.splitTangents ? 3 : 2)];
    const float units = rotation ? 0.087890625f : 1.0f;
    float result = keys[next + 2] * (cubic - squared * inverse) +
        tangent * (time + (cubic - 2.0f * squared * inverse)) +
        (units * keys[index + 1] * (1.0f + blend) - units * keys[next + 1] * blend);
    if (rotation) {
        result *= 182.04445f;
        result = static_cast<float>(static_cast<std::int16_t>(static_cast<int>(result >= 0 ? result + 0.5f : result - 0.5f)));
    }
    return result;
}

std::vector<s3danimation::Pose> s3danimation::Animation::sample(float frame) const {
    require(std::isfinite(frame), "Invalid S3D animation frame.");
    std::vector<Pose> result(channels.size());
    for (std::size_t node = 0; node < channels.size(); ++node) {
        for (unsigned component = 0; component < 9; ++component) {
            const float sampled = value(channels[node][component], frame, component);
            if (component < 3) result[node].scale[component] = sampled;
            else if (component < 6) result[node].rotation[component - 3] = static_cast<std::int16_t>(sampled);
            else result[node].translation[component - 6] = sampled;
        }
    }
    return result;
}
