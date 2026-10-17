#pragma once

#include "jlib/jaudio/jaudioeffectcontrols.h"
#include "jlib/jaudio/jaudioeffectadmission.h"

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

    void updateEffectVoice(EffectRecord& record, unsigned listenerCount, EffectVoiceServices& services);
}
