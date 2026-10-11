#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace jaudio {
    struct EffectDestination {
        std::uint16_t bus = 0;
        std::uint16_t gain = 0;
    };

    struct EffectLine {
        std::uint8_t flags = 0;
        std::uint16_t delayBlocks = 0;
        std::array<EffectDestination, 2> destinations{};
        std::array<std::int16_t, 8> coefficients{};
    };

    struct EffectConfiguration {
        std::array<std::uint32_t, 4> allocationBlocks{};
        std::vector<std::array<EffectLine, 4>> presets;
    };

    bool decodeEffectConfiguration(const std::vector<std::uint8_t>& data, EffectConfiguration& configuration, std::string& error);
}
