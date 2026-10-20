#pragma once

#include "game/audio/jaudiopositionowners.h"
#include "jlib/jaudio/jaudioplayback.h"

#include <memory>

namespace jaudio {
    class PositionalPlayback {
    public:
        PositionalPlayback(PositionOwners& owners, const std::vector<std::uint8_t>& executable,
            gameassets::Package& package, const Resources& resources,
            std::function<std::uint32_t(const std::array<float, 3>&, bool)> sourceContext = {});
        ~PositionalPlayback();
        PositionalPlayback(const PositionalPlayback&) = delete;
        PositionalPlayback& operator=(const PositionalPlayback&) = delete;
        PositionalSound* play(unsigned id, const std::array<float, 3>& position, PositionalSound** binding = nullptr);
        void setPort(PositionalSound& sound, unsigned port, std::uint16_t value);
        void playDeathEffects(const std::array<float, 3>& position);
        actoranimation::SoundServices animationServices(const std::array<float, 3>& position);
        void update(const std::array<float, 12>& listenerMatrix);
        bool mix(DspMixer& mixer, std::string& error);
        void stop();
        bool finished() const;
        SpatialParameters spatial(const std::array<float, 3>& relative, std::uint32_t flags) const;

    private:
        struct Handle;
        struct Runtime;
        Handle& owned(actoranimation::SoundHandle& sound);
        void remove(Handle& handle);
        PositionOwners& owners;
        std::unique_ptr<Runtime> runtime;
        std::vector<std::uint32_t> soundFlags;
        std::vector<std::unique_ptr<Handle>> handles;
        std::array<float, 12> listener{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        float maximumDistance, fullDistance, minimumVolume;
    };
}
