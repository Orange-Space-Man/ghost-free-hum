#include "game/actors/actortimingsampler.h"
#include "game/enemies/itemghostloot.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    void half(std::vector<std::uint8_t>& data, unsigned offset, unsigned value) {
        data.at(offset) = static_cast<std::uint8_t>(value >> 8);
        data.at(offset + 1) = static_cast<std::uint8_t>(value);
    }

    void word(std::vector<std::uint8_t>& data, unsigned offset, unsigned value) {
        half(data, offset, value >> 16);
        half(data, offset + 2, value);
    }

    void number(std::vector<std::uint8_t>& data, unsigned offset, float value) {
        unsigned bits;
        std::memcpy(&bits, &value, 4);
        word(data, offset, bits);
    }

    unsigned hash(const char* name) {
        unsigned result = 0;
        while (*name) result = (result * 256 + static_cast<unsigned char>(*name++)) % 33554393;
        return result;
    }
}

int main() {
    try {
        actormodel::Resource timing;
        timing.bytes.resize(128);
        auto& bytes = timing.bytes;
        half(bytes, 0, 2);
        word(bytes, 4, 56);
        const float values[]{0, 0, 0, 30, 1, 0};
        for (unsigned i = 0; i < 6; ++i) number(bytes, 8 + i * 4, values[i]);
        std::memcpy(bytes.data() + 56, "prefix_alfa2_timing", 19);
        half(bytes, 86, 2); half(bytes, 88, 0); half(bytes, 90, 3);
        std::memcpy(bytes.data() + 92, "alfa2_timing", 12);
        half(bytes, 122, 1); half(bytes, 124, 4); half(bytes, 126, 1);
        require(actoranimation::sampleTiming(nullptr, "alfa2_timing", 0) == -10000, "Missing timing resource sentinel.");
        require(actoranimation::sampleTiming(&timing, "absent", 0) == -10000, "Missing timing channel sentinel.");
        require(actoranimation::sampleTiming(&timing, "alfa2_timing", -1) == 0 &&
            actoranimation::sampleTiming(&timing, "alfa2_timing", 31) == 1, "Timing clamps to endpoint values.");
        require(std::fabs(actoranimation::sampleTiming(&timing, "alfa2_timing", 15) - 0.5f) < 0.00001f,
            "First substring match samples the original cubic track.");
        require(actoranimation::sampleTiming(&timing, "alfa2_timing", std::numeric_limits<double>::quiet_NaN()) == 0,
            "Unordered frame comparison retains the first key value.");
        require(actoranimation::timingAlpha(-10000) == 255 && actoranimation::timingAlpha(-1) == 0 &&
            actoranimation::timingAlpha(0.5f) == 127 && actoranimation::timingAlpha(2) == 255 &&
            actoranimation::timingAlpha(std::numeric_limits<float>::quiet_NaN()) == 0, "Original alpha sentinel, clamp and truncation.");
        word(bytes, 4, 127);
        bool rejected = false;
        try { actoranimation::sampleTiming(&timing, "alfa2_timing", 0); } catch (const std::exception&) { rejected = true; }
        require(rejected, "Truncated timing records fail bounds checks.");

        constexpr const char* names[]{"rdiamond", "coin", "gold", "bill", "other", "SPearl"};
        constexpr unsigned offsets[]{0, 4, 8, 12, 16, 32};
        constexpr unsigned start = 16 + 6 * 12;
        std::vector<std::uint8_t> data(start + 36);
        word(data, 0, 1); word(data, 4, 6); word(data, 8, start); word(data, 12, 36);
        for (unsigned i = 0; i < 6; ++i) {
            word(data, 16 + i * 12, hash(names[i]));
            word(data, 20 + i * 12, 0xffffffff);
            word(data, 24 + i * 12, offsets[i] << 16);
        }
        word(data, start, 4); word(data, start + 4, 257); word(data, start + 8, 258);
        word(data, start + 12, 256); word(data, start + 32, 3);
        std::memcpy(data.data() + start + 16, "key_test", 9);
        jmp::ToolData table;
        std::string error;
        require(table.attach(data, error), error.c_str());
        const auto loot = itemghost::readLoot({&table, 0});
        require(loot.drops.size() == 5 && loot.drops[0].kind == itemghost::Drop::Kind::coin && loot.drops[0].count == 1 &&
            loot.drops[1].kind == itemghost::Drop::Kind::bill && loot.drops[1].count == 0,
            "Coin and bill arguments truncate to bytes and preserve zero calls.");
        require(loot.drops[2].type == 0xcb && loot.drops[2].count == 258 && loot.drops[3].type == 0xc2 &&
            loot.drops[4].type == 0xca && loot.other == "key_test", "Loot retains native field order, actor counts and named item.");
        rejected = false;
        try { itemghost::readLoot({&table, -1}); } catch (const std::exception&) { rejected = true; }
        require(rejected, "An absent original loot row is not replaced by an invented reward.");
        std::cout << "Item ghost dependency checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
