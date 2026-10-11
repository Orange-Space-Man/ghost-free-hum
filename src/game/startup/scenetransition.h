#pragma once

namespace scenetransition {
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
