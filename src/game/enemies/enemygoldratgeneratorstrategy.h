#pragma once

#include "game/actors/charactergenerator.h"

namespace enemystrategy {
    class GoldRatGenerator final : public Strategy, public charactergenerator::Observer {
    public:
        explicit GoldRatGenerator(enemies::Runtime& runtime);
        void behavior() override;
        void receive(charactergenerator::Event& event) override;

    private:
        enemies::Runtime& runtime;
        std::int32_t previousPoint = 0;
    };
}
