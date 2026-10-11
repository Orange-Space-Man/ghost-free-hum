#include "game/actors/actorkeysampler.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    unsigned half(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (unsigned(data.at(offset)) << 8) | data.at(offset + 1);
    }

    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (half(data, offset) << 16) | half(data, offset + 2);
    }

    float number(const std::vector<std::uint8_t>& data, std::size_t offset) {
        const auto bits = word(data, offset);
        float result;
        std::memcpy(&result, &bits, 4);
        return result;
    }

    float interpolate(double frame, float start, float end, float first, float second, float outgoing, float incoming) {
        constexpr float seconds = 0x1.111112p-6f;
        const float elapsed = static_cast<float>(frame * seconds) - start * seconds;
        const float inverse = 1.0f / (end * seconds - start * seconds);
        const float square = elapsed * elapsed;
        const float inverseSquare = inverse * inverse;
        const float squareInverse = square * inverse;
        const float threeSquare = 3.0f * square;
        const float cube = square * elapsed;
        const float inverseCube = inverseSquare * inverse;
        const float twiceSquareInverse = 2.0f * squareInverse;
        const float twiceCube = 2.0f * cube;
        const float tangentCube = cube * inverseSquare;
        const float positive = twiceCube * inverseCube;
        const float negative = threeSquare * inverseSquare;
        const float incomingWeight = tangentCube - squareInverse;
        const float outgoingWeight = elapsed + (tangentCube - twiceSquareInverse);
        const float blend = positive - negative;
        const float incomingValue = incoming * incomingWeight;
        const float outgoingValue = outgoing * outgoingWeight;
        const float firstValue = first * (1.0f + blend);
        const float secondValue = second * blend;
        return incomingValue + (outgoingValue + (firstValue - secondValue));
    }

    float sampleFloat(const std::vector<std::uint8_t>& data, std::size_t offset, unsigned descriptor, double frame, float fallback) {
        if (!descriptor) return fallback;
        if (descriptor == 1) return number(data, offset);
        const unsigned count = descriptor & 0x7fff, stride = descriptor & 0x8000 ? 4 : 3;
        if (!count) throw std::runtime_error("Invalid actor float key count.");
        const auto at = [&](unsigned i) { return number(data, offset + std::size_t(i) * 4); };
        if (frame <= at(0)) return at(1);
        if (at((count - 1) * stride) <= frame) return at((count - 1) * stride + 1);
        unsigned index = 0;
        while (index + 1 < count && !(frame < at((index + 1) * stride))) ++index;
        const auto first = index * stride, second = first + stride;
        return interpolate(frame, at(first), at(second), at(first + 1), at(second + 1),
            at(first + stride - 1), at(second + 2));
    }

    std::int32_t sampleRotation(const std::vector<std::uint8_t>& data, std::size_t offset, unsigned descriptor,
        double frame, std::uint16_t& cursor) {
        const auto at = [&](unsigned i) { return static_cast<std::int16_t>(half(data, offset + std::size_t(i) * 2)); };
        if (!descriptor) return 0;
        if (descriptor == 1) return static_cast<std::int32_t>(at(0)) * 16;
        const unsigned count = descriptor & 0x7fff, stride = descriptor & 0x8000 ? 4 : 3;
        if (!count || cursor >= count) throw std::runtime_error("Invalid actor rotation key cursor.");
        unsigned index = cursor;
        float time = static_cast<float>(at(index * stride));
        if (time <= frame) {
            while (index < count - 1) {
                time = static_cast<float>(at((index + 1) * stride));
                if (!(time < frame)) break;
                ++index;
            }
            if (time < frame) return static_cast<std::int16_t>(at(index * stride + 1) * 16);
        } else {
            while (index && frame < time) time = static_cast<float>(at(--index * stride));
            if (frame < time) return static_cast<std::int16_t>(at(index * stride + 1) * 16);
        }
        cursor = static_cast<std::uint16_t>(index);
        const unsigned first = index * stride, second = (index + 1) * stride;
        const float value = 182.04445f * interpolate(frame, static_cast<float>(at(first)), static_cast<float>(at(second)),
            0.087890625f * at(first + 1), 0.087890625f * at(second + 1),
            static_cast<float>(at(first + stride - 1)), static_cast<float>(at(second + 2)));
        const float rounded = value >= 0 ? value + 0.5f : value - 0.5f;
        const auto converted = !std::isfinite(rounded) || rounded < -2147483648.0 || rounded >= 2147483648.0 ?
            std::int32_t(-2147483647 - 1) : static_cast<std::int32_t>(rounded);
        return static_cast<std::int16_t>(converted);
    }
}

actoranimation::JointPose actoranimation::sampleJoint(const actormodel::Resource& key, unsigned joint, double frame,
    actormodel::Attachment& cursors) {
    const auto& data = key.bytes;
    if (joint >= half(data, 2)) throw std::runtime_error("Actor key joint exceeds its channel table.");
    const auto indices = std::size_t(word(data, 24)) + std::size_t(joint) * 36;
    const auto counts = std::size_t(word(data, 28)) + std::size_t(joint) * 18;
    JointPose pose;
    std::uint16_t* rotations[]{&cursors.field0, &cursors.field2, &cursors.field4};
    for (unsigned axis = 0; axis < 3; ++axis) {
        pose.scale[axis] = sampleFloat(data, std::size_t(word(data, 12)) + std::size_t(word(data, indices + axis * 4)) * 4,
            half(data, counts + axis * 2), frame, 1);
        pose.rotation[axis] = sampleRotation(data, std::size_t(word(data, 16)) + std::size_t(word(data, indices + (axis + 3) * 4)) * 2,
            half(data, counts + (axis + 3) * 2), frame, *rotations[axis]);
        pose.translation[axis] = sampleFloat(data, std::size_t(word(data, 20)) + std::size_t(word(data, indices + (axis + 6) * 4)) * 4,
            half(data, counts + (axis + 6) * 2), frame, 0);
    }
    return pose;
}
