#include "game/menu/menulayout.h"
#include "game/menu/menulayoutgraphics.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"

#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const std::string& message) {
        if (!condition) throw std::runtime_error(message);
    }

    void word(std::vector<std::uint8_t>& data, unsigned value) {
        data.push_back(static_cast<std::uint8_t>(value >> 8));
        data.push_back(static_cast<std::uint8_t>(value));
    }

    void dword(std::vector<std::uint8_t>& data, unsigned value) {
        word(data, value >> 16);
        word(data, value);
    }

    void block(std::vector<std::uint8_t>& data, const std::string& kind, const std::vector<std::uint8_t>& body = {}) {
        data.insert(data.end(), kind.begin(), kind.end());
        dword(data, static_cast<unsigned>(body.size() + 8));
        data.insert(data.end(), body.begin(), body.end());
    }

    std::vector<std::uint8_t> fixture() {
        std::vector<std::uint8_t> data{'S','C','R','N','b','l','o','1'};
        data.resize(32);
        block(data, "INF1", {2,128,1,224,0,0,0,0});
        block(data, "PAN1", {6,1,0,0,'R','O','O','T',0,0,0,0,2,128,1,224});
        block(data, "BGN1");
        block(data, "PIC1", {9,1,0,0,'t','e','s','t',255,246,0,20,0,32,0,16,0,90,4,128,
            7,2,1,'x',0,0,15,3,0,0,0,0,0,255,255,255,255,0,0,0});
        block(data, "END1");
        block(data, "EXT1");
        data.resize(data.size() + 16);
        const auto size = static_cast<unsigned>(data.size());
        for (unsigned i = 0; i < 4; ++i) data[8 + i] = static_cast<std::uint8_t>(size >> (24 - i * 8));
        return data;
    }
}

int main(int argc, char** argv) {
    try {
        menulayout::Layout layout;
        std::string error;
        auto data = fixture();
        require(menulayout::read(data, layout, error), error);
        require(layout.width == 640 && layout.height == 480 && layout.panes.size() == 2, "Canvas or pane count.");
        auto* pane = layout.find("test");
        require(pane && pane->parent == 0 && pane->x == -10 && pane->rotation == 90 && pane->anchor == 4 &&
            pane->alpha == 128 && pane->textures[0] == "x" && pane->mirror == 3 && pane->white == 0xffffffff, "Picture fields.");
        for (std::size_t size = 0; size < data.size(); ++size) {
            const std::vector<std::uint8_t> truncated(data.begin(), data.begin() + size);
            require(!menulayout::read(truncated, layout, error), "Accepted truncated layout.");
        }
        data[52] = 255;
        require(!menulayout::read(data, layout, error), "Accepted out of bounds block.");
        require(layout.panes.size() == 2, "Failed read changed existing layout.");
        menulayout::Layout layered;
        layered.panes.resize(4);
        layered.panes[0].kind = "PAN1";
        for (unsigned i = 1; i < 4; ++i) {
            auto& pane = layered.panes[i];
            pane.kind = "PIC1";
            pane.tag = std::to_string(i);
            pane.parent = i == 2 ? 1 : 0;
            pane.previewTexture = 10 + i;
            pane.width = pane.height = 20;
            pane.x = i * 10;
        }
        menulayout::Graphics layeredGraphics;
        layeredGraphics.textureOffset = 100;
        layered.panes[1].colors = {0xff0000ff, 0x00ff00ff, 0x0000ffff, 0xffffff20};
        layered.raise("1");
        auto rendered = layeredGraphics.pictures(layered);
        require(rendered.size() == 3 && rendered[0].texture == 13 && rendered[1].texture == 11 && rendered[2].texture == 12,
            "Raising a pane did not preserve its subtree or global preview texture indices.");
        require(rendered[2].transform[4] == 30, "Raising changed inherited transforms.");
        require(rendered[1].colors[0][0] == 1 && rendered[1].colors[0][1] == 0 &&
            rendered[1].colors[1][1] == 1 && rendered[1].colors[2][2] == 1 &&
            rendered[1].colors[3][3] == 32 / 255.0f, "Picture corner colors were lost.");
        layered.panes[1].visible = false;
        require(layeredGraphics.pictures(layered).size() == 1, "Hidden raised parent exposed its child.");
        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> compressed, archive, layoutData;
            require(package.open(argv[1], error), error);
            gamefont::Font font;
            require(package.read("sys/main.dol", layoutData, error), error);
            require(font.readExecutable(layoutData, error), error);
            require(font.textures.size() == 6 && font.width == 32 && font.ascent == 22 && font.descent == 10, "Original font metrics.");
            require(font.glyph('0').x == 0 && font.glyph('0').y == 0 && font.glyph('0').advance == 14, "Original digit mapping.");
            require(package.read("files/Kawano/ENGLISH/res_slct.szp", compressed, error), error);
            require(roomarchive::decompress(compressed, archive, error), error);
            for (const auto* name : {"file_select_1", "file_select_2", "option_1"}) {
                require(roomarchive::read(archive, std::string("blo/") + name + ".blo", layoutData, error), error);
                require(menulayout::read(layoutData, layout, error), error);
                menulayout::Graphics graphics;
                require(graphics.load(archive, layout, error), error);
                graphics.setFont(font);
                const auto pictures = graphics.pictures(layout);
                std::cout << name << ": " << layout.panes.size() << " panes, " << pictures.size() << " quads\n";
            }
        }
        std::cout << "Menu layout checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
