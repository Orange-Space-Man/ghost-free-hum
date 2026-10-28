#include "game/world/roomfurnitureresources.h"

#include <algorithm>
#include <stdexcept>

std::vector<std::uint16_t> roomfurniture::selectRows(const jmp::ToolData& table, unsigned room, const gameflags::State& flags) {
    std::vector<std::uint16_t> selected;
    for (unsigned row = 0; row < table.count(); ++row) {
        std::uint32_t number = 0, appear = 0, disappear = 0;
        if (!table.getValue(row, "room_no", number) || !table.getValue(row, "appear_flag", appear) ||
            !table.getValue(row, "disappear_flag", disappear)) throw std::runtime_error("Original furniture admission fields are missing.");
        if (number != (room & 255) || (appear && !flags.value(static_cast<std::uint8_t>(appear))) ||
            (disappear && flags.value(static_cast<std::uint8_t>(disappear)))) continue;
        if (selected.size() == 64) throw std::runtime_error("Furniture selection exceeds the original room scratch capacity.");
        selected.push_back(static_cast<std::uint16_t>(row));
    }
    return selected;
}

void roomfurniture::Resources::load(const gamearchive::Archive& archive, const jmp::ToolData& table, unsigned room,
    const gameflags::State& flags, const dooranimation::PoseTables& poseTables) {
    Resources result;
    const auto selected = selectRows(table, room, flags);
    if (selected.empty()) {
        *this = std::move(result);
        return;
    }
    std::vector<std::string> modelNames;
    std::string error;
    for (const auto row : selected) {
        std::string_view name;
        Placement placement;
        placement.row = row;
        if (!table.getValue(row, "dmd_name", name) || !table.getValue(row, "move", placement.behavior)) {
            throw std::runtime_error("Original furniture model or behavior field is missing.");
        }
        const std::string path = "/" + std::string(name) + ".bin";
        if (path != "/(null).bin") {
            const auto resource = archive.findPathResource(path);
            if (resource < 0) continue;
            const auto found = std::find(modelNames.begin(), modelNames.end(), path);
            if (found == modelNames.end()) {
                placement.model = static_cast<std::uint16_t>(modelNames.size());
                doors::ModelResource model;
                if (!model.read(archive.resource(resource), poseTables, error)) throw std::runtime_error(path + ": " + error);
                modelNames.push_back(path);
                result.modelResources.push_back(std::move(model));
            } else placement.model = static_cast<std::uint16_t>(found - modelNames.begin());
        }
        result.entries.push_back(placement);
    }
    auto files = archive.directory("/Anm");
    files.resize(files.size() > 2 ? files.size() - 2 : 0);
    for (const auto index : files) {
        Animation animation;
        animation.name = archive.entry(index).name;
        if (!animation.animation.read(archive.resource(index), error)) throw std::runtime_error(animation.name + ": " + error);
        auto sound = "/" + animation.name;
        if (sound.size() < 3) throw std::runtime_error("Original furniture animation name is truncated.");
        sound.replace(sound.size() - 3, 3, "bas");
        const auto resource = archive.findPathResource(sound);
        if (resource >= 0) animation.sound = archive.resource(resource);
        result.animationResources.push_back(std::move(animation));
    }
    *this = std::move(result);
}

const std::vector<roomfurniture::Placement>& roomfurniture::Resources::placements() const { return entries; }
const std::vector<doors::ModelResource>& roomfurniture::Resources::models() const { return modelResources; }
const std::vector<roomfurniture::Animation>& roomfurniture::Resources::animations() const { return animationResources; }
