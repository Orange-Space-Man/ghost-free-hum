#pragma once

#include "game/world/roommodelreader.h"

namespace roompose {
    bool readInitial(const std::vector<std::uint8_t>& animation, std::size_t nodes,
        std::vector<roommodel::Node>& pose, std::string& error);
}
