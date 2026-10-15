#pragma once

#include "jlib/jaudio/jaudioeffectcontrols.h"

namespace jaudio {
    struct EffectVoiceServices {
        virtual ~EffectVoiceServices() = default;
        virtual std::uint16_t readPort(unsigned port) = 0;
        virtual void writePort(unsigned port, std::uint16_t value) = 0;
        virtual void publishPort(unsigned port, std::uint16_t value) = 0;
        virtual std::uint32_t random() = 0;
        virtual void initializePosition() = 0;
        virtual void updateParameters(EffectControls& controls, unsigned state, std::uint8_t pitchOffset) = 0;
        virtual void initializeParameters(EffectControls& controls) = 0;
        virtual void publishParameters(EffectControls& controls, unsigned state) = 0;
        virtual unsigned variant() = 0;
        virtual unsigned distanceVariant() = 0;
        virtual unsigned roomSize() = 0;
        virtual void release() = 0;
    };

    struct EffectVoice {
        EffectControls controls;
        std::uint32_t soundId = 0, metadata = 0, stopFrames = 0;
        unsigned state = 2, listenerCount = 1;
        std::uint8_t pitchOffset = 0;
        std::array<std::uint16_t, 16> ports{};
        std::uint16_t pendingPorts = 0;

        void update(EffectVoiceServices& services);
    };
}
