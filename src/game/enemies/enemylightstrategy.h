#pragma once

#include "game/actors/actorregistry.h"
#include "game/actors/characterevent.h"
#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"
#include "game/enemies/enemystrategy.h"
#include "game/world/dooranimationpose.h"
#include <array>
#include <functional>

namespace enemystrategy {
    void beginLightEffect(characterowner::Owner& owner, const actors::Registry& registry);

    struct LightServices {
        actors::MoveActor& actor;
        characterowner::State& character;
        characterownership::Ownership& ownership;
        const dooranimation::PoseTables& trigonometry;
        std::function<std::uint32_t(unsigned)> bindFrame;
        std::function<float(unsigned)> minBindRange;
        std::function<float(unsigned)> maxBindRange;
        std::function<std::array<float, 3>()> playerPosition;
        std::function<bool()> scenarioMatches;
        std::function<bool()> reducedScale;
        std::function<std::uint32_t(characterevent::Event&)> event;
        std::function<void()> beginEffect;
        std::function<void(unsigned, unsigned, std::uint32_t)> startEffect;
        std::function<void(void*&)> releaseReference;
        std::function<void(std::array<float, 3>&)> projectFloor;
        std::function<void(unsigned, const std::array<float, 3>&)> particle;
        std::function<void(unsigned, const std::array<float, 3>&)> sound;
    };

    class Light final : public Strategy {
    public:
        explicit Light(LightServices services);
        void initializeBehavior() override;
        void behavior() override;
        std::uint32_t hitThreshold() const;

        std::int32_t litCount = 0;

    private:
        LightServices services;
        void updateSpin();
        void releaseOwnerReference();
    };
}
