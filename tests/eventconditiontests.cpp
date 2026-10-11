#include "game/events/eventconditions.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
}

int main() {
    try {
        gameflags::State flags;
        for (unsigned i = 0; i < 256; ++i) {
            const auto flag = static_cast<std::uint8_t>(i);
            require(!flags.value(flag), "Runtime flags begin cleared.");
            flags.setOn(flag);
        }
        std::array<std::uint8_t, 11> saved{};
        saved[0] = 0x81;
        saved[10] = 0xf0;
        flags.restore(saved);
        for (unsigned i = 0; i < 256; ++i) {
            const bool expected = i == 0 || i == 7 || i == 84 || i >= 85;
            require(flags.value(static_cast<std::uint8_t>(i)) == expected,
                "Restore replaces exactly 85 low-bit-first flags and retains all higher runtime flags.");
        }
        saved.fill(0xff);
        saved[10] = 0xa0;
        flags.store(saved);
        require(saved[0] == 0x81 && saved[1] == 0 && saved[10] == 0xb0,
            "Store preserves the destination's unused high bits, not transient runtime bits.");
        flags.setOff(255);
        require(!flags.value(255) && flags.value(254), "Flag 255 can be cleared without disturbing its neighbor.");
        flags.clear();
        for (unsigned i = 0; i < 256; ++i)
            require(!flags.value(static_cast<std::uint8_t>(i)), "Full flag clear includes transient flags.");

        gameevent::ActivationState activation;
        gameevent::ConditionState state;
        gameevent::Conditions conditions;
        state.eventNumber = 109;
        activation.configure(state, conditions, 0, flags);
        require(state.flags == 4 && activation.value(109) == 1, "Unconditional unlimited event starts active.");
        state.eventNumber = 108;
        activation.configure(state, conditions, 0, flags);
        activation.reset();
        require(activation.value(108) == 0 && activation.value(109) == 1 && activation.value(110) == 0,
            "Original reset clears 109 bytes but lookup permits event 109.");
        bool rejected = false;
        try { activation.value(-1); } catch (const std::out_of_range&) { rejected = true; }
        require(rejected, "Negative native event lookup cannot read before storage.");

        state = {};
        state.eventNumber = 3;
        conditions.loadLimit = 2;
        activation.configure(state, conditions, 2, flags);
        require((state.flags & 0x40) && !activation.value(3), "Reached load limit disables initial activation.");
        state = {};
        state.eventNumber = 4;
        activation.configure(state, conditions, 1, flags);
        require(state.flags == 0 && activation.value(4) == 1, "Unsigned completion count below signed limit permits activation.");
        activation.configure(state, conditions, 2, flags);
        require((state.flags & 0x40) && activation.value(4) == 1,
            "Configuration does not clear an already active entry when its disable condition becomes true.");
        state = {};
        conditions.loadLimit = 0x10000;
        activation.configure(state, conditions, 0, flags);
        require(state.loadLimit == 0 && state.flags == 0x40,
            "Unlimited test uses the full source word before comparison uses its truncated signed half.");
        state = {};
        conditions.loadLimit = 0xffff;
        activation.configure(state, conditions, 0, flags);
        require(state.loadLimit == -1 && (state.flags & 0x40), "Signed negative load limits remain disabling.");

        conditions = {};
        flags.setOn(0);
        state = {};
        state.eventNumber = 5;
        activation.configure(state, conditions, 0, flags);
        require((state.flags & 0x40) && !activation.value(5), "Initialization queries disappear flag zero rather than skipping it.");
        flags.clear();
        flags.setOn(1);
        conditions.disappearFlag = 0x101;
        state = {};
        activation.configure(state, conditions, 0, flags);
        require(state.disappearFlag == 257 && (state.flags & 0x40), "Flag query narrows to the low byte.");
        conditions.disappearFlag = 0;
        conditions.appearFlag = 1;
        state = {};
        state.eventNumber = 6;
        activation.configure(state, conditions, 0, flags);
        require(!activation.value(6), "Nonzero appearance flag waits for the update path even if already set.");
        conditions.appearFlag = 0;
        conditions.startTime = 0x1000a;
        conditions.endTime = 20;
        conditions.lock = 1;
        conditions.playerStop = 1;
        conditions.parameter = 0x87654321;
        conditions.characterType = 380;
        state = {};
        state.eventNumber = 7;
        state.flags = 8;
        activation.configure(state, conditions, 0, flags);
        require(state.flags == 0x38c && state.startTime == 10 && state.endTime == 20 &&
            state.parameter == 0x87654321 && state.characterType == 380 && !activation.value(7),
            "Configuration retains existing bits, truncates time and transfers parameters.");
        require(gameevent::checkTime(state, 9, 0) == 0 && gameevent::checkTime(state, 10, 0) == 1 &&
            gameevent::checkTime(state, 19, 0) == 1 && gameevent::checkTime(state, 20, 0) == 2 &&
            gameevent::checkTime(state, 9, 1) == 1, "Event time window includes start and excludes end, with original lead adjustment.");
        state.startTime = std::numeric_limits<std::int32_t>::min();
        state.endTime = 100;
        require(gameevent::checkTime(state, 0, 1) == 0, "Lead subtraction retains PowerPC word wrap.");
        conditions.startTime = 0xffff;
        conditions.lock = 2;
        conditions.playerStop = 2;
        state = {};
        activation.configure(state, conditions, 0, flags);
        require(state.startTime == -1 && state.flags == 0x84, "Only lock and stop values equal to one enable their bits.");
        state.flags = 0;
        require(gameevent::checkTime(state, 1000, 0) == 1, "Events without timed flag ignore time window.");
        flags.clear();
        activation.reset();
        conditions = {};
        conditions.appearFlag = 3;
        conditions.startTime = 10;
        conditions.endTime = 20;
        state = {};
        state.eventNumber = 48;
        activation.configure(state, conditions, 0, flags);
        activation.updateAppearance(state, flags, 12);
        require(!activation.value(48), "Appearance waits for its real flag even inside the event time window.");
        flags.setOn(3);
        activation.updateAppearance(state, flags, 8);
        require(!activation.value(48), "Appearance does not activate before the original one-second lead.");
        activation.updateAppearance(state, flags, 9);
        require(activation.value(48) == 1, "Appearance activates at the one-second lead boundary.");
        flags.setOff(3);
        activation.updateAppearance(state, flags, 20);
        require(activation.value(48) == 1, "Appearance activation remains latched until original event completion clears it.");
        activation.reset();
        flags.setOn(3);
        state.flags |= 0x40;
        activation.updateAppearance(state, flags, 12);
        require(!activation.value(48), "Completed or disabled event records cannot activate their placements.");
        state.flags &= ~0x40u;
        activation.updateAppearance(state, flags, 20);
        require(!activation.value(48), "An expired event window cannot newly activate placements.");
        std::cout << "Runtime flags and original event-condition edge cases verified.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
