#pragma once

namespace gameframepacing {
    inline constexpr unsigned presentationFramesPerSecond = 30;
    inline constexpr unsigned hostTicksPerSecond = 60;

    class Clock {
    public:
        bool ready(double seconds);

    private:
        double next = 0;
        bool started = false;
    };
}
