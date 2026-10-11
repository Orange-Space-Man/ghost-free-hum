#include "platform/windows/gameframepacing.h"

#include <iostream>
#include <stdexcept>

int main() {
    try {
        for (unsigned pass = 0; pass < 2; ++pass) {
            gameframepacing::Clock clock;
            unsigned frames = 0;
            for (unsigned tick = 0; tick < 2400; ++tick) frames += clock.ready(tick / 240.0 + 0.000001);
            if (frames != 300U) throw std::runtime_error("Incorrect presentation rate.");
            if (!clock.ready(20) || clock.ready(20)) throw std::runtime_error("Presentation catch-up burst.");
        }
        std::cout << "Frame pacing checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
