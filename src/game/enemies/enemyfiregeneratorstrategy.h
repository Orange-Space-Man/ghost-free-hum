#pragma once

#include "game/actors/charactergenerator.h"
#include "game/effects/particleeffectfactory.h"
#include "game/effects/particleglowpool.h"
#include "game/player/playeractorview.h"

namespace enemystrategy {
    struct FireGeneratorEnvironment {
        particles::ArchiveResources& common;
        particles::EmitterPool& particles;
        particles::GlowPool& glows;
        const playeractor::View& player;
        const roomlocation::Rooms& rooms;
        enemies::Runtime& runtime;
    };

    class FireGenerator final : public Strategy {
    public:
        explicit FireGenerator(FireGeneratorEnvironment& environment);
        ~FireGenerator() override;
        void initializeBehavior() override;
        void behavior() override;
        std::array<float, 3> position{};
        float upperHeight = 0, lowerHeight = 0, radius = 0;
        std::uint16_t angle = 0;
        std::uint32_t attractionFrames = 0, appearTime = 0, disabledFrames = 0;

    private:
        charactergenerator::Generator& generator() const;
        void createCandle();
        void createGlow();
        FireGeneratorEnvironment& environment;
        particles::Emitter* candle = nullptr;
        particles::Glow* glow = nullptr;
        bool requiresVacuum = false, changed = false, disabled = false;
    };
}
