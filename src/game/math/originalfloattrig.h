#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace originalmath {
    class FloatTrig {
    public:
        bool read(const std::vector<std::uint8_t>& executable, std::string& error);
        std::array<float, 2> sinCos(float angle) const;

    private:
        std::array<float, 4> reduction{};
        std::array<float, 18> polynomial{};
        std::array<float, 3> constants{};
        bool loaded = false;
    };
}
