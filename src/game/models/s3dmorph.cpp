#include "game/models/s3dmorph.h"

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
        if (!std::isfinite(value)) throw std::runtime_error("Invalid S3D morph value.");
        return value;
    }
}

bool s3dmorph::Animation::read(const std::vector<std::uint8_t>& shapes, const std::vector<std::uint8_t>& keys,
    std::size_t positions, std::string& error) {
    error.clear();
    try {
        Animation result;
        result.frames = word(keys, 4);
        result.flags = word(keys, 6);
        if (!result.frames) throw std::runtime_error("Empty S3D morph animation.");
        std::size_t destination = dword(shapes, 44), targetList = dword(shapes, 40);
        for (unsigned index = 0; index < word(shapes, 8); ++index) {
            const std::size_t record = dword(shapes, 16) + index * 20;
            Group group;
            const auto count = word(shapes, record + 18), targets = word(shapes, record + 16);
            if (!targets) throw std::runtime_error("Empty S3D morph group.");
            for (unsigned vertex = 0; vertex < count; ++vertex) {
                const auto entry = word(shapes, destination);
                destination += 2;
                if (entry >= positions) throw std::runtime_error("Invalid S3D morph destination.");
                group.indices.push_back(entry);
            }
            for (unsigned target = 0; target < targets; ++target) {
                Target loaded;
                const auto id = word(shapes, targetList);
                targetList += 2;
                const auto first = word(shapes, dword(shapes, 20) + id * 8 + 2);
                for (unsigned vertex = 0; vertex < count; ++vertex) {
                    const auto encoded = word(shapes, std::size_t(dword(shapes, 32)) + (first + vertex) * 2);
                    const std::size_t offset = dword(shapes, 24) + (encoded & 0x1fff) * 12;
                    std::array<float, 3> value;
                    for (unsigned axis = 0; axis < 3; ++axis) {
                        value[axis] = number(shapes, offset + axis * 4);
                        if (encoded & (0x8000 >> axis)) value[axis] = -value[axis];
                    }
                    loaded.positions.push_back(value);
                }
                if (targets != 1) {
                    const auto channel = word(keys, dword(keys, 28) + index * 2) + target;
                    const auto descriptor = word(keys, dword(keys, 24) + channel * 2);
                    loaded.stride = (descriptor & 0x8000) ? 4 : 3;
                    const auto keyCount = descriptor & 0x7fff;
                    if (!keyCount) throw std::runtime_error("Empty S3D morph weight curve.");
                    const std::size_t start = dword(keys, 16) + std::size_t(dword(keys, dword(keys, 20) + channel * 4)) * 4;
                    for (unsigned value = 0; value < keyCount * loaded.stride; ++value) loaded.keys.push_back(number(keys, start + value * 4));
                    for (unsigned key = 1; key < keyCount; ++key)
                        if (loaded.keys[key * loaded.stride] <= loaded.keys[(key - 1) * loaded.stride])
                            throw std::runtime_error("Unordered S3D morph keys.");
                }
                group.targets.push_back(std::move(loaded));
            }
            result.groups.push_back(std::move(group));
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

float s3dmorph::Animation::Target::sample(float frame) const {
    if (keys.empty()) return 1;
    const auto last = keys.size() - stride;
    if (frame <= keys[0]) return keys[1];
    if (frame >= keys[last]) return keys[last + 1];
    std::size_t index = 0;
    while (frame >= keys[index + stride]) index += stride;
    const auto next = index + stride;
    const float time = frame * 0.016666668f - keys[index] * 0.016666668f;
    const float squared = time * time;
    const float inverse = 1.0f / (keys[next] * 0.016666668f - keys[index] * 0.016666668f);
    const float inverseSquared = inverse * inverse;
    const float cubic = squared * time * inverseSquared;
    const float blend = 2.0f * squared * time * inverseSquared * inverse - 3.0f * squared * inverseSquared;
    return keys[next + 2] * (cubic - squared * inverse) +
        keys[index + (stride == 4 ? 3 : 2)] * (time + (cubic - 2.0f * squared * inverse)) +
        (keys[index + 1] * (1.0f + blend) - keys[next + 1] * blend);
}

void s3dmorph::Animation::apply(float frame, std::vector<std::array<float, 3>>& positions) const {
    if (!std::isfinite(frame)) throw std::runtime_error("Invalid S3D morph frame.");
    for (const auto& group : groups) for (const auto index : group.indices) positions.at(index) = {};
    for (const auto& group : groups) {
        float total = 0;
        for (const auto& target : group.targets) {
            const float weight = target.sample(frame);
            for (unsigned vertex = 0; vertex < group.indices.size(); ++vertex) for (unsigned axis = 0; axis < 3; ++axis)
                positions.at(group.indices[vertex])[axis] += weight * target.positions[vertex][axis];
            total += weight;
        }
        for (const auto index : group.indices) for (auto& component : positions.at(index)) component /= total;
    }
}

float s3dmorph::Animation::advance(float frame, float step) const {
    frame += step;
    if (frame >= frames) {
        if (flags & 1) frame = float(frames);
        else frame -= frames;
    }
    return frame;
}
