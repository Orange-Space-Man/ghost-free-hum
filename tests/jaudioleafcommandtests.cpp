#include "jlib/jaudio/jaudiosequence.h"

#include <iostream>
#include <stdexcept>

namespace {
    void check(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
    unsigned run(const std::vector<std::uint8_t>& sequence, std::uint64_t& ticks) {
        jaudio::Interpreter interpreter;
        interpreter.start(sequence);
        unsigned notes = 0;
        std::string error;
        for (unsigned block = 0; block < 10000 && !interpreter.finished(); ++block) {
            check(interpreter.advance([&](const jaudio::SequenceEvent& event) {
                if (event.type == jaudio::EventType::noteOn) {
                    check(event.velocity == 100 && event.key == 60, "Register note operands retain key and velocity.");
                    ++notes;
                }
            }, error), error);
        }
        check(interpreter.finished(), "Counted loop must finish.");
        ticks = interpreter.ticks();
        return notes;
    }
}

int main() {
    try {
        std::uint64_t ticks = 0;
        check(run({0xa4, 0, 100, 0xa4, 4, 2, 0xa4, 5, 100,
            0xc9, 0, 2, 0xc9, 0, 3, 60, 8, 0x80, 0x85, 0x84, 0xca, 0xca, 0xff}, ticks) == 6,
            "Nested counted loops preserve independent counts and register note duration.");
        check(run({0xc9, 0, 0, 60, 1, 100, 0xca, 0xff}, ticks) == 1,
            "Original zero loop count executes once, not forever.");
        check(run({0xc9, 0, 1, 60, 1, 100, 0xca, 0xff}, ticks) == 1, "Count one executes once.");
        check(run({0xa4, 4, 3, 0xcf, 4, 60, 1, 100, 0xff}, ticks) == 1 && ticks == 4,
            "Register wait resumes after its original tick count.");
        check(run({0xa4, 4, 0, 0xcf, 4, 60, 1, 100, 0xff}, ticks) == 1 && ticks == 1,
            "Zero register wait does not yield.");
        check(run({0xc9, 0, 2, 0xc4, 0, 0, 0, 10, 0xca, 0xff, 60, 1, 100, 0xc6, 0}, ticks) == 2,
            "Call returns and counted loops share stack ordering.");
        check(run({0xca}, ticks) == 0, "Unmatched loop end terminates the track.");
        std::cout << "Leaf sequence command checks passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
