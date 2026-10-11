#pragma once

#include "game/audio/jaudiopositionowners.h"
#include "jlib/jaudio/jaudioplayback.h"

#include <memory>

namespace jaudio {
    class PositionalPlayback {
    public:
        PositionalPlayback(PositionOwners& owners, const std::vector<std::uint8_t>& executable,
            gameassets::Package& package, const Resources& resources);
        ~PositionalPlayback();
        PositionalPlayback(const PositionalPlayback&) = delete;
        PositionalPlayback& operator=(const PositionalPlayback&) = delete;
        PositionalSound* play(unsigned id, const std::array<float, 3>& position, PositionalSound** binding = nullptr);
        actoranimation::SoundServices animationServices(const std::array<float, 3>& position);
        void update(const std::array<float, 12>& listenerMatrix);
        bool mix(DspMixer& mixer, std::string& error);
        void stop();
        bool finished() const;
        SpatialParameters spatial(const std::array<float, 3>& relative, std::uint32_t flags) const;

    private:
        struct Handle;
        Handle& owned(actoranimation::SoundHandle& sound);
        void remove(Handle& handle);
        PositionOwners& owners;
        std::array<Playback, 4> prepared;
        std::array<std::uint32_t, 4> soundFlags{};
        std::vector<std::unique_ptr<Handle>> handles;
        std::array<float, 12> listener{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        float maximumDistance, fullDistance, minimumVolume;
    };
}
