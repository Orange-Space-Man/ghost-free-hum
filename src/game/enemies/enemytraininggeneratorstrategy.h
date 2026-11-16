#pragma once

#include "game/enemies/enemystrategy.h"
#include "game/actors/charactergenerator.h"
#include "game/events/runtimeflags.h"

namespace enemystrategy {
    class TrainingGenerator final : public Strategy, public charactergenerator::Observer {
    public:
        explicit TrainingGenerator(gameflags::State& flags);
        void initializeBehavior() override;
        void behavior() override;
        void receive(charactergenerator::Event& event) override;

    private:
        charactergenerator::Generator& generator() const;
        gameflags::State& flags;
        std::uint8_t signal = 0x57;
    };
}
