#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/menu/fileselectscene.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

bool fileselect::Scene::ready(unsigned group) const { return selection.expanded(group); }

bool fileselect::Scene::load(const std::filesystem::path& path, std::string& error) {
    error.clear();
    try {
        Scene result;
        gameassets::Package package;
        gamefont::Font font;
        std::vector<std::uint8_t> data, archive, executable, table;
        if (!package.open(path, error) || !package.read("files/Kawano/ENGLISH/res_slct.szp", data, error) ||
            !roomarchive::decompress(data, archive, error) || !package.read("sys/main.dol", executable, error) ||
            !font.readExecutable(executable, error) || !result.options.read(executable, error) ||
            !result.details.read(executable, error) ||
            !dolresources::read(executable, glme01::sineTable, 5120 * 4, table, error)) throw std::runtime_error(error);
        result.trigonometry.resize(5120);
        for (std::size_t i = 0; i < result.trigonometry.size(); ++i) {
            const auto offset = i * 4;
            const std::uint32_t bits = (std::uint32_t(table[offset]) << 24) | (table[offset + 1] << 16) |
                (table[offset + 2] << 8) | table[offset + 3];
            std::memcpy(&result.trigonometry[i], &bits, 4);
        }
        const char* paths[]{"blo/file_select_1.blo", "blo/file_select_2.blo", "blo/option_1.blo"};
        for (unsigned i = 0; i < 3; ++i) {
            if (!roomarchive::read(archive, paths[i], data, error) || !menulayout::read(data, result.layouts[i], error) ||
                !result.graphics[i].load(archive, result.layouts[i], error,
                    i == 0 ? result.details.textures() : i == 2 ? result.options.textures() : std::vector<std::string>{})) throw std::runtime_error(error);
            if (i == 2 && !result.graphics[i].loadOptionsPortrait(path, error)) throw std::runtime_error(error);
            result.graphics[i].setFont(font);
        }
        result.selection.reset(result.layouts);
        result.messages.reset(result.layouts);
        result.options.reset(result.layouts[2]);
        result.details.reset(result.layouts[0]);
        result.help.reset(result.layouts[0]);
        for (const auto* tag : {"ti4a","ti4b","curo","frao","movo","shao"}) {
            auto* pane = result.layouts[1].find(tag);
            if (!pane) throw std::runtime_error("Missing card layout placeholder.");
            pane->visible = false;
        }
        for (const auto* tag : {"frao","movo","ti4a","ti4b","shao"}) {
            auto* pane = result.layouts[2].find(tag);
            if (!pane) throw std::runtime_error("Missing options layout placeholder.");
            pane->visible = false;
        }
        const char* names[]{"k_select", "select", "optdemo1", "optdemo2"};
        result.models.resize(4);
        for (unsigned i = 0; i < 4; ++i) {
            if (!roomarchive::read(archive, std::string(names[i]) + ".mdl", data, error) || !result.models[i].read(data, error) ||
                !roomarchive::read(archive, std::string(names[i]) + ".key", data, error) || !result.animations[i].read(data, error)) throw std::runtime_error(error);
            if (i > 0) {
                std::vector<std::uint8_t> settings;
                if (!roomarchive::read(archive, std::string(names[i]) + ".scd", data, error) ||
                    !roomarchive::read(archive, std::string(names[i]) + ".sco", settings, error) ||
                    !result.environments[i].read(data, settings, error)) throw std::runtime_error(error);
            }
        }
        if (!roomarchive::read(archive, "k_select.clk", data, error) ||
            !result.materialAnimation.read(data, result.models[0].materials.size(), error)) throw std::runtime_error(error);
        result.initialLayouts = result.layouts;
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

void fileselect::Scene::open() {
    if (models.size() != 4) throw std::runtime_error("File-selection resources are not loaded.");
    std::size_t textureCount = 0;
    for (auto& renderer : graphics) {
        renderer.textureOffset = textureCount;
        textureCount += renderer.textures.size();
    }
    for (unsigned i = 0; i < 4; ++i) previewTextures[i] = static_cast<int>(textureCount + i);
    if (gamerenderer::useScene("file-select")) { opened = true; return; }
    std::vector<roommaterials::Texture> textures;
    for (const auto& renderer : graphics) textures.insert(textures.end(), renderer.textures.begin(), renderer.textures.end());
    gamerenderer::setInterfaceTextures(textures);
    const auto indices = gamerenderer::loadPreviews(models);
    for (unsigned i = 0; i < 4; ++i) previewTextures[i] = static_cast<int>(indices[i]);
    opened = true;
    gamerenderer::cacheScene("file-select");
}

void fileselect::Scene::reset() {
    layouts = initialLayouts;
    selection.reset(layouts);
    messages.reset(layouts);
    options.reset(layouts[2]);
    details.reset(layouts[0]);
    help.reset(layouts[0]);
    updates = 0;
}

void fileselect::Scene::update(const State& state, const gamesave::State& saves) {
    if (!opened) throw std::runtime_error("File-selection graphics are not open.");
    std::array<bool, 3> used;
    for (unsigned i = 0; i < 3; ++i) used[i] = gamesave::slotUsed(saves, i);
    selection.update(layouts, state, used);
    messages.update(layouts, state, used);
    options.update(layouts[2], state);
    details.update(layouts[0], state, saves, selection.expanded(state.selection));
    help.update(layouts[0], state);
    bindPreviews(layouts, state, used, previewTextures);
    ++updates;
}

void fileselect::Scene::draw(bool present) {
    if (!opened) throw std::runtime_error("File-selection graphics are not open.");
    materialAnimation.apply(float(updates ? updates - 1 : 0), models[0].materials);
    for (unsigned i = 0; i < 4; ++i) {
        const auto frame = std::fmod(updates * (i == 1 ? 1.0f : 0.5f), float(animations[i].frames));
        gamerenderer::drawPreview(i, models[i], models[i].animate(animations[i].sample(frame), animations[i].flags, trigonometry), environments[i].sample(frame));
    }
    std::vector<startupscene::Picture> pictures;
    for (unsigned i = 0; i < 3; ++i) {
        const auto layer = graphics[i].pictures(layouts[i]);
        pictures.insert(pictures.end(), layer.begin(), layer.end());
    }
    gamerenderer::drawInterface(pictures, present);
}
