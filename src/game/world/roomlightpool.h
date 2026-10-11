#pragma once

#include "game/models/modellighting.h"

namespace roomlighting {
    struct Entry {
        modellighting::RoomLight light;
        std::uint8_t priority = 0;
    };

    class Pool {
    public:
        void beginFrame();
        bool add(std::uint8_t priority, const modellighting::RoomLight& light);
        bool setPrimary(std::uint8_t priority, const modellighting::RoomLight& light);
        std::vector<modellighting::RoomLight> lights() const;
        const Entry& entry(unsigned index) const;
        unsigned count() const;
        unsigned last() const;
        std::uint8_t disabled = 0;

    private:
        std::array<Entry, 6> entries{};
        unsigned used = 0, selected = 0;
        Entry* allocate(std::uint8_t priority);
    };
}
