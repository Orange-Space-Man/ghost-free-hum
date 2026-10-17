#pragma once

#include <cstdint>

namespace jaudio {
    struct EffectParameters;
    struct EffectRecord {
        EffectRecord* previous = nullptr;
        EffectRecord* next = nullptr;
        EffectRecord** binding = nullptr;
        EffectParameters* parameters = nullptr;
        const void* actor = nullptr;
        std::uint32_t soundId = 0, metadata = 0, stopFrames = 0;
        std::uint8_t state = 0, priority = 0, lifetime = 0, pitchOffset = 0;
    };

    struct EffectPool {
        EffectRecord* free = nullptr;
        EffectRecord* active = nullptr;
        EffectRecord* acquire();
        void recycle(EffectRecord& record);
    };

    struct EffectAdmissionServices {
        virtual ~EffectAdmissionServices() = default;
        virtual void stop(EffectRecord& record) = 0;
        virtual void release(EffectRecord& record) = 0;
        virtual EffectParameters* acquireParameters() = 0;
        virtual void initialize(EffectRecord& record, EffectRecord** binding) = 0;
    };

    void admitEffect(EffectPool& pool, EffectRecord** binding, const void* actor, std::uint32_t soundId,
        std::uint8_t priority, std::uint8_t actorLimit, EffectAdmissionServices& services);
}
