#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace gameevent {
    class MessageText {
    public:
        MessageText(const std::vector<std::uint8_t>& executable, std::vector<std::uint8_t> source);
        std::string line(unsigned index);

    private:
        std::vector<std::uint8_t> source;
        std::array<std::uint32_t, 8> colors{};
        static void color(std::string& text, std::uint32_t rgba);
    };
}
