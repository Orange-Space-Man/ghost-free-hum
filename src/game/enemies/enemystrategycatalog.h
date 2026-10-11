#pragma once

#include "game/events/jmptooldata.h"

#include <array>

namespace enemystrategy {
    class Catalog {
    public:
        bool read(const std::vector<std::uint8_t>& executable, std::string& error);
        unsigned select(unsigned type, const jmp::ToolDataRef& source) const;
        unsigned selectGenerator(unsigned type) const;
        std::uint32_t constructor(unsigned strategy) const;
        unsigned attachmentJoint(unsigned type) const;

    private:
        struct Selector {
            unsigned strategy = 0;
            bool attackOverride = false;
        };
        std::array<Selector, 381> selectors{};
        std::array<unsigned, 381> generatorSelectors{};
        std::array<unsigned, 381> attachmentJoints{};
        std::array<std::uint32_t, 187> constructors{};
        bool loaded = false;
    };
}
