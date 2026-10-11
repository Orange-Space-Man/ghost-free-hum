#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace menulayout {
    struct Pane {
        std::string kind;
        std::string tag;
        int parent = -1;
        bool visible = true;
        int x = 0, y = 0, width = 0, height = 0;
        unsigned rotation = 0, anchor = 0, alpha = 255;
        std::array<std::string, 5> textures;
        std::string palette;
        unsigned binding = 15, mirror = 0, wrap = 0;
        std::uint32_t black = 0, white = 0xffffffff;
        std::array<std::uint32_t, 4> colors{0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
        std::array<unsigned, 4> content{};
        std::string font;
        std::string text;
        unsigned textAlignment = 0, fontWidth = 0, fontHeight = 0;
        int characterSpacing = 0, lineSpacing = 0;
        bool textConnected = false;
        std::uint64_t order = 0;
        int previewTexture = -1;
    };

    struct Layout {
        unsigned width = 0, height = 0;
        std::uint32_t background = 0;
        std::vector<Pane> panes;
        Pane* find(const std::string& tag);
        void raise(const std::string& tag);
        std::vector<std::size_t> drawOrder() const;
    };

    bool read(const std::vector<std::uint8_t>& data, Layout& layout, std::string& error);
}
