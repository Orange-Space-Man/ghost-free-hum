#include "jlib/jaudio/jaudioeffectparameterpool.h"

#include <stdexcept>

jaudio::EffectParameterPool::EffectParameterPool(unsigned count) : entries(count) {
    for (unsigned index = 0; index < count; ++index) {
        entries[index].previous = index ? &entries[index - 1] : nullptr;
        entries[index].next = index + 1 < count ? &entries[index + 1] : nullptr;
    }
    if (count) free = entries.data();
}

jaudio::EffectParameters* jaudio::EffectParameterPool::acquire() {
    auto* parameters = free;
    if (!parameters) return nullptr;
    free = parameters->next;
    parameters->next = active;
    if (active) active->previous = parameters;
    parameters->previous = nullptr;
    active = parameters;
    parameters->controls.reset();
    parameters->pendingPorts = 0;
    return parameters;
}

void jaudio::EffectParameterPool::release(EffectParameters& parameters) {
    if (!free) throw std::runtime_error("Original effect parameter release dereferences an exhausted free list.");
    auto* found = active;
    while (found && found != &parameters) found = found->next;
    if (!found) throw std::runtime_error("Effect parameter release requires an active block owned by this pool.");
    if (active == &parameters) {
        active = parameters.next;
        if (parameters.next) parameters.next->previous = nullptr;
    } else {
        parameters.previous->next = parameters.next;
        if (parameters.next) parameters.next->previous = parameters.previous;
    }
    parameters.next = free;
    free->previous = &parameters;
    free = &parameters;
}
