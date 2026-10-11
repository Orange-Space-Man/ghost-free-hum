#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace enemytypes {
    class Catalog {
    public:
        bool read(const std::vector<std::uint8_t>& executable, std::string& error);
        std::uint32_t find(std::string_view name) const;
        const std::string& name(unsigned type) const;
        std::uint16_t flags(unsigned type) const;

    private:
        struct Type {
            std::string name;
            std::uint16_t flags = 0;
            std::uint32_t hash = 0;
        };
        std::array<Type, 381> types;
        bool loaded = false;
    };
}
