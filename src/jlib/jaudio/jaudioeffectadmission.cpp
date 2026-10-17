#include "jlib/jaudio/jaudioeffectadmission.h"

#include <stdexcept>

std::uint32_t jaudio::EffectRecord::metadataFlags() const {
    if (!metadataSource) throw std::runtime_error("Effect record requires decoded sound metadata.");
    return metadataSource->flags;
}

std::uint8_t jaudio::EffectRecord::priority() const {
    if (!metadataSource) throw std::runtime_error("Effect record requires decoded sound metadata.");
    return metadataSource->priority;
}

jaudio::EffectRecord* jaudio::EffectPool::acquire() {
    if (!free) throw std::runtime_error("Effect pool requires its original reserved entry.");
    auto* record = free;
    if (!record->next) return nullptr;
    free = record->next;
    record->next = active;
    if (active) active->previous = record;
    record->previous = nullptr;
    active = record;
    record->parameters = nullptr;
    return record;
}

void jaudio::EffectPool::recycle(EffectRecord& record) {
    record.parameters = nullptr;
    record.binding = nullptr;
    if (active == &record) {
        active = record.next;
        if (record.next) record.next->previous = nullptr;
    } else {
        record.previous->next = record.next;
        if (record.next) record.next->previous = record.previous;
    }
    record.next = free;
    free->previous = &record;
    free = &record;
}

void jaudio::initializeEffectRequest(EffectRecord& record, EffectRecord** binding, const EffectRequest& request) {
    if (!request.metadata) throw std::runtime_error("Effect request requires decoded sound metadata.");
    record.soundId = request.soundId;
    record.actor = request.source ? request.source->actor : nullptr;
    record.position = record.actor ? request.source->position : nullptr;
    record.secondarySource = record.actor ? request.source->secondarySource : nullptr;
    record.context = record.actor ? request.source->context : 0;
    record.binding = binding;
    record.stopFrames = request.stopFrames;
    record.listener = request.listener;
    record.metadataSource = request.metadata;
    record.lifetime = 10;
    if (binding) *binding = &record;
}

void jaudio::admitEffect(EffectPool& pool, EffectRecord** binding, const EffectRequest& request,
    std::uint8_t actorLimit, EffectAdmissionServices& services) {
    if (!request.metadata) throw std::runtime_error("Effect request requires decoded sound metadata.");
    const auto* actor = request.source ? request.source->actor : nullptr;
    const auto soundId = request.soundId;
    const auto priority = request.metadata->priority;
    if (binding && *binding && (*binding)->soundId != soundId) services.stop(**binding);
    EffectRecord* weakest = nullptr;
    std::uint8_t count = 0;
    for (auto* record = pool.active; record;) {
        if (record->actor != actor) {
            record = record->next;
            continue;
        }
        if (record->soundId == soundId) {
            if (!(soundId & 0x800) && record->state == 5) {
                record->state = 4;
                return;
            }
            services.stop(*record);
            count = 255;
            break;
        }
        if (!weakest || record->priority() <= weakest->priority()) weakest = record;
        record = record->next;
        ++count;
    }
    if (count == actorLimit) {
        if (!weakest) throw std::runtime_error("Original effect admission has no replacement candidate.");
        if (priority < weakest->priority() || (priority == weakest->priority() && weakest->state == 5)) return;
        services.release(*weakest);
    }
    auto* record = pool.acquire();
    if (!record) {
        if (binding) *binding = nullptr;
        return;
    }
    record->parameters = services.acquireParameters();
    if (!record->parameters) {
        if (!binding) throw std::runtime_error("Original effect parameter exhaustion requires an output binding.");
        *binding = nullptr;
        return;
    }
    record->state = 1;
    initializeEffectRequest(*record, binding, request);
}
