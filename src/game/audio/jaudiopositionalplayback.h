#pragma once

#include "game/audio/jaudiopositionowners.h"
#include "jlib/jaudio/jaudioplayback.h"

#include <memory>

namespace gameplaymusic { class Owner; }

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
        PositionalSound* playGlobal(unsigned id, PositionalSound** binding = nullptr);
        void setPlayerContext(std::uint32_t context);
        void setPort(PositionalSound& sound, unsigned port, std::uint16_t value);
        float interactionPitch(PositionalSound& sound);
        void setInteractionPitch(PositionalSound& sound, float value);
        void setInteractionVolume(PositionalSound& sound, float value);
        void updateFurnitureSwing(unsigned id, const std::array<float, 3>& position,
            const std::array<float, 2>& velocity, PositionalSound*& binding);
        void playDeathEffects(const std::array<float, 3>& position);
        actoranimation::SoundServices animationServices(const std::array<float, 3>& position);
        actoranimation::SoundServices animationServices();
        void update(const std::array<float, 12>& listenerMatrix);
        void bindMusic(gameplaymusic::Owner& owner);
        bool mix(DspMixer& mixer, std::string& error);
        void stop();
        bool finished() const;
        SpatialParameters spatial(const std::array<float, 3>& relative, std::uint32_t flags) const;

    private:
        struct Handle;
        struct Runtime;
        Handle& owned(actoranimation::SoundHandle& sound);
        void remove(Handle& handle);
        PositionalSound* playAt(unsigned id, const std::array<float, 3>* position, PositionalSound** binding);
        actoranimation::SoundServices animationServicesAt(const std::array<float, 3>* position);
        PositionOwners& owners;
        gameplaymusic::Owner* music = nullptr;
        std::unique_ptr<Runtime> runtime;
        std::vector<std::uint32_t> soundFlags;
        std::vector<std::unique_ptr<Handle>> handles;
        std::array<float, 12> listener{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        float maximumDistance, fullDistance, minimumVolume;
    };
}
