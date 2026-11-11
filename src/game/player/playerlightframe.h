#pragma once

#include "game/player/playeractor.h"
#include "game/world/roomlightpool.h"
#include "game/player/playerfinishframe.h"

namespace playeractor {
    struct LightInput {
        bool enabled = false, scanner = false, illuminate = false, charged = false;
        std::array<float, 3> position{}, direction{};
        float angle = 0, distance = 0, scale = 1;
        std::uint32_t color = 0, frame = 0;
        gameplaycamera::CameraOwner& camera;
    };

    struct LightServices {
        roomlighting::Pool& lights;
        const playermodels::SharedModels& models;
        playerresources::LightPulse& pulse;
        std::function<void(unsigned, const std::array<float, 3>&)> sound;
        std::function<void(Player&, const playermodels::LightModel&)> illuminate;
    };

    class LightFrame {
    public:
        explicit LightFrame(const std::vector<std::uint8_t>& executable);
        void publishShadow(Player& player, roomlighting::Pool& lights, const FinishFrame& finish) const;
        void run(Player& player, const LightInput& input, const LightServices& services,
            const actormodel::InitialPose& pose) const;
        void update(Player& player, bool roomCleared, std::uint32_t frame, const FinishFrame& finish,
            const LightServices& services, const actormodel::InitialPose& pose) const;

    private:
        dooranimation::PoseTables trig;
        float half, angleUnits, degrees, downwardOffset, downwardScale, downwardLimit, coneRatio, epsilon;
        std::uint16_t brightness;
        std::array<std::uint8_t, 6> axes;
        float beamAngle, beamDistance;
        float shadowHeight, shadowDistance, alternateShadowScale, shadowVelocityScale;
        std::uint32_t shadowColor;
        std::uint16_t shadowReference, shadowBrightness;
        std::array<std::uint32_t, 2> colors;
        std::uint32_t angle(float value) const;
    };
}
