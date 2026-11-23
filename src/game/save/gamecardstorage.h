#pragma once

#include "game/save/gamesavestate.h"
#include <filesystem>
#include <string>

namespace gamecard {
    using Block = std::array<std::uint8_t, 0x2000>;
    using Image = std::array<std::uint8_t, 0x6040>;
    enum class LoadResult { empty, loaded, damaged, unavailable };

    std::uint32_t checksum(const Block& block);
    bool valid(const Block& block);
    LoadResult decode(const Image& image, gamesave::State& state, const std::string& gameId = "GLME01");
    Image create(const std::string& gameId = "GLME01");
    void update(Image& image, gamesave::State& state, const Block& banner, std::uint32_t tick, std::uint32_t modified);

    class Storage {
    public:
        LoadResult open(const std::filesystem::path& filename, gamesave::State& state,
            std::string& error, const std::string& gameId = "GLME01");
        bool save(gamesave::State& state, const Block& banner, std::uint32_t tick, std::uint32_t modified, std::string& error);
        LoadResult status() const;

    private:
        std::filesystem::path path;
        Image image{};
        LoadResult result = LoadResult::unavailable;
    };
}
