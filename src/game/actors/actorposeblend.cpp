#include "game/actors/actorposeblend.h"
#include "game/actors/actoreffectstate.h"
#include "game/world/dooranimationpose.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    using Vector = std::array<float, 3>;

    unsigned half(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (unsigned(data.at(offset)) << 8) | data.at(offset + 1);
    }

    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (half(data, offset) << 16) | half(data, offset + 2);
    }

    float number(const std::vector<std::uint8_t>& data, std::size_t offset) {
        const auto bits = word(data, offset);
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }

    float length(const Vector& value) {
        const float squared = (value[2] * value[2] + value[0] * value[0]) + value[1] * value[1];
        return squared > 0 ? std::sqrt(squared) : squared;
    }

    Vector column(const actormodel::Matrix& matrix, unsigned index) {
        return {matrix[index], matrix[index + 4], matrix[index + 8]};
    }

    void store(actormodel::Matrix& matrix, unsigned index, const Vector& value, float scale) {
        for (unsigned i = 0; i < 3; ++i) matrix[index + i * 4] = value[i] * scale;
    }

    Vector crossDifference(const Vector& first, const Vector& second) {
        return {first[1] * (second[2] - first[2]) - first[2] * (second[1] - first[1]),
            first[2] * (second[0] - first[0]) - first[0] * (second[2] - first[2]),
            first[0] * (second[1] - first[1]) - first[1] * (second[0] - first[0])};
    }
}

void actormodel::blendTransform(double weight, const Matrix& current, const Matrix& previous, Matrix& output) {
    const auto blendColumn = [&](unsigned index, float& scale) {
        auto first = column(current, index), second = column(previous, index);
        const float firstLength = length(first), secondLength = length(second);
        const float firstFactor = 1.0f / firstLength, secondFactor = 1.0f / secondLength;
        for (unsigned i = 0; i < 3; ++i) {
            first[i] *= firstFactor;
            second[i] *= secondFactor;
            first[i] += static_cast<float>(weight * double(second[i] - first[i]));
        }
        scale = firstLength + static_cast<float>(weight * double(secondLength - firstLength));
        return first;
    };
    float scale;
    auto up = blendColumn(1, scale);
    float magnitude = length(up);
    if (magnitude == 0) return;
    for (auto& value : up) value *= 1.0f / magnitude;
    store(output, 1, up, scale);
    auto right = blendColumn(0, scale);
    magnitude = length(right);
    if (magnitude == 0) return;
    for (auto& value : right) value *= 1.0f / magnitude;
    store(output, 0, right, scale);
    const auto forward = blendColumn(2, scale);
    if (length(forward) == 0) return;
    const auto perpendicular = crossDifference(up, forward);
    if (length(perpendicular) == 0) return;
    auto corrected = crossDifference(perpendicular, up);
    magnitude = length(corrected);
    if (magnitude == 0) return;
    for (auto& value : corrected) value *= 1.0f / magnitude;
    store(output, 2, corrected, scale);
    for (unsigned i : {3u, 7u, 11u}) output[i] = current[i] + static_cast<float>(weight * double(previous[i] - current[i]));
}

void actormodel::blendPose(Instance& instance) {
    if (!instance.model) return;
    const auto& data = instance.model->bytes;
    const auto hierarchy = word(data, 48);
    const auto count = half(data, 8);
    struct Visit { unsigned record; float weight; };
    std::vector<Visit> pending{{0, instance.blend}};
    std::vector<bool> seen(count);
    while (!pending.empty()) {
        const auto visit = pending.back();
        pending.pop_back();
        if (visit.record >= count || seen.at(visit.record)) throw std::runtime_error("Invalid actor blend hierarchy.");
        seen[visit.record] = true;
        const auto record = std::size_t(hierarchy) + std::size_t(visit.record) * 16;
        const auto index = half(data, record);
        float weight = visit.weight;
        if (auto* effect = static_cast<actoreffects::State*>(instance.attachment(index).object)) {
            const float overrideWeight = effect->field76 ? float(effect->field74) / float(effect->field76) : 0;
            if (overrideWeight > 0 || (effect->flags & 1)) {
                weight = overrideWeight;
                if (effect->field74) --effect->field74;
            }
        }
        auto& current = instance.matrix(0, index);
        auto& previous = instance.matrix(3, index);
        if (weight > 0) {
            auto output = current;
            blendTransform(weight, current, previous, output);
            current = output;
        }
        previous = current;
        const auto child = static_cast<std::int16_t>(half(data, record + 2));
        const auto sibling = static_cast<std::int16_t>(half(data, record + 4));
        if (child) pending.push_back({static_cast<unsigned>(static_cast<int>(visit.record) + child), weight});
        if (sibling) pending.push_back({static_cast<unsigned>(static_cast<int>(visit.record) + sibling), visit.weight});
    }
}

void actormodel::updateEnvelopes(Instance& instance) {
    if (!instance.model) return;
    const auto& data = instance.model->bytes;
    const auto joints = half(data, 8), envelopes = half(data, 12);
    const auto matrices = word(data, 56), weights = word(data, 60), indices = word(data, 64), counts = word(data, 68);
    std::size_t cursor = 0;
    for (unsigned envelope = 0; envelope < envelopes; ++envelope) {
        auto& destination = instance.matrix(0, joints + envelope);
        destination.fill(0);
        for (unsigned influence = 0; influence < data.at(std::size_t(counts) + envelope); ++influence, ++cursor) {
            const auto joint = half(data, std::size_t(indices) + cursor * 2);
            if (joint >= joints) throw std::runtime_error("Actor envelope joint exceeds its model.");
            Matrix inverse, scaled;
            const float weight = number(data, std::size_t(weights) + cursor * 4);
            for (unsigned component = 0; component < 12; ++component) {
                inverse[component] = number(data, std::size_t(matrices) + joint * 48 + component * 4);
                scaled[component] = instance.matrix(0, joint)[component] * weight;
            }
            const auto combined = dooranimation::concatenate(scaled, inverse);
            for (unsigned component = 0; component < 12; ++component) destination[component] += combined[component];
        }
    }
}
