#pragma once

#include <array>
#include <cstdint>

namespace jaudio {
    struct EffectParameters;
    struct EffectMetadata {
        std::uint32_t flags = 0;
        std::uint8_t priority = 0;
    };

    struct EffectActorSource {
        const void* actor = nullptr;
        const std::array<float, 3>* position = nullptr;
        const void* secondarySource = nullptr;
        std::uint32_t context = 0;
    };

    struct EffectRequest {
        std::uint32_t soundId = 0, stopFrames = 0;
        std::uint8_t listener = 0;
        const EffectActorSource* source = nullptr;
        const EffectMetadata* metadata = nullptr;
    };

    struct EffectRecord {
        EffectRecord* previous = nullptr;
        EffectRecord* next = nullptr;
        EffectRecord** binding = nullptr;
        EffectParameters* parameters = nullptr;
        const void* actor = nullptr;
        const std::array<float, 3>* position = nullptr;
        const void* secondarySource = nullptr;
        const EffectMetadata* metadataSource = nullptr;
        std::uint32_t context = 0;
        std::uint8_t listener = 0;
        std::uint32_t soundId = 0, stopFrames = 0;
        std::uint8_t state = 0, lifetime = 0, pitchOffset = 0, channel = 0;
        std::uint32_t metadataFlags() const;
        std::uint8_t priority() const;
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
    };

    void initializeEffectRequest(EffectRecord& record, EffectRecord** binding, const EffectRequest& request);
    void admitEffect(EffectPool& pool, EffectRecord** binding, const EffectRequest& request,
        std::uint8_t actorLimit, EffectAdmissionServices& services);
}
