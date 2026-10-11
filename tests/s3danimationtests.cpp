#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/models/s3danimation.h"
#include "game/models/s3dmodel.h"
#include "game/startup/startupresources.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <cstring>

namespace {
    void require(bool condition, const std::string& message) {
        if (!condition) throw std::runtime_error(message);
    }

    void put(std::vector<std::uint8_t>& data, std::size_t offset, unsigned value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i) data.at(offset + i) = static_cast<std::uint8_t>(value >> ((bytes - i - 1) * 8));
    }

    std::vector<std::uint8_t> fixture() {
        std::vector<std::uint8_t> data(122);
        put(data, 2, 1, 2);
        put(data, 4, 60, 2);
        put(data, 12, 32, 4);
        put(data, 16, 56, 4);
        put(data, 20, 68, 4);
        put(data, 24, 68, 4);
        put(data, 28, 104, 4);
        put(data, 36, 0x3f800000, 4);
        put(data, 44, 0x42700000, 4);
        put(data, 48, 0x40400000, 4);
        put(data, 62, 60, 2);
        put(data, 64, 512, 2);
        put(data, 104, 2, 2);
        put(data, 110, 2, 2);
        return data;
    }
}

int main(int argc, char** argv) {
    try {
        s3danimation::Animation animation;
        std::string error;
        const auto data = fixture();
        require(animation.read(data, error), error);
        auto pose = animation.sample(30);
        require(pose.size() == 1 && std::abs(pose[0].scale[0] - 2) < 0.00001f && pose[0].rotation[0] == 4096,
            "S3D cubic interpolation and rotation units.");
        require(pose[0].scale[1] == 1 && pose[0].translation[0] == 0, "S3D empty channel defaults.");
        require(animation.sample(-1)[0].scale[0] == 1 && animation.sample(61)[0].rotation[0] == 8192, "S3D key endpoints.");
        auto negative = data;
        put(negative, 64, 0xfe00, 2);
        require(animation.read(negative, error) && animation.sample(30)[0].rotation[0] == -4096, "Signed S3D rotation.");
        auto split = data;
        split.insert(split.begin() + 56, 8, 0);
        put(split, 16, 64, 4);
        put(split, 20, 76, 4);
        put(split, 24, 76, 4);
        put(split, 28, 112, 4);
        put(split, 44, 0x3f800000, 4);
        put(split, 48, 0x42700000, 4);
        put(split, 52, 0x40400000, 4);
        put(split, 112, 0x8002, 2);
        require(animation.read(split, error) && std::abs(animation.sample(30)[0].scale[0] - 2.125f) < 0.00001f,
            "Separate incoming and outgoing S3D tangents.");
        auto duplicate = data;
        put(duplicate, 44, 0, 4);
        require(!animation.read(duplicate, error), "Accepted duplicate key times.");
        require(animation.read(data, error), error);
        for (std::size_t length = 0; length < data.size(); ++length) {
            const std::vector<std::uint8_t> truncated(data.begin(), data.begin() + length);
            require(!animation.read(truncated, error), "Accepted truncated key animation.");
        }
        require(animation.sample(30)[0].rotation[0] == 4096, "Failed read changed loaded animation.");
        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> compressed, archive, key;
            require(package.open(argv[1], error), error);
            require(package.read("files/Kawano/ENGLISH/res_slct.szp", compressed, error), error);
            require(roomarchive::decompress(compressed, archive, error), error);
            std::vector<std::uint8_t> dol, table;
            require(package.read("sys/main.dol", dol, error), error);
            require(dolresources::read(dol, glme01::sineTable, 5120 * 4, table, error), error);
            std::vector<float> trigonometry;
            for (std::size_t i = 0; i < table.size(); i += 4) {
                const std::uint32_t bits = (table[i] << 24) | (table[i+1] << 16) | (table[i+2] << 8) | table[i+3];
                float value;
                std::memcpy(&value, &bits, 4);
                trigonometry.push_back(value);
            }
            for (const auto* name : {"k_select.key", "select.key", "optdemo1.key", "optdemo2.key"}) {
                require(roomarchive::read(archive, name, key, error), error);
                require(animation.read(key, error), std::string(name) + ": " + error);
                std::vector<std::uint8_t> modelData;
                const auto modelName = std::string(name).substr(0, std::string(name).size() - 4) + ".mdl";
                s3dmodel::Model model;
                require(roomarchive::read(archive, modelName, modelData, error), error);
                if (!model.read(modelData, error)) throw std::runtime_error(modelName + ": " + error);
                std::size_t nodes = 0;
                for (float frame = 0; frame < animation.frames; frame += 0.5f) {
                    pose = animation.sample(frame);
                    nodes = pose.size();
                    for (const auto& node : pose) {
                        for (float value : node.scale) require(std::isfinite(value), "Invalid scale sample.");
                        for (float value : node.translation) require(std::isfinite(value), "Invalid translation sample.");
                    }
                    for (const auto& batch : model.animate(pose, animation.flags, trigonometry)) {
                        for (const auto& vertex : batch.vertices) {
                            for (float value : vertex.normal) require(std::isfinite(value), "Invalid animated normal.");
                        }
                    }
                }
                std::size_t triangles = 0;
                const auto animated = model.animate(animation.sample(10), animation.flags, trigonometry);
                for (const auto& batch : animated) {
                    triangles += batch.vertices.size() / 3;
                    for (const auto& vertex : batch.vertices) for (float value : vertex.position) require(std::isfinite(value), "Invalid skinned position.");
                }
                std::cout << name << ": " << nodes << " joints, " << animation.frames << " frames, " << triangles << " triangles\n";
            }
        }
        std::cout << "S3D animation checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
