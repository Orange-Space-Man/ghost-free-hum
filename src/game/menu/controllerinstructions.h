#pragma once

#include "game/menu/menulayoutgraphics.h"

namespace controllerinstructions {
    struct Result {
        bool changed = false;
        bool close = false;
        unsigned scheme = 1;
        unsigned playerEvent = 0;
        std::vector<unsigned> sounds;
    };

    class Screen {
    public:
        bool load(const std::filesystem::path& package, std::string& error);
        Result update(std::uint32_t pressed, unsigned option, std::uint64_t presentationFrame, bool gameplay);
        void reset();
        const menulayout::Layout& layout() const;
        const menulayout::Graphics& graphics() const;

    private:
        menulayout::Layout panes;
        menulayout::Graphics images;
        std::uint32_t initialColor = 0;
        unsigned pulsePeriod = 0;
        bool loaded = false;
    };
}
