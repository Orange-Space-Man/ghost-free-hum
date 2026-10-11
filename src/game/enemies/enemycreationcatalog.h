#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace enemycreation {
    struct Pool {
        std::uint32_t capacity = 0, type = 0, count = 0;
    };

    class Catalog {
    public:
        bool read(const std::vector<std::uint8_t>& executable, std::string& error);
        const std::array<Pool, 4>& pools() const;
        unsigned category(unsigned type) const;
        unsigned capacity(unsigned type) const;
        unsigned model(unsigned type) const;
        std::int16_t group(unsigned type) const;
        bool suppressInClearedRoom(unsigned type) const;
        bool countsForRoom(unsigned type) const;
        std::uint32_t ownerConstructor(unsigned type) const;

    private:
        struct Type {
            std::uint32_t model = 0, modelCapacity = 0, constructor = 0;
            std::int16_t group = 0;
            bool validModel = false;
        };
        std::array<Pool, 4> groups{};
        std::array<Type, 381> types{};
        bool loaded = false;
    };
}
