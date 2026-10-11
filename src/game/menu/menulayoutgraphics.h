#pragma once

#include "game/menu/menulayout.h"
#include "game/startup/startupscene.h"
#include "game/menu/gamefont.h"
#include <map>

namespace menulayout {
    class Graphics {
    public:
        bool load(const std::vector<std::uint8_t>& archive, const Layout& layout, std::string& error,
            const std::vector<std::string>& additionalTextures = {});
        void setFont(const gamefont::Font& font);
        bool loadOptionsPortrait(const std::filesystem::path& packagePath, std::string& error);
        std::vector<startupscene::Picture> pictures(const Layout& layout) const;
        std::vector<roommaterials::Texture> textures;
        std::size_t textureOffset = 0;

    private:
        std::map<std::string, std::size_t> names;
        gamefont::Font font;
        std::size_t fontOffset = 0;
    };
}
