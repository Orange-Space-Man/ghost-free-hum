#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/actors/actorinitialpose.h"
#include "game/actors/actorposeblend.h"
#include "game/world/dooranimationpose.h"
#include "game/startup/startupresources.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    unsigned half(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        return (unsigned(bytes.at(offset)) << 8) | bytes.at(offset + 1);
    }

    unsigned word(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        return (half(bytes, offset) << 16) | half(bytes, offset + 2);
    }

    float number(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        const auto bits = word(bytes, offset);
        float result;
        std::memcpy(&result, &bits, 4);
        return result;
    }

    constexpr actormodel::Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};

}

actormodel::InitialPose::InitialPose(const std::vector<std::uint8_t>& executable) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, glme01::sineTable, 4096 * 4, bytes, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < sine.size(); ++i) sine[i] = number(bytes, i * 4);
    if (!dolresources::read(executable, glme01::cosineTable, 4096 * 4, bytes, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < cosine.size(); ++i) cosine[i] = number(bytes, i * 4);
    if (!dolresources::read(executable, glme01::arctangentTable, 1025 * 2, bytes, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < arcTangent.size(); ++i) arcTangent[i] = static_cast<std::uint16_t>(half(bytes, i * 2));
}

std::array<float, 3> actormodel::InitialPose::horizontalDirection(std::uint16_t angle) const {
    return {sine[angle >> 4], 0, cosine[angle >> 4]};
}

std::uint16_t actormodel::InitialPose::angle(double x, double y) const {
    const auto lookup = [&](double numerator, double denominator) {
        if (denominator == 0) return unsigned(arcTangent[0]);
        const float index = 0.5f + 1024.0f * static_cast<float>(numerator / denominator);
        if (!std::isfinite(index) || index < 0 || index >= 1025) throw std::runtime_error("Invalid original actor angle table index.");
        return unsigned(arcTangent[static_cast<unsigned>(index)]);
    };
    unsigned result;
    if (y < 0) {
        y = -y;
        if (x >= 0) result = y <= x ? 0U - lookup(y, x) : 0xc000 + lookup(x, y);
        else {
            x = -x;
            result = x < y ? 0xc000 - lookup(x, y) : 0x8000 + lookup(y, x);
        }
    } else if (x < 0) {
        x = -x;
        result = y <= x ? 0x8000 - lookup(y, x) : 0x4000 + lookup(x, y);
    } else result = x < y ? 0x4000 - lookup(x, y) : lookup(y, x);
    return static_cast<std::uint16_t>(result);
}

std::array<std::int16_t, 3> actormodel::InitialPose::angles(const Matrix& matrix) const {
    std::uint16_t x, y, z;
    if (0.001f <= 1.0f - matrix[8]) {
        if (0.001f <= 1.0f + matrix[8]) {
            if (matrix[10] >= 0) {
                x = angle(matrix[10], matrix[9]);
                z = angle(matrix[0], matrix[4]);
            } else {
                x = angle(-double(matrix[10]), -double(matrix[9]));
                z = angle(-double(matrix[0]), -double(matrix[4]));
            }
            const float cosineValue = cosine[x >> 4];
            if (cosineValue <= 0.008f) {
                if (-0.008f <= cosineValue) {
                    const float sineValue = sine[x >> 4];
                    y = sineValue <= 0.0001f ? angle(-double(matrix[9]), matrix[8] * sineValue) :
                        angle(matrix[9], -matrix[8] * sineValue);
                } else y = angle(-double(matrix[10]), matrix[8] * cosineValue);
            } else y = angle(matrix[10], -matrix[8] * cosineValue);
        } else {
            x = 0;
            y = 0x4000;
            z = angle(matrix[5], matrix[6]);
        }
    } else {
        x = 0;
        y = 0xc000;
        z = angle(-double(matrix[2]), -double(matrix[1]));
    }
    return {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), static_cast<std::int16_t>(z)};
}

