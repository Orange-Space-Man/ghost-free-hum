#pragma once

#include "game/enemies/enemystrategy.h"
#include "game/actors/characterevent.h"

namespace enemystrategy {
    class FakeDoor final : public Strategy, public characterevent::Observer {
    public:
        explicit FakeDoor(const std::array<std::uint8_t, 4>& ambient);
        void behavior() override;
        std::uint32_t sprayedWithFire(characterevent::Event& event) override;

    private:
        const std::array<std::uint8_t, 4>& ambient;
    };
}
