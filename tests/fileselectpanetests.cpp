#include "game/menu/fileselectpanes.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"
#include "game/menu/menulayoutgraphics.h"

#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    void check(std::array<menulayout::Layout, 3> layouts) {
        fileselect::MessagePanes presentation;
        presentation.reset(layouts);
        fileselect::State state;
        const std::array<bool, 3> used{true, false, true};
        presentation.update(layouts, state, used);
        require(layouts[0].find("m11a")->alpha == 16 && layouts[0].find("m11b")->alpha == 12,
            "Initial message fade and original pulse phase.");
        require(layouts[0].find("m12a")->alpha == 16 && layouts[0].find("m13a")->alpha == 16,
            "Used file copy/erase availability.");
        state.mode = 7;
        state.confirmation = 1;
        presentation.update(layouts, state, used);
        require(layouts[0].find("m11a")->alpha == 0 && layouts[0].find("m31a")->alpha == 16,
            "Erase prompt cross-fade.");
        require(layouts[0].find("oc1n")->alpha == 255, "Default No highlight.");
        state.confirmation = 0;
        presentation.update(layouts, state, used);
        require(layouts[0].find("oc1y")->alpha == 255 && layouts[0].find("oc1n")->alpha == 223,
            "Unselected confirmation fade step.");
        for (unsigned mode = 0; mode < 22; ++mode) {
            state.mode = mode;
            for (unsigned frame = 0; frame < 230; ++frame) presentation.update(layouts, state, used);
            for (const auto& layout : layouts) for (const auto& pane : layout.panes)
                require(pane.alpha <= 255, "Pane alpha exceeded byte range.");
        }
        state.mode = 0;
        state.selection = 1;
        for (unsigned frame = 0; frame < 16; ++frame) presentation.update(layouts, state, used);
        require(layouts[0].find("m12a")->alpha == 0 && layouts[0].find("m13a")->alpha == 0,
            "Empty slot exposes copy/erase controls.");
    }

    void checkSelection(std::array<menulayout::Layout, 3> layouts) {
        const auto width = layouts[0].find("fra1")->width;
        fileselect::SelectionPanes presentation;
        presentation.reset(layouts);
        fileselect::State state;
        state.cursorPhase = 255;
        fileselect::bindPreviews(layouts, state, {true,false,true}, {10,11,12,13});
        require(layouts[0].find("mov1")->previewTexture == 11 && layouts[0].find("mov2")->previewTexture == 10 &&
            layouts[0].find("movo")->previewTexture == 12 && layouts[1].find("mov5")->previewTexture == 10, "Original preview model selection.");
        for (unsigned frame = 0; frame < 30; ++frame) presentation.update(layouts, state, {true,false,true});
        require(layouts[0].find("fra1")->width > width, "Selected frame did not expand.");
        require(layouts[0].find("pic1")->alpha == 0 && layouts[0].find("pic2")->alpha == 255, "Selected preview cover visibility.");
        state.selection = 1;
        presentation.update(layouts, state, {true,false,true});
        require(layouts[0].find("fra1")->width == width, "Unselected frame did not reset its size.");
        state.mode = 3;
        state.selection = 0;
        state.copyDestination = 1;
        presentation.update(layouts, state, {true,false,true});
        require(!layouts[0].find("cur1")->visible && layouts[0].find("cur2")->visible, "Copy destination cursor.");
        state.mode = 20;
        state.selection = 3;
        fileselect::bindPreviews(layouts, state, {true,false,true}, {10,11,12,13});
        require(layouts[0].find("movo")->previewTexture == 13, "Options preview selection.");
        for (unsigned frame = 0; frame < 100; ++frame) presentation.update(layouts, state, {true,false,true});
        require(layouts[0].find("fra1")->alpha == 0 && layouts[0].find("frao")->alpha == 255, "Options transition selection panes.");
    }
}

