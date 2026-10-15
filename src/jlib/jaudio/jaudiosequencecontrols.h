#pragma once

#include <array>
#include <cstdint>

namespace jaudio {
    enum class SequenceParameter { volume, pan, pitch, fxmix, dolby };

    float blendTrackParameter(float local, float inherited, float weight, unsigned mode);

    struct SequenceVolume {
        float target = 1, value = 1, decrement = 0;
        std::uint32_t remaining = 0;

        void set(float volume, std::uint32_t frames);
        void update();
    };

    struct SequenceTrackControl {
        SequenceVolume volume;
        SequenceVolume pitch;
        SequenceVolume pan{0.5f, 0.5f};
        SequenceVolume fxmix{0, 0};
        SequenceVolume dolby{0, 0};
        bool requestedMute = false, muted = false;
    };

    class SequenceControls {
    public:
        void setVolume(unsigned track, float volume, std::uint32_t frames);
        void setPitch(unsigned track, float pitch, std::uint32_t frames);
        void setPan(unsigned track, float pan, std::uint32_t frames);
        void setFxmix(unsigned track, float fxmix, std::uint32_t frames);
        void setDolby(unsigned track, float dolby, std::uint32_t frames, unsigned outputMode);
        float value(unsigned track, SequenceParameter parameter, unsigned soundState) const;
        void setMuted(unsigned track, bool muted);
        void update();
        const SequenceTrackControl& track(unsigned index) const;

    private:
        std::array<SequenceTrackControl, 32> tracks{};
    };
}
