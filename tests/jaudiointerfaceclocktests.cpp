#include "jlib/jaudio/jaudiointerfaceclock.h"

#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}

int main() {
    try {
        jaudio::InterfaceClock clock;
        unsigned updates = 0;
        const auto work = [&] {
            require(clock.frame() == updates, "The interface counter advances after its work.");
            ++updates;
        };
        require(!clock.update(work) && clock.frame() == 0, "No DAC activity must not advance the interface.");
        clock.mixed();
        clock.mixed();
        clock.mixed();
        require(clock.update(work) && clock.frame() == 1 && updates == 1,
            "Several generated buffers must coalesce into one interface update.");
        require(!clock.update(work), "Repeated game updates without a generated buffer must not advance audio.");
        clock.mixed();
        require(clock.update([&] { work(); clock.mixed(); }), "New DAC activity admits interface work.");
        require(!clock.update(work) && clock.frame() == 2,
            "The original gate snapshots DAC activity after work, including changes during that work.");
        clock.mixed();
        bool failed = false;
        try { clock.update([] { throw std::runtime_error("test failure"); }); }
        catch (const std::runtime_error&) { failed = true; }
        require(failed && clock.frame() == 2 && clock.update(work), "Failed work must not publish a completed interface frame.");
        clock.reset();
        require(clock.frame() == 0 && !clock.update({}), "Fresh boot resets both clock domains.");
        std::cout << "Audio interface gating and retained frame checks passed.\n";
        return 0;
    } catch (const std::exception& exception) { std::cerr << exception.what() << '\n'; return 1; }
}