int main(int argc, char** argv) {
    try {
        std::array<menulayout::Layout, 3> layouts;
        const char* tags[]{
            "m11a m21a m22a m23a m31a m32a m33a sv1a sv2a ye1a no1a m12a m13a me5a me6a",
            "er1a er2a e31a e32a e33a e34a e41a e42a ye2a no2a es1a"};
        for (unsigned i = 0; i < 2; ++i) {
            const std::string names = tags[i];
            for (std::size_t offset = 0; offset < names.size(); offset += 5) {
                menulayout::Pane value;
                value.tag = names.substr(offset, 4);
                layouts[i].panes.push_back(value);
                value.tag.back() = 'b';
                layouts[i].panes.push_back(value);
            }
        }
        for (const auto* tag : {"note", "oc1y", "oc1n"}) { menulayout::Pane value; value.tag = tag; layouts[0].panes.push_back(value); }
        for (const auto* tag : {"oc2y", "oc2n"}) { menulayout::Pane value; value.tag = tag; layouts[1].panes.push_back(value); }
        check(layouts);
        const char* selectionTags[]{
            "cur1 fra1 mov1 sha1 pic1 ti1a ti1b cur2 fra2 mov2 sha2 pic2 ti2a ti2b cur3 fra3 mov3 sha3 pic3 ti3a ti3b curo frao movo sha4 pico ti4a ti4b",
            "cur5 fra5 mov5 sha5 pic5 ti5a ti5b pico curo frao movo shao",
            "frao movo shao"};
        for (unsigned i = 0; i < 3; ++i) {
            const std::string names = selectionTags[i];
            for (std::size_t offset = 0; offset < names.size(); offset += 5) {
                menulayout::Pane value;
                value.tag = names.substr(offset, 4);
                value.width = 100;
                value.height = 80;
                value.x = int(i) * 100;
                layouts[i].panes.push_back(value);
            }
        }
        checkSelection(layouts);
        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> data, archive;
            std::string error;
            require(package.open(argv[1], error), error.c_str());
            require(package.read("files/Kawano/ENGLISH/res_slct.szp", data, error), error.c_str());
            require(roomarchive::decompress(data, archive, error), error.c_str());
            const char* names[]{"blo/file_select_1.blo", "blo/file_select_2.blo", "blo/option_1.blo"};
            for (unsigned i = 0; i < 3; ++i) {
                require(roomarchive::read(archive, names[i], data, error), error.c_str());
                require(menulayout::read(data, layouts[i], error), error.c_str());
            }
            check(layouts);
            checkSelection(layouts);
            fileselect::OptionsPanes options;
            require(package.read("sys/main.dol", data, error), error.c_str());
            if (!options.read(data, error)) throw std::runtime_error(error);
            require(options.textures().size() == 14, "Original options texture table lengths.");
            options.reset(layouts[2]);
            menulayout::Graphics graphics;
            if (!graphics.load(archive, layouts[2], error, options.textures())) throw std::runtime_error(error);
            require(graphics.loadOptionsPortrait(argv[1], error), error.c_str());
            gamefont::Font font;
            require(font.readExecutable(data, error), error.c_str());
            graphics.setFont(font);
            fileselect::State state;
            state.mode = 20;
            for (unsigned sound = 0; sound < 3; ++sound) for (unsigned rumble = 0; rumble < 2; ++rumble)
                for (unsigned controls = 0; controls < 2; ++controls) for (unsigned selection = 0; selection < 6; ++selection) {
                    state.options = {sound,rumble,controls};
                    state.optionSelection = selection;
                    options.update(layouts[2], state);
                    require(!layouts[2].find("helpText")->text.empty(), "Original help text missing for options selection.");
                    require(layouts[2].find("helpText")->visible == (layouts[2].find("mesw")->alpha == 255), "Help must wait for the original window fade.");
                    require(!graphics.pictures(layouts[2]).empty(), "Options failed to draw.");
                    require(layouts[2].find("smpl")->visible == (selection == 3), "Options sample visibility.");
                }
        }
        std::cout << "File-selection message pane checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
