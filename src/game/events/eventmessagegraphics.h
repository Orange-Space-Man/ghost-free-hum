#pragma once

#include "game/events/eventmessagewindows.h"
#include "game/menu/gamefont.h"
#include "game/scene/gamearchive.h"

namespace gameevent {
    class MessageGraphics {
    public:
        MessageGraphics(const gamearchive::Archive& archive, const std::vector<std::uint8_t>& executable, const gamefont::Font& font);
        std::array<float, 2> measure(const std::string& text) const;
        std::vector<startupscene::Picture> pictures(const MessageWindows& windows) const;
        const std::vector<roommaterials::Texture>& textures() const;

    private:
        std::size_t load(const gamearchive::Archive& archive, const std::string& path);
        const gamefont::Font& font;
        std::vector<roommaterials::Texture> images;
        std::array<std::array<float, 4>, 21> colors;
        std::array<std::size_t, 19> portraits{};
        std::size_t upper = 0, lower = 0, fontOffset = 0;
        int portraitX = 0, portraitY = 0, textX = 0, textY = 0;
        int openingFrames = 0, closingFrames = 0;
        unsigned fontSize = 0;
        int lineSpacing = 0;
    };
}
