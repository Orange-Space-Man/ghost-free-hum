#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dooranimation {
    class Resource {
    public:
        bool read(const std::vector<std::uint8_t>& data, std::string& error);
        bool loaded() const;
        const std::vector<std::uint8_t>& bytes() const;
        std::uint8_t headerByte(unsigned index) const;
        std::uint32_t headerValue() const;
        std::uint32_t sectionOffset(unsigned index) const;

    private:
        std::vector<std::uint8_t> storage;
        std::array<std::uint8_t, 2> attributes{};
        std::uint32_t value = 0;
        std::array<std::uint32_t, 4> offsets{};
    };
}
