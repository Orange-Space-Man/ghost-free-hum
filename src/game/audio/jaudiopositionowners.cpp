#include "game/audio/jaudiopositionowners.h"

#include <stdexcept>

jaudio::RetainedPosition* jaudio::PositionOwners::allocate() {
    auto* point = free;
    if (!point) return nullptr;
    free = point->next;
    point->next = active;
    if (active) active->previous = point;
    point->previous = nullptr;
    active = point;
    point->remaining = 600;
    return point;
}

void jaudio::PositionOwners::recycle(RetainedPosition& point) {
    if (!free) throw std::runtime_error("Original retained-position release dereferences an empty free list.");
    if (active == &point) {
        active = point.next;
        if (point.next) point.next->previous = nullptr;
    } else {
        point.previous->next = point.next;
        if (point.next) point.next->previous = point.previous;
    }
    point.next = free;
    free->previous = &point;
    free = &point;
}

void jaudio::PositionOwners::release(const std::array<float, 3>* position) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    RetainedPosition* retained = nullptr;
    for (auto* first : categories) {
        auto* sound = first;
        while (sound) {
            auto* next = sound->next;
            if (sound->ownerPosition == position) {
                const bool persistent = (sound->metadataFlags() & 0x8000) != 0;
                if (persistent && !retained) retained = allocate();
                if (persistent && retained) {
                    if (!sound->position) throw std::runtime_error("Original positional sound has no position.");
                    retained->position = *sound->position;
                    retained->sound = sound;
                    sound->clearBinding();
                    sound->binding = &retained->sound;
                    sound->retainPosition(retained->position);
                } else sound->stop(0);
            }
            sound = next;
        }
    }
}

void jaudio::PositionOwners::update() {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    auto* point = active;
    while (point) {
        auto* next = point->next;
        --point->remaining;
        if (!point->remaining || !point->sound) {
            if (point->sound) point->sound->stop(0);
            recycle(*point);
        }
        point = next;
    }
}
