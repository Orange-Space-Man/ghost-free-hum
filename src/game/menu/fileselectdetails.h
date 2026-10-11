#pragma once

#include "game/menu/fileselectstate.h"
#include "game/save/gamesavestate.h"
#include "game/menu/menulayout.h"

namespace fileselect {
    class DetailPanes {
    public:
        bool read(const std::vector<std::uint8_t>& executable, std::string& error);
        void reset(menulayout::Layout& layout);
        void update(menulayout::Layout& layout, const State& state, const gamesave::State& saves, bool expanded);
        std::vector<std::string> textures() const;

    private:
        std::array<std::string, 10> digits;
        std::array<std::string, 4> areas;
        std::array<int, 9> offsets{};
        std::array<int, 3> moneyX{};
        unsigned moneyDigits = 0;
        bool initialized = false;
    };
}
