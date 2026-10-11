#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace playerweapons {
    std::uint32_t parameterAddress(int actorType, int partType);
    std::array<std::uint32_t, 19> readParameters(const std::vector<std::uint8_t>& executable, int actorType, int partType);

    struct Part {
        Part(const std::vector<std::uint8_t>& executable, int actorType, int partType);
        void reset(const std::vector<std::uint8_t>& executable, int actorType, int partType);
        std::array<std::uint32_t, 19> parameters;
        std::array<std::uint32_t, 165> first, second;
        std::array<std::uint32_t, 9> field574, field598, field5BC, field5E0;
        std::array<float, 3> position, field610, field61C, field628;
        std::array<std::uint8_t, 0x60> fields634;
    };
}
