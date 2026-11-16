#pragma once

#include "game/enemies/enemycapturecollision.h"
#include "game/enemies/enemycapturelifecycle.h"
#include "game/enemies/enemycapturesteering.h"
#include "game/enemies/enemyghostmovement.h"
#include "game/enemies/enemytypeparameters.h"
#include "game/scene/gamearchive.h"

namespace enemystrategy {
    struct CaptureServices {
        actors::MoveActor& actor;
        characterowner::State& character;
        const mapcollision::State& map;
        const actormodel::InitialPose& pose;
        const enemytypes::Manager& parameters;
        ghostcapture::Collision& collision;
        ghostcapture::Bookkeeping& bookkeeping;
        std::function<std::uint32_t()> random, frame;
        std::function<ghostcapture::Vector()> nearestPlayerDelta;
        std::function<const ghostcapture::Vector*()> captureSource;
        std::function<void(void*&)> releaseReference;
    };

    class Capture final : public Strategy {
    public:
        Capture(CaptureServices services, const gamearchive::Archive& common);
        void update() override;
        void initializeBehavior() override;
        void behavior() override;
        ghostcapture::Motion motion;
        ghostcapture::Steering steering;
        float escapeHeight = 180;
        bool blocked = false;

    private:
        CaptureServices services;
        gameparameters::Parameters parameters;
        std::uint16_t baseYaw = 0;
        bool unlinked();
        void animation(const char* name, unsigned duration);
        void initializeEscape();
        void faceForce();
        ghostcapture::SteeringInput steeringInput() const;
    };
}
