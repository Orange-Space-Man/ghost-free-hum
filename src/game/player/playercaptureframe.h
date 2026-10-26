#pragma once

#include "game/player/playeractor.h"
#include "game/player/playereventdispatch.h"
#include "game/effects/particleeffectfactory.h"

namespace playeractor {
    class CaptureFrame {
    public:
        explicit CaptureFrame(const std::vector<std::uint8_t>& executable);
        bool begin(Player& player, const std::array<float, 3>& position, std::int32_t recipient,
            actorcapture::Motion& motion, const playerevents::CollectionSound& sound) const;
        bool release(Player& player, const std::array<float, 3>* position, const std::array<float, 3>* direction,
            bool launched, const actormodel::InitialPose& pose) const;
        void run(Player& player, const playerevents::Dispatcher& events, particles::EffectFactory& effects) const;
        void updateOpacity(Player& player) const;

    private:
        std::int32_t concealFrames;
        float launchSpeed, directionEpsilon;
    };
}
