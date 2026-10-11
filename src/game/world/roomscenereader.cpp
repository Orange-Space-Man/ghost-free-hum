#include "game/world/roomscenereader.h"
#include "resources/roomarchivereader.h"
#include "resources/roomdiscreader.h"
#include "resources/gameassetpackage.h"
#include "game/world/roomobjectplacement.h"
#include "game/world/roomdoorplacement.h"
#include "game/world/roommodelpose.h"

#include <cmath>
#include <algorithm>
#include <map>
#include <stdexcept>

bool roomscene::read(const std::filesystem::path& disc, Scene& scene, std::string& error, unsigned room) {
    scene = {};
    error.clear();
    try {
        if (room >= 74) throw std::runtime_error("Invalid mansion room number.");
        gameassets::Package package;
        const bool packaged = disc.extension() == ".ppak";
        if (packaged && !package.open(disc, error)) throw std::runtime_error(error);
        const auto readFile = [&](const std::string& path, std::vector<std::uint8_t>& bytes) {
            return packaged ? package.read("files/" + path, bytes, error) : roomdisc::readFile(disc, path, bytes, error);
        };
        const std::string roomName = "room_" + std::string(room < 10 ? "0" : "") + std::to_string(room);
        Scene result;
        std::vector<std::uint8_t> roomArchive;
        std::vector<std::uint8_t> data;
        if (!readFile("Iwamoto/map2/" + roomName + ".arc", roomArchive) ||
            !roomarchive::read(roomArchive, "room.bin", data, error) ||
            !roomgeometry::decode(data, result.mesh, error) ||
            !roommaterials::read(data, result.mesh, result.materials, error)) {
            throw std::runtime_error(error);
        }
        std::vector<std::uint8_t> compressed;
        std::vector<std::uint8_t> mapArchive;
        std::vector<roomobjects::Placement> placements;
        if (!readFile("Map/map2.szp", compressed) ||
            !roomarchive::decompress(compressed, mapArchive, error) ||
            !roomarchive::read(mapArchive, "jmp/furnitureinfo", data, error) ||
            !roomobjects::read(data, room, placements, error)) {
            throw std::runtime_error(error);
        }
        struct Model {
            roomgeometry::Mesh mesh;
            std::size_t materialBase = 0;
        };
        std::map<std::string, Model> models;
        std::size_t textureBytes = 0;
        for (const auto& texture : result.materials.textures) textureBytes += texture.pixels.size();
        const auto append = [&](const std::vector<std::uint8_t>& archive, const std::string& path,
            const roomobjects::Placement& placement, const std::vector<std::uint8_t>& animation) {
            auto found = models.find(path);
            if (found == models.end()) {
                Model model;
                roommaterials::Collection materials;
                bool missing = false;
                if (!roomarchive::read(archive, path, data, error, &missing)) {
                    if (missing && animation.empty()) {
                        error.clear();
                        return false;
                    }
                    throw std::runtime_error(error);
                }
                std::vector<roommodel::Node> pose;
                if (!animation.empty()) {
                    roommodel::Model description;
                    if (!roommodel::inspect(data, description, error)) throw std::runtime_error(error);
                    std::size_t nodes = 0;
                    for (const auto& node : description.nodes) nodes = std::max(nodes, static_cast<std::size_t>(node.index) + 1);
                    if (!roompose::readInitial(animation, nodes, pose, error)) throw std::runtime_error(error);
                }
                if (!roomgeometry::decode(data, model.mesh, error, pose) ||
                    !roommaterials::read(data, model.mesh, materials, error)) {
                    throw std::runtime_error(placement.model + ": " + error);
                }
                model.materialBase = result.materials.materials.size();
                if (model.materialBase + materials.materials.size() > 65536) throw std::runtime_error("Scene exceeds the material limit.");
                const auto textureBase = result.materials.textures.size();
                for (auto& material : materials.materials) {
                    material.texture += textureBase;
                    result.materials.materials.push_back(material);
                }
                for (auto& texture : materials.textures) {
                    textureBytes += texture.pixels.size();
                    if (textureBytes > 256 * 1024 * 1024) throw std::runtime_error("Scene exceeds the texture memory limit.");
                    result.materials.textures.push_back(std::move(texture));
                }
                found = models.emplace(path, std::move(model)).first;
            }
            const auto& model = found->second;
            if (model.mesh.triangles.size() > 2000000 - result.mesh.triangles.size()) throw std::runtime_error("Scene exceeds the triangle limit.");
            for (auto triangle : model.mesh.triangles) {
                triangle.material = static_cast<std::uint16_t>(triangle.material + model.materialBase);
                for (auto& position : triangle.positions) {
                    position = roomobjects::transform(position, placement);
                    for (float component : position) {
                        if (!std::isfinite(component)) throw std::runtime_error("Invalid placed object position.");
                    }
                }
                result.mesh.triangles.push_back(triangle);
            }
            result.mesh.batches += model.mesh.batches;
            return true;
        };
        for (const auto& placement : placements) {
            if (append(roomArchive, placement.model + ".bin", placement, {})) ++result.objects;
        }
        std::vector<std::uint8_t> executable;
        const bool executableRead = packaged ? package.read("sys/main.dol", executable, error) : roomdisc::readExecutable(disc, executable, error);
        if (!executableRead || !roomdoors::read(executable, room, placements, error) ||
            !readFile("Game/game.szp", compressed) || !roomarchive::decompress(compressed, mapArchive, error)) {
            throw std::runtime_error(error);
        }
        std::vector<std::uint8_t> doorPose;
        if (!roomarchive::read(mapArchive, "iwamoto/door/pull.anm", doorPose, error)) throw std::runtime_error(error);
        for (const auto& placement : placements) {
            append(mapArchive, placement.model, placement, doorPose);
            ++result.doors;
        }
        scene = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
