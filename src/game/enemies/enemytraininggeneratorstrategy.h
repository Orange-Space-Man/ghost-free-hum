#pragma once

#include "game/enemies/enemystrategy.h"
#include "game/actors/charactergenerator.h"
#include "game/events/runtimeflags.h"
#include <functional>

namespace enemystrategy {
    struct TrainingServices {
        std::function<std::uint32_t()> random;
        std::function<bool()> showAllPortraits;
        std::function<void(unsigned)> portrait;
        std::function<void(unsigned)> createChild;
    };

    class TrainingGenerator final : public Strategy, public charactergenerator::Observer {
    public:
        explicit TrainingGenerator(gameflags::State& flags);
        void bindServices(TrainingServices services);
        void initializeBehavior() override;
        void behavior() override;
        void receive(charactergenerator::Event& event) override;

    private:
        charactergenerator::Generator& generator() const;
        gameflags::State& flags;
        std::uint8_t signal = 0x57;
        TrainingServices services;
        std::array<std::uint8_t, 32> available{};
    };
}
