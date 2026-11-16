#pragma once

#include "game/menu/menulayout.h"
#include "game/startup/startupscene.h"
#include "game/menu/gamefont.h"
#include <map>
#include <functional>
#include "game/scene/gamearchive.h"

namespace menulayout {
    class Graphics {
    public:
        bool load(const std::vector<std::uint8_t>& archive, const Layout& layout, std::string& error,
            const std::vector<std::string>& additionalTextures = {});
        bool load(const gamearchive::Archive& archive, const Layout& layout, std::string& error,
            const std::vector<std::string>& additionalTextures = {});
        void setFont(const gamefont::Font& font);
        bool loadOptionsPortrait(const std::filesystem::path& packagePath, std::string& error);
        std::vector<startupscene::Picture> pictures(const Layout& layout) const;
        std::vector<roommaterials::Texture> textures;
        std::size_t textureOffset = 0;

    private:
        bool loadTextures(const Layout& layout, std::string& error, const std::vector<std::string>& additionalTextures,
            const std::function<bool(const std::string&, std::vector<std::uint8_t>&, std::string&)>& read);
        std::map<std::string, std::size_t> names;
        gamefont::Font font;
        std::size_t fontOffset = 0;
    };
}
