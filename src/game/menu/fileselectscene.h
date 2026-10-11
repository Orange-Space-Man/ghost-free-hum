#pragma once

#include "game/menu/fileselectpanes.h"
#include "game/menu/fileselectdetails.h"
#include "game/menu/menulayoutgraphics.h"
#include "platform/d3d11/gamerenderer.h"

namespace fileselect {
    class Scene {
    public:
        bool load(const std::filesystem::path& package, std::string& error);
        void open();
        void reset();
        void update(const State& state, const gamesave::State& saves);
        void draw(bool present);
        bool ready(unsigned group) const;

    private:
        std::array<menulayout::Layout, 3> layouts;
        std::array<menulayout::Layout, 3> initialLayouts;
        std::array<menulayout::Graphics, 3> graphics;
        SelectionPanes selection;
        MessagePanes messages;
        OptionsPanes options;
        DetailPanes details;
        HelpPanes help;
        std::vector<s3dmodel::Model> models;
        std::array<s3danimation::Animation, 4> animations;
        std::array<s3denvironment::Environment, 4> environments;
        s3denvironment::MaterialAnimation materialAnimation;
        std::vector<float> trigonometry;
        std::array<int, 4> previewTextures{};
        unsigned updates = 0;
        bool opened = false;
    };
}
