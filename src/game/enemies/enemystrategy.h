#pragma once

#include <cstdint>

namespace enemies {
    class Owner;
}

namespace enemystrategy {
    class Strategy {
    public:
        virtual ~Strategy() = default;
        virtual void begin();
        virtual void update();
        virtual bool query(enemies::Owner& owner);
        virtual void finish();
        virtual void behavior();
        virtual void initializeBehavior();
        void setNextState(std::uint16_t value);
        void changeState();

        enemies::Owner* owner = nullptr;
        void* context = nullptr;
        std::uint16_t nextState = 0;
        std::uint16_t state = 0;
        std::uint32_t frames = 0;
    };
}
