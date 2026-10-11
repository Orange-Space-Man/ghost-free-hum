#pragma once

#include "game/menu/fileselectstate.h"
#include "game/menu/menulayout.h"

namespace fileselect {
    class HelpPanes {
    public:
        void reset(menulayout::Layout& layout);
        void update(menulayout::Layout& layout, const State& state);

    private:
        unsigned timer = 0;
        unsigned previousMode = 0;
    };
    class OptionsPanes {
    public:
        bool read(const std::vector<std::uint8_t>& executable, std::string& error);
        void reset(menulayout::Layout& layout);
        void update(menulayout::Layout& layout, const State& state);
        std::vector<std::string> textures() const;

    private:
        std::array<std::vector<std::string>, 6> choices;
        std::array<std::string, 5> helpText;
        unsigned phase = 255;
    };
    void bindPreviews(std::array<menulayout::Layout, 3>& layouts, const State& state,
        const std::array<bool, 3>& used, const std::array<int, 4>& textures);
    class SelectionPanes {
    public:
        void reset(std::array<menulayout::Layout, 3>& layouts);
        void update(std::array<menulayout::Layout, 3>& layouts, const State& state, const std::array<bool, 3>& used);
        bool expanded(unsigned group) const;

    private:
        std::array<std::array<menulayout::Pane, 7>, 5> original;
        std::array<float, 5> scales{1,1,1,1,1};
        std::array<std::array<int, 2>, 4> centers{};
        std::array<std::array<std::array<int, 2>, 4>, 3> destinations{};
        unsigned cursorAlpha = 0;
        unsigned phase = 255;
        bool initialized = false;
    };

    class MessagePanes {
    public:
        void reset(std::array<menulayout::Layout, 3>& layouts);
        void update(std::array<menulayout::Layout, 3>& layouts, const State& state, const std::array<bool, 3>& used);

    private:
        unsigned phase = 255;
        unsigned damagedPhase = 0;
    };
}
