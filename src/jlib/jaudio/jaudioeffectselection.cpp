#include "jlib/jaudio/jaudioeffectselection.h"

#include <stdexcept>
#include <cmath>

std::array<float, 3> jaudio::transformEffectPosition(const std::array<float, 12>& matrix, const std::array<float, 3>& position) {
    std::array<float, 3> result;
    for (unsigned row = 0; row < 3; ++row) {
        const auto offset = row * 4;
        const float first = std::fma(matrix[offset + 2], position[2], matrix[offset] * position[0]);
        const float second = matrix[offset + 3] + matrix[offset + 1] * position[1];
        result[row] = first + second;
    }
    return result;
}

jaudio::EffectRank jaudio::prepareEffectRank(std::vector<EffectListenerDistance>& listeners,
    const std::vector<std::array<float, 12>>& matrices, const std::array<float, 3>* position,
    unsigned selectedListener, unsigned listenerCount, std::uint8_t priority, std::uint32_t metadata, float maximumDistance) {
    const unsigned begin = selectedListener == 4 ? 0 : selectedListener;
    const unsigned end = selectedListener == 4 ? listenerCount : selectedListener + 1;
    if (begin >= end || end > listeners.size() || (position && end > matrices.size()))
        throw std::runtime_error("Invalid effect listener preparation range.");
    for (unsigned index = begin; index < end; ++index) {
        auto& listener = listeners[index];
        listener.previous = {listener.x, listener.y, listener.z};
        const auto relative = position ? transformEffectPosition(matrices[index], *position) : std::array<float, 3>{};
        listener.x = relative[0]; listener.y = relative[1]; listener.z = relative[2];
    }
    return rankEffect(listeners, selectedListener, listenerCount, priority, metadata, maximumDistance);
}

jaudio::EffectRank jaudio::rankEffect(std::vector<EffectListenerDistance>& listeners, unsigned selectedListener,
    unsigned listenerCount, std::uint8_t priority, std::uint32_t metadata, float maximumDistance) {
    constexpr float distanceLimit = 2147483648.0f;
    constexpr float overflowDistance = 1879048192.0f;
    const unsigned begin = selectedListener == 4 ? 0 : selectedListener;
    const unsigned end = selectedListener == 4 ? listenerCount : selectedListener + 1;
    if (begin >= end || end > listeners.size()) throw std::runtime_error("Invalid original effect listener range.");
    float nearest = distanceLimit;
    std::uint32_t score = 0;
    for (unsigned index = begin; index < end; ++index) {
        auto& listener = listeners[index];
        listener.squared = listener.z * listener.z + (listener.x * listener.x + listener.y * listener.y);
        if (listener.squared > distanceLimit) listener.squared = overflowDistance;
        if (!std::isfinite(listener.squared)) throw std::runtime_error("Unrecovered non-finite effect distance conversion.");
        const std::uint32_t inversePriority = 255 - priority;
        score = static_cast<std::uint32_t>(listener.squared) + inversePriority * inversePriority * 5776u;
        if (listener.z > 0) {
            const float depth = 6.0f * listener.z;
            if (depth >= 2147483648.0f) throw std::runtime_error("Unrecovered effect depth conversion overflow.");
            score += static_cast<std::uint32_t>(depth);
        }
        if (listener.squared < nearest) nearest = listener.squared;
    }
    if (selectedListener == 4) score /= listenerCount;
    const float range = metadata & 0x20 ? maximumDistance * maximumDistance : 10000000000.0f;
    return {score, nearest <= range};
}

void jaudio::updateEffectCategory(EffectPool& pool, std::vector<EffectRecord*>& channels, EffectCategoryServices& services) {
    EffectSelection selection(static_cast<unsigned>(channels.size()));
    for (auto* record = pool.active; record;) {
        auto* next = record->next;
        if (record->state == 1 && (record->soundId & 0xc00)) --record->lifetime;
        else if (record->state == 5 && !(record->soundId & 0xc00)) {
            services.release(*record);
            record = next;
            continue;
        }
        if (!record->lifetime) services.release(*record);
        else if (record->state) {
            const auto rank = services.prepareRank(*record);
            if (rank.eligible) selection.consider(*record, rank.score);
            else if (record->state == 5) {
                throw std::runtime_error("Unrecovered out-of-range state-five continuing effect lifecycle.");
            }
        }
        record = next;
    }
    selection.assign(channels, [&](EffectRecord& record) { services.release(record); });
}

jaudio::EffectSelection::EffectSelection(unsigned channelCount) : candidates(channelCount) {
    if (channelCount > 255) throw std::runtime_error("Effect category exceeds the original byte-sized channel count.");
}

void jaudio::EffectSelection::consider(EffectRecord& record, std::uint32_t score) {
    for (unsigned index = 0; index < candidates.size(); ++index) {
        auto& candidate = candidates[index];
        if (score > candidate.score || record.state > candidate.state) continue;
        if (count < candidates.size()) ++count;
        for (unsigned tail = static_cast<unsigned>(candidates.size()) - 1; tail > index; --tail) {
            candidates[tail].score = candidates[tail - 1].score;
            candidates[tail].record = candidates[tail - 1].record;
            candidate.state = candidates[tail - 1].state;
        }
        candidate.score = score;
        candidate.record = &record;
        candidate.state = record.state;
        return;
    }
}

void jaudio::EffectSelection::assign(std::vector<EffectRecord*>& channels, const std::function<void(EffectRecord&)>& release) {
    if (channels.size() != candidates.size()) throw std::runtime_error("Effect channel count differs from its category selection capacity.");
    for (unsigned index = 0; index < count; ++index) {
        auto& state = candidates[index].record->state;
        if (state == 1) state = 2;
        else if (state == 4) state = 3;
    }
    for (auto& channel : channels) {
        bool vacant = !channel;
        if (channel) {
            if (channel->state == 4) {
                if (!(channel->soundId & 0xc00)) {
                    channel->state = 1;
                    channel->lifetime = 0;
                } else release(*channel);
                vacant = true;
            } else if (channel->state == 0) {
                channel = nullptr;
                vacant = true;
            } else {
                for (auto& candidate : candidates) {
                    if (candidate.record == channel) { candidate.record = nullptr; break; }
                }
            }
        }
        if (!vacant) continue;
        bool available = true, assigned = false;
        for (auto& candidate : candidates) {
            if (!candidate.record || candidate.record->state == 3) continue;
            for (auto* current : channels) {
                if (current && current == candidate.record) { available = false; break; }
            }
            if (available) {
                channel = candidate.record;
                candidate.record = nullptr;
                assigned = true;
                break;
            }
        }
        if (!assigned) channel = nullptr;
    }
}
