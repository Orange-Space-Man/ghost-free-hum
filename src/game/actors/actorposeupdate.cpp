#include "game/actors/actorposeupdate.h"
#include "game/actors/actoreffectstate.h"
#include "game/world/dooranimationpose.h"
#include "game/player/playerorientation.h"

#include <cmath>
#include <stdexcept>

namespace {
    using Vector = std::array<float, 3>;
    using actormodel::Matrix;
    constexpr Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};

    unsigned half(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (unsigned(data.at(offset)) << 8) | data.at(offset + 1);
    }

    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (half(data, offset) << 16) | half(data, offset + 2);
    }

    Vector cross(const Vector& first, const Vector& second) {
        return {first[1] * second[2] - first[2] * second[1], first[2] * second[0] - first[0] * second[2],
            first[0] * second[1] - first[1] * second[0]};
    }

    Vector normalize(Vector value) {
        const float length = std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
        const float factor = 1.0f / length;
        for (auto& component : value) component *= factor;
        return value;
    }

    void aim(Matrix& matrix, const Vector& target, bool preserveRight) {
        Vector forward;
        for (unsigned i = 0; i < 3; ++i) forward[i] = target[i] - matrix[i * 4 + 3];
        forward = normalize(forward);
        const unsigned axis = preserveRight ? 0 : 1;
        Vector reference{matrix[axis], matrix[axis + 4], matrix[axis + 8]};
        auto perpendicular = normalize(preserveRight ? cross(forward, reference) : cross(reference, forward));
        reference = preserveRight ? cross(perpendicular, forward) : cross(forward, perpendicular);
        if (reference[0] * reference[0] + reference[1] * reference[1] + reference[2] * reference[2] == 0)
            throw std::runtime_error("Original actor key cross-product failure.");
        reference = normalize(reference);
        for (unsigned i = 0; i < 3; ++i) {
            matrix[i * 4 + 2] = forward[i];
            matrix[i * 4 + axis] = reference[i];
            matrix[i * 4 + (preserveRight ? 1 : 0)] = perpendicular[i];
        }
    }

    Matrix transform(const actoreffects::State& effect) {
        return {effect.field28[0], 0, 0, effect.field1C[0], 0, effect.field28[1], 0, effect.field1C[1],
            0, 0, effect.field28[2], effect.field1C[2]};
    }
}

void actormodel::samplePose(Instance& instance, const InitialPose& evaluator, const Matrix* root) {
    if (!instance.model || !instance.key) return;
    const auto& data = instance.model->bytes;
    const auto joints = half(data, 8), hierarchy = word(data, 48);
    struct Visit {
        unsigned record;
        Matrix parent;
        Vector scale;
        ResourceRef key;
        float frame;
    };
    std::vector<Visit> pending{{0, root ? *root : identity, {1, 1, 1}, instance.key, instance.frame}};
    std::vector<bool> seen(joints);
    while (!pending.empty()) {
        const auto visit = pending.back();
        pending.pop_back();
        if (visit.record >= joints || seen.at(visit.record)) throw std::runtime_error("Invalid actor pose hierarchy.");
        seen[visit.record] = true;
        const auto record = std::size_t(hierarchy) + std::size_t(visit.record) * 16;
        const auto index = half(data, record);
        auto& attachment = instance.attachment(index);
        auto* effect = static_cast<actoreffects::State*>(attachment.object);
        auto key = visit.key;
        float frame = visit.frame;
        if (effect && (effect->flags & 0x140)) throw std::runtime_error("Actor pose requires original IK or concatenated rotation scratch state.");
        if (effect && (effect->flags & 1)) {
            key = effect->field4;
            frame = effect->field8;
        }
        if (!key) throw std::runtime_error("Actor joint override requires its original key resource.");
        auto joint = actoranimation::sampleJoint(*key, index, frame, attachment);
        const bool embeddedScale = (word(key->bytes, 8) & 2) != 0;
        Vector scale{};
        if (!embeddedScale) {
            for (unsigned i = 0; i < 3; ++i) {
                scale[i] = visit.scale[i] * joint.scale[i];
                joint.translation[i] *= visit.scale[i];
            }
        }
        auto parent = dooranimation::concatenate(visit.parent, evaluator.jointMatrix(joint, embeddedScale));
        if (effect) {
            effect->fields44 = parent;
            if (effect->flags & 4) aim(parent, effect->field10, false);
            if (effect->flags & 2) aim(parent, effect->field10, true);
            if (effect->flags & 8) {
                const auto rotation = playerorientation::toMatrix({effect->field34, effect->field38, effect->field3C, effect->field40});
                for (unsigned row = 0; row < 3; ++row) for (unsigned column = 0; column < 3; ++column)
                    parent[row * 4 + column] = rotation[row * 4 + column];
            }
            if (effect->flags & 0x20) {
                parent = dooranimation::concatenate(parent, transform(*effect));
                effect->fields44 = parent;
            }
        }
        if (!embeddedScale) {
            const Matrix scaling{scale[0], 0, 0, 0, 0, scale[1], 0, 0, 0, 0, scale[2], 0};
            instance.matrix(0, index) = dooranimation::concatenate(parent, scaling);
            if (effect) effect->fields44 = dooranimation::concatenate(effect->fields44, scaling);
        } else instance.matrix(0, index) = parent;
        if (effect && (effect->flags & 0x10)) parent = dooranimation::concatenate(parent, transform(*effect));
        const auto child = static_cast<std::int16_t>(half(data, record + 2));
        const auto sibling = static_cast<std::int16_t>(half(data, record + 4));
        if (child) pending.push_back({static_cast<unsigned>(static_cast<int>(visit.record) + child), parent, scale, key, frame});
        if (sibling) pending.push_back({static_cast<unsigned>(static_cast<int>(visit.record) + sibling), visit.parent, visit.scale, visit.key, visit.frame});
    }
}
