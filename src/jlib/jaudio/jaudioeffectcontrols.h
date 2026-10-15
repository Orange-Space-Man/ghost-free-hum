#pragma once

#include "jlib/jaudio/jaudioeffectparameter.h"

#include <array>

namespace jaudio {
    enum class EffectControl { volume, pan, pitch, fxmix, auxiliary, dolby };

    struct EffectSpatialState {
        std::uint32_t flags = 0;
        unsigned soundState = 2, listenerCount = 1;
        std::uint8_t pitchOffset = 0;
        float distance = 0, maximumDistance = 1, relativeDepth = 0;
        std::uint16_t defaultFxmix = 0;
        std::uint8_t distantFxmix = 80;
    };

    struct EffectSpatialServices {
        virtual ~EffectSpatialServices() = default;
        virtual float distanceVolume(unsigned curve) = 0;
        virtual float pan() = 0;
        virtual float dopplerPitch() = 0;
        virtual float roomFxmix() = 0;
        virtual std::uint32_t random() = 0;
    };

    class EffectControls {
    public:
        EffectControls();
        void reset();
        void set(EffectControl control, unsigned slot, float value, std::uint32_t frames);
        void bind(EffectControl control, const float* value);
        void advance(bool paused);
        void updateSpatial(const EffectSpatialState& state, EffectSpatialServices& services);
        float value(EffectControl control, unsigned slot) const;
        float combined(EffectControl control, float categoryVolume = 1);
        const EffectParameter& parameter(EffectControl control, unsigned slot) const;

    private:
        std::array<std::array<EffectParameter, 8>, 6> banks;
        std::array<const float*, 6> bindings{};
    };
}