actormodel::Matrix actormodel::InitialPose::jointMatrix(const actoranimation::JointPose& pose, bool scaled) const {
    const unsigned x = (static_cast<unsigned>(pose.rotation[0]) >> 4) & 4095;
    const unsigned y = (static_cast<unsigned>(pose.rotation[1]) >> 4) & 4095;
    const unsigned z = (static_cast<unsigned>(pose.rotation[2]) >> 4) & 4095;
    const float sx = sine[x], sy = sine[y], sz = sine[z];
    const float cx = cosine[x], cy = cosine[y], cz = cosine[z];
    const float sxcz = sx * cz, sxsz = sx * sz, cxcz = cx * cz, cxsz = cx * sz;
    Matrix result{cy * cz, sy * sxcz - cxsz, sxsz + sy * cxcz, pose.translation[0],
        cy * sz, cxcz + sy * sxsz, sy * cxsz - sxcz, pose.translation[1],
        -sy, cy * sx, cy * cx, pose.translation[2]};
    if (scaled) {
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned column = 0; column < 3; ++column) result[row * 4 + column] *= pose.scale[column];
    }
    return result;
}

actormodel::Matrix actormodel::InitialPose::rotation(const std::array<std::int16_t, 3>& angles) const {
    return jointMatrix({{1, 1, 1}, {angles[0], angles[1], angles[2]}, {0, 0, 0}}, true);
}

void actormodel::InitialPose::initialize(Instance& instance) const {
    if (!instance.model) return;
    const auto& data = instance.model->bytes;
    const unsigned joints = half(data, 8);
    const auto hierarchy = word(data, 48);
    if (instance.key) {
        struct Visit {
            unsigned record;
            Matrix parent;
            std::array<float, 3> scale{};
        };
        std::vector<Visit> pending{{0, identity, {1, 1, 1}}};
        std::vector<bool> seen(joints);
        const bool embeddedScale = (word(instance.key->bytes, 8) & 2) != 0;
        while (!pending.empty()) {
            const auto visit = pending.back();
            pending.pop_back();
            if (visit.record >= joints || seen.at(visit.record)) throw std::runtime_error("Invalid initial actor hierarchy.");
            seen[visit.record] = true;
            const auto record = std::size_t(hierarchy) + std::size_t(visit.record) * 16;
            const auto index = half(data, record);
            auto& attachment = instance.attachment(index);
            if (attachment.object) throw std::runtime_error("Initial actor pose must precede component attachment.");
            auto pose = actoranimation::sampleJoint(*instance.key, index, instance.frame, attachment);
            std::array<float, 3> scale{};
            if (!embeddedScale) {
                for (unsigned axis = 0; axis < 3; ++axis) {
                    scale[axis] = visit.scale[axis] * pose.scale[axis];
                    pose.translation[axis] *= visit.scale[axis];
                }
            }
            const auto unscaled = dooranimation::concatenate(visit.parent, jointMatrix(pose, embeddedScale));
            if (embeddedScale) instance.matrix(0, index) = unscaled;
            else {
                const Matrix scaling{scale[0], 0, 0, 0, 0, scale[1], 0, 0, 0, 0, scale[2], 0};
                instance.matrix(0, index) = dooranimation::concatenate(unscaled, scaling);
            }
            const auto child = static_cast<std::int16_t>(half(data, record + 2));
            const auto sibling = static_cast<std::int16_t>(half(data, record + 4));
            if (child) pending.push_back({static_cast<unsigned>(static_cast<int>(visit.record) + child), unscaled,
                embeddedScale ? std::array<float, 3>{} : scale});
            if (sibling) pending.push_back({static_cast<unsigned>(static_cast<int>(visit.record) + sibling), visit.parent, visit.scale});
        }
    }
    updateEnvelopes(instance);
    if (instance.blend != 0) throw std::runtime_error("Initial actor pose requires the original zero blend state.");
    for (unsigned joint = 0; joint < joints; ++joint) instance.matrix(3, joint) = instance.matrix(0, joint);
}
