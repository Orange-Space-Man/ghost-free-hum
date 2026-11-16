#pragma once

#include <array>
#include <cstdint>

namespace scenetransition {
    struct Request {
        std::int32_t destination = -1;
        std::int32_t kind = 0;
        std::int32_t duration = 0;
    };

    class ParlorWipe {
    public:
        void beginOutgoing(const std::array<std::uint8_t, 3>& retainedColor = {0, 0, 0});
        void beginIncoming();
        bool update(unsigned viTicks);
        unsigned alpha() const;
        bool active() const;
        bool drawsCapturedFrame() const;
        std::array<std::uint8_t, 4> overlay() const;

    private:
        enum class Phase { Idle, Outgoing, Covered, Incoming };
        Phase phase = Phase::Idle;
        std::array<std::uint8_t, 3> color{};
        unsigned ticks = 0;
        unsigned delay = 0;
    };

    class OutgoingFade {
    public:
        void reset();
        void begin();
        void update(unsigned viTicks = 1);
        bool active() const;
        bool finished() const;
        unsigned alpha() const;

    private:
        unsigned ticks = 0;
        bool started = false;
    };
}
