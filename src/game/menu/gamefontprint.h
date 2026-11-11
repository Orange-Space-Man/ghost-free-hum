#pragma once

#include "game/menu/gamefont.h"
#include <string_view>

namespace gamefont {
    struct PrintLayout {
        std::array<float, 2> size{}, cursor{};
        std::vector<startupscene::Picture> pictures;
    };
    PrintLayout print(const Font& font, std::string_view text, float x, float baseline,
        unsigned width, unsigned height, int lineSpacing, bool shadow = false);
}
