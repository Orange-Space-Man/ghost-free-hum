#pragma once

#include "game/scene/gameparameters.h"
#include <array>

namespace enemytypes {
    gameparameters::Parameters defaults();

    class Manager {
    public:
        Manager();
        bool loadParameters(const std::vector<std::uint8_t>& executable, const std::vector<std::uint8_t>& archive, std::string& error);
        const gameparameters::Parameters& parameters(unsigned type) const;

    private:
        std::array<gameparameters::Parameters, 0x17d> types;
    };
}
