#pragma once

#include "game/startup/startupscene.h"
#include <map>

namespace gamefont {
    struct Glyph {
        unsigned texture = 0, x = 0, y = 0;
        unsigned left = 0, advance = 0;
    };

    class Font {
    public:
        bool read(const std::vector<std::uint8_t>& data, const std::vector<std::uint8_t>& ascii, std::string& error);
        bool readExecutable(const std::vector<std::uint8_t>& dol, std::string& error);
        const Glyph& glyph(unsigned character) const;
        float draw(unsigned character, float x, float baseline, float width, float height, bool continuation,
            startupscene::Picture& picture) const;
        std::vector<roommaterials::Texture> textures;
        unsigned ascent = 0, descent = 0, width = 0, leading = 0;

    private:
        unsigned fallback = 0;
        std::map<unsigned, unsigned> mapping;
        std::map<unsigned, Glyph> glyphs;
    };
}
