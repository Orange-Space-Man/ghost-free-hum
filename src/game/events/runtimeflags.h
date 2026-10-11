#pragma once

#include <array>
#include <cstdint>

namespace gameflags {
    class State {
    public:
        void clear();
        bool value(std::uint8_t flag) const;
        void setOn(std::uint8_t flag);
        void setOff(std::uint8_t flag);
        void restore(const std::array<std::uint8_t, 11>& saved);
        void store(std::array<std::uint8_t, 11>& saved) const;

    private:
        std::array<std::uint8_t, 32> flags{};
    };
}
