#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include "game/actors/actormodelarchive.h"

namespace actoranimation {
    class SoundHandle {
    public:
        virtual ~SoundHandle() = default;
        virtual void stop(unsigned frames) = 0;
        virtual std::uint32_t flags() const = 0;
    };

    struct SoundSlot {
        std::uint8_t state = 0;
        SoundHandle* sound = nullptr;
        const std::uint8_t* event;
        actormodel::ResourceRef resource;
    };

    struct SoundServices {
        std::function<void(unsigned, SoundHandle*&)> start;
        std::function<void(SoundHandle&, float)> pitch, volume, pan;
    };

    struct SoundState {
        SoundState();
        void bind(actormodel::ResourceRef value, int playbackDirection, double startFrame);
        void update(double currentFrame, double speed, const SoundServices& services);
        void stop();
        std::array<SoundSlot, 8> slots;
        std::uint32_t field64 = 0, field68 = 0;
        std::int32_t direction;
        std::uint32_t forwardIndex, reverseIndex;
        std::int32_t field78;
        float frame;
        std::uint32_t field80;
        actormodel::ResourceRef resource;
    };
}
