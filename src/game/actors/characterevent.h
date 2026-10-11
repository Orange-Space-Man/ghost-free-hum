#pragma once

#include <array>
#include <cstdint>

namespace characterevent {
    struct Event {
        explicit Event(unsigned message);
        unsigned message;
        std::array<std::uint32_t, 6> fields{};
        std::array<std::uint8_t, 381>* types = nullptr;
    };

    class Observer {
    public:
        virtual ~Observer() = default;
        virtual std::uint32_t receive(Event& event);
        virtual std::uint32_t enteredFlashlight(Event& event);
        virtual std::uint32_t captureBegin(Event& event);
        virtual std::uint32_t captureEnd(Event& event);
        virtual std::uint32_t replacement(Event& event);
        virtual std::uint32_t collideWithPlayer(Event& event);
        virtual std::uint32_t sprayedWithFire(Event& event);
        virtual std::uint32_t sprayedWithIce(Event& event);
        virtual std::uint32_t sprayedWithWater(Event& event);
        virtual std::uint32_t hitWithFireBall(Event& event);
        virtual std::uint32_t hitWithIceBall(Event& event);
        virtual std::uint32_t hitWithWaterBall(Event& event);
        virtual std::uint32_t fishingBegin(Event& event);
        virtual std::uint32_t playerLeftRoom(Event& event);
        virtual std::uint32_t playerEnteredRoom(Event& event);
        virtual std::uint32_t playerTapped(Event& event);
        virtual std::uint32_t resources(Event& event);
    };
}
