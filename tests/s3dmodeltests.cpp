#include "game/models/s3dmodel.h"

#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    void put(std::vector<std::uint8_t>& data, unsigned offset, unsigned value, unsigned size) {
        for (unsigned i = 0; i < size; ++i) data.at(offset + i) = static_cast<std::uint8_t>(value >> ((size - i - 1) * 8));
    }

    std::vector<std::uint8_t> fixture() {
        std::vector<std::uint8_t> data(536, 0);
        put(data, 0, 0x04b40000, 4);
        put(data, 8, 2, 2);
        put(data, 10, 1, 2);
        put(data, 16, 1, 2);
        put(data, 38, 1, 2);
        put(data, 40, 1, 2);
        put(data, 42, 1, 2);
        put(data, 48, 128, 4);
        put(data, 52, 160, 4);
        put(data, 72, 192, 4);
        put(data, 104, 204, 4);
        put(data, 112, 492, 4);
        put(data, 116, 500, 4);
        put(data, 130, 1, 2);
        put(data, 144, 1, 2);
        put(data, 152, 1, 2);
        put(data, 160, 504, 4);
        put(data, 164, 32, 4);
        put(data, 170, 1, 2);
        put(data, 172, 1, 2);
        put(data, 192, 0x3f800000, 4);
        put(data, 204, 0xffffffff, 4);
        put(data, 496, 1, 2);
        data[504] = 0x90;
        put(data, 505, 3, 2);
        return data;
    }
}

int main() {
    try {
        const auto data = fixture();
        s3dmodel::Model model;
        std::string error;
        require(model.read(data, error), error.c_str());
        require(model.batches.size() == 1 && model.batches[0].vertices.size() == 3, "Triangle display list.");
        std::vector<float> table(5120);
        for (unsigned i = 0; i < table.size(); ++i) table[i] = std::sin(i * 6.283185307179586f / 4096);
        std::vector<s3danimation::Pose> pose(2);
        pose[0].scale = {2,3,4};
        pose[0].translation = {10,0,0};
        pose[1].translation = {5,0,0};
        model.batches[0].vertices[0].normal = {0,0,1};
        const auto animated = model.animate(pose, 0, table);
        require(std::abs(animated[0].vertices[0].position[0] - 22) < 0.0001f, "Inherited scale and translation.");
        require(std::abs(animated[0].vertices[0].normal[2] - 0.25f) < 0.0001f &&
            animated[0].vertices[0].normal[0] == 0, "Inverse-transpose normal ignores translation.");
        pose[1].rotation[2] = 16384;
        const auto compensated = model.animate(pose, 0, table);
        const auto ordinary = model.animate(pose, 2, table);
        require(std::abs(compensated[0].vertices[0].position[1] - 2) < 0.0001f &&
            std::abs(ordinary[0].vertices[0].position[1] - 3) < 0.0001f, "Original scale compensation flag.");
        auto invalid = data;
        put(invalid, 146, 0xffff, 2);
        require(!model.read(invalid, error), "Accepted cyclic hierarchy.");
        invalid = data;
        put(invalid, 130, 0xffff, 2);
        require(!model.read(invalid, error), "Accepted negative root link.");
        invalid = data;
        put(invalid, 172, 0xffff, 2);
        require(!model.read(invalid, error), "Accepted uninitialized palette.");
        invalid = data;
        invalid[507] = 1;
        require(!model.read(invalid, error), "Accepted unaligned matrix index.");
        invalid = data;
        put(invalid, 510, 1, 2);
        require(!model.read(invalid, error), "Accepted invalid position index.");
        for (std::size_t length = 0; length < data.size(); ++length) {
            require(!model.read({data.begin(), data.begin() + length}, error), "Accepted truncated model.");
        }
        require(model.batches.size() == 1 && model.joints.size() == 2, "Failed read changed model.");
        auto weighted = data;
        weighted.resize(645);
        put(weighted, 12, 1, 2);
        put(weighted, 56, 536, 4);
        put(weighted, 60, 632, 4);
        put(weighted, 64, 640, 4);
        put(weighted, 68, 644, 4);
        for (unsigned joint = 0; joint < 2; ++joint) {
            for (unsigned axis = 0; axis < 3; ++axis) put(weighted, 536 + joint * 48 + axis * 20, 0x3f800000, 4);
        }
        put(weighted, 632, 0x3f000000, 4);
        put(weighted, 636, 0x3f000000, 4);
        put(weighted, 642, 1, 2);
        weighted[644] = 2;
        put(weighted, 172, 2, 2);
        require(model.read(weighted, error), error.c_str());
        pose[1].rotation[2] = 0;
        require(std::abs(model.animate(pose, 0, table)[0].vertices[0].position[0] - 17) < 0.0001f,
            "Weighted envelope matrix blend.");
        auto shadow = data;
        shadow.resize(615);
        put(shadow, 52, 536, 4);
        std::copy(data.begin() + 160, data.begin() + 192, shadow.begin() + 536);
        std::copy(data.begin() + 160, data.begin() + 192, shadow.begin() + 568);
        put(shadow, 568, 600, 4);
        put(shadow, 572, 15, 4);
        put(shadow, 576, 2, 2);
        shadow[600] = 0x90;
        put(shadow, 601, 3, 2);
        for (unsigned i = 0; i < 3; ++i) shadow[606 + i * 4] = 255;
        require(model.read(shadow, error, true), error.c_str());
        require(model.shadowBatches.size() == 1 && model.shadowBatches[0].vertices.size() == 3 &&
            model.shadowBatches[0].cull == 2 && !model.shadowBatches[0].lighting, "Shadow packet bank layout.");
        require(std::abs(model.animate(pose, 0, table, true)[0].vertices[0].position[0] - 22) < 0.0001f,
            "Shadow packet skinning.");
        shadow.pop_back();
        require(!model.read(shadow, error, true), "Accepted truncated shadow display list.");
        require(model.shadowBatches.size() == 1, "Failed read changed shadow geometry.");
        require(model.read(shadow, error) && model.shadowBatches.empty(), "Optional shadow decoding.");
        s3dmodel::Material material;
        material.color[3] = 0;
        require(s3dmodel::renderMode(material) == 0, "Untextured alpha must preserve source mode.");
        material.texture = 1;
        require(s3dmodel::renderMode(material) == 2, "Textured material alpha selects translucent pass.");
        material.color[3] = 1;
        require(s3dmodel::renderMode(material) == 0, "Opaque alpha restores source mode.");
        material.mode = 1;
        require(s3dmodel::renderMode(material) == 1, "Texture cutout mode.");
        std::cout << "S3D model checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
