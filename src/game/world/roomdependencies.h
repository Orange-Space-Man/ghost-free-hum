#pragma once

#include <cstdint>
#include <vector>

namespace roomdependencies {
    class Catalog {
    public:
        Catalog(const std::vector<std::uint8_t>& executable, unsigned map);
        const std::vector<std::int16_t>* list(unsigned room) const;
        bool contains(unsigned playerRoom, unsigned room) const;

    private:
        std::vector<std::vector<std::int16_t>> rooms;
    };
}
