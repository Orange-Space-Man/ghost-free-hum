#include "game/menu/fileselectdetails.h"
#include "game/menu/fileselectpanes.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"
#include "game/menu/menulayoutgraphics.h"

#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* text) { if (!value) throw std::runtime_error(text); }

    void write(std::vector<std::uint8_t>& data, unsigned offset, unsigned value) {
        for (unsigned i = 0; i < 4; ++i) data[offset + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }

    std::vector<std::uint8_t> executable() {
        std::vector<std::uint8_t> data(0x10100);
        write(data, 0, 0x100);
        write(data, 0x48, 0x80330000);
        write(data, 0x90, 0x10000);
        for (unsigned i = 0; i < 14; ++i) {
            const auto address = 0x80335000 + i * 128;
            write(data, 0x100 + (i < 10 ? 0x43c4 + i * 4 : 0x455c + (i - 10) * 4), address);
            const auto text = i < 10 ? "digit" + std::to_string(i) : "area" + std::to_string(i - 10);
            std::copy(text.begin(), text.end(), data.begin() + 0x5100 + i * 128);
        }
        return data;
    }

    menulayout::Layout layout() {
        menulayout::Layout result;
        const char* rows[]{
            "plce 1cnm 1cnw me4a me4b tere tnsl tnn5 tnn0 tnm2 tnm1 gcm2 gcm1 gnmg gnm9 gnm8 gnm7 gnm6 gnm5 gnm4 gnm3 gnm2 gnm1",
            "2lce 2cnm 2cnw 2e4a 2e4b 2ere 2nsl 2nn5 2nn0 2tm2 2tm1 2cm2 2cm1 2nmg 2nm9 2nm8 2nm7 2nm6 2nm5 2nm4 2nm3 2nm2 2nm1",
            "3lce 3cnm 3cnw 3e4a 3e4b 3ere 3nsl 3nn5 3nn0 3tm2 3tm1 3cm2 3cm1 3nmg 3nm9 3nm8 3nm7 3nm6 3nm5 3nm4 3nm3 3nm2 3nm1",
            "tg11 tg12 tg20 tg31 tg32"};
        for (const std::string row : rows) for (std::size_t offset = 0; offset < row.size(); offset += 5) {
            menulayout::Pane pane;
            pane.tag = row.substr(offset, 4);
            result.panes.push_back(pane);
        }
        return result;
    }

    void check(menulayout::Layout layout, fileselect::DetailPanes details) {
        fileselect::State state;
        gamesave::State saves;
        saves.initialized = true;
        details.reset(layout);
        details.update(layout, state, saves, false);
        require(!layout.find("me4a")->visible, "Details appeared before expansion.");
        details.update(layout, state, saves, true);
        require(layout.find("me4a")->visible && layout.find("me4a")->alpha == 16, "Empty file label.");
        saves.working[13] = 1;
        saves.working[12 + 0xb] = 8;
        saves.working[12 + 0x368] = 0xff;
        saves.working[12 + 0x36f] = 0xff;
        saves.working[12 + 0x353] = 0x10;
        saves.working[12 + 0x3a7] = 2;
        const auto putMoney = [&](unsigned amount) {
            for (unsigned i = 0; i < 4; ++i) saves.working[12 + 0x3a8 + i] = static_cast<std::uint8_t>(amount >> (24 - i * 8));
        };
        putMoney(1002003);
        details.reset(layout);
        details.update(layout, state, saves, true);
        require(layout.find("1cnm")->text == "2" && layout.find("plce")->visible, "Used file text.");
        require(layout.find("tnm2")->textures[0] == details.textures()[2] &&
            layout.find("tnm1")->textures[0] == details.textures()[1], "Boo count must use exactly sixty bits.");
        require(layout.find("gnm9")->textures[0] == details.textures()[3] &&
            !layout.find("gnm2")->visible && layout.find("gnm3")->visible, "Money digit order or leading zeros.");
        require(layout.find("gcm1")->alpha == 16 && layout.find("gcm2")->alpha == 16, "Money separator thresholds.");
        putMoney(0);
        details.update(layout, state, saves, true);
        require(layout.find("gnm9")->visible && !layout.find("gnm8")->visible, "Zero money display.");
        state.selection = 1;
        details.update(layout, state, saves, false);
        require(!layout.find("gnm9")->visible && layout.find("plce")->alpha == 0, "Deselected file retained details.");
        fileselect::HelpPanes help;
        help.reset(layout);
        state.mode = 0;
        for (unsigned i = 0; i < 64; ++i) help.update(layout, state);
        require(layout.find("tg11")->alpha == 255 && layout.find("tg12")->alpha == 255, "Help fade-in.");
        for (unsigned i = 0; i < 225; ++i) help.update(layout, state);
        require(layout.find("tg12")->alpha == 251, "Secondary help fade threshold.");
        state.mode = 7;
        for (unsigned i = 0; i < 64; ++i) help.update(layout, state);
        require(layout.find("tg31")->alpha == 255 && layout.find("tg32")->alpha == 0, "Erase help initial phase.");
        for (unsigned i = 0; i < 365; ++i) help.update(layout, state);
        require(layout.find("tg31")->alpha == 0 && layout.find("tg32")->alpha == 255, "Erase help alternation.");
    }
}

int main(int argc, char** argv) {
    try {
        fileselect::DetailPanes details;
        std::string error;
        require(details.read(executable(), error), error.c_str());
        check(layout(), details);
        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> data, archive;
            menulayout::Layout original;
            require(package.open(argv[1], error), error.c_str());
            require(package.read("sys/main.dol", data, error), error.c_str());
            require(details.read(data, error), error.c_str());
            require(package.read("files/Kawano/ENGLISH/res_slct.szp", data, error), error.c_str());
            require(roomarchive::decompress(data, archive, error), error.c_str());
            require(roomarchive::read(archive, "blo/file_select_1.blo", data, error), error.c_str());
            require(menulayout::read(data, original, error), error.c_str());
            menulayout::Graphics graphics;
            require(graphics.load(archive, original, error, details.textures()), error.c_str());
            check(original, details);
        }
        std::cout << "Save-detail and help checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
