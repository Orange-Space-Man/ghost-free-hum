#pragma once

#include "game/world/roomscenereader.h"

namespace gameplayspawn {
    struct Spawn {
        std::array<float, 3> position{};
        std::array<std::int16_t, 3> rotation{};
        int characterRecord = -1;
        std::uint32_t roomId = 0xffffffff;
        std::string roomArchive;
    };

    bool readCharacter(const std::vector<std::uint8_t>& data, unsigned appearPoint, Spawn& spawn, std::string& error);
    bool resolveRoom(const std::vector<std::uint8_t>& executable, unsigned map, Spawn& spawn, std::string& error);
    bool load(const std::filesystem::path& package, Spawn& spawn, roomscene::Scene& room, std::string& error);
}
