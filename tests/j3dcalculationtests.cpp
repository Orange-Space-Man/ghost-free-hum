#include "jlib/j3d/j3dmodelcalculation.h"
#include "jlib/j3d/j3ddrawbuffer.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
    bool near(float a, float b) { return std::abs(a - b) < 0.00001f; }
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Original executable required for transform tables.");
        std::ifstream file(argv[1], std::ios::binary);
        check(bool(file), "Cannot open original executable.");
        const std::vector<std::uint8_t> executable{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        j3dmodel::TransformTables tables(executable);
        auto data = std::make_shared<j3dmodel::ModelData>();
        data->calculator = std::make_unique<j3dmodel::MatrixCalculator>(0);
        data->geometry.joints.resize(3);
        data->geometry.hierarchy.root = 0;
        data->geometry.hierarchy.joints = {{1, -1, -1}, {-1, 2, -1}, {-1, -1, -1}};
        data->jointCalculators.resize(3);
        for (unsigned i = 0; i < 3; ++i) {
            auto& joint = data->geometry.joints[i];
            joint.index = i;
            joint.scale = {1, 1, 1};
        }
        auto& root = data->geometry.joints[0];
        root.scale = {2, 1, 1}; root.translation = {1, 2, 3};
        data->geometry.joints[1].scale = {0.5f, 1, 1};
        data->geometry.joints[1].translation = {5, 0, 0};
        data->geometry.joints[2].scale = {3, 1, 1};
        data->geometry.joints[2].translation = {7, 0, 0};
        data->geometry.drawMatrices = {{0, 2}, {0, 0}, {0, 1}};
        j3dmodel::ModelInstance instance(data, 0x10000);
        instance.matrix[3] = 10;
        j3dmodel::calculateBasic(instance, tables);
        check(instance.jointMatrices[0][0] == 2 && instance.jointMatrices[0][3] == 11,
            "Root calculation combines model and original joint transform.");
        check(instance.jointMatrices[1][0] == 2 && instance.jointMatrices[1][3] == 21 && instance.jointScaleFlags[1] == 1,
            "Original basic calculator skips local scale when cumulative scale cancels to one.");
        check(instance.jointMatrices[2][0] == 6 && instance.jointMatrices[2][3] == 25 && instance.jointScaleFlags[2] == 0,
            "Sibling traversal restores the parent's matrix and cumulative scale.");
        const j3dmodel::Matrix camera{1, 0, 0, 100, 0, 1, 0, 200, 0, 0, 1, 300};
        j3dmodel::calculateViewMatrices(instance, camera);
        auto* draws = reinterpret_cast<j3dmodel::Matrix*>(instance.drawMatrices.data());
        auto* normals = reinterpret_cast<j3dmodel::NormalMatrix*>(instance.normalMatrices.data());
        check(draws[0][3] == 125 && draws[1][3] == 111 && draws[2][3] == 121,
            "Draw matrix indirection preserves source order and applies the active camera.");
        check(near(normals[0][0], 1.0f / 6) && normals[1][0] == 0.5f && normals[2][0] == 2,
            "Normal matrices invert scaled sources but copy sources marked exactly one.");
        instance.field8 = 1;
        j3dmodel::calculateViewMatrices(instance, camera);
        check(draws[0] == instance.jointMatrices[2], "Model flag one bypasses camera concatenation.");
        instance.jointMatrices[2] = {};
        normals[0].fill(42);
        j3dmodel::calculateViewMatrices(instance, camera);
        check(normals[0] == j3dmodel::NormalMatrix{42, 42, 42, 42, 42, 42, 42, 42, 42},
            "Singular inverse transpose preserves previous normal storage.");
        instance.scale = {2, 3, 4};
        root.scale = {1, 1, 1};
        j3dmodel::calculateBasic(instance, tables);
        check(instance.jointMatrices[0][0] == 2 && instance.jointMatrices[0][5] == 3 &&
            instance.jointMatrices[0][10] == 4 && instance.jointMatrices[0][3] == 12 &&
            instance.jointMatrices[0][7] == 6 && instance.jointMatrices[0][11] == 12,
            "Initialization scales basis columns, retains model translation, and scales subsequent joint translation.");
        j3dmodel::Joint rotated;
        rotated.rotation[2] = 0x4000;
        const auto rotation = tables.transform(rotated);
        check(near(rotation[0], 0) && near(rotation[1], -1) && near(rotation[4], 1) && near(rotation[10], 1),
            "Original lookup tables produce the signed quarter-turn transform.");
        data->geometry.envelopes.resize(1);
        data->geometry.drawMatrices = {{1, 0}, {0, 1}};
        j3dmodel::ModelInstance weighted(data, 0x10000);
        weighted.envelopeMatrices[0] = {2, 0, 0, 3, 0, 4, 0, 5, 0, 0, 8, 7};
        weighted.envelopeScaleFlags[0] = 0;
        weighted.jointMatrices[1] = {1, 0, 0, 4, 0, 1, 0, 6, 0, 0, 1, 8};
        weighted.jointScaleFlags[1] = 1;
        j3dmodel::calculateViewMatrices(weighted, camera);
        const auto* weightedDraws = reinterpret_cast<j3dmodel::Matrix*>(weighted.drawMatrices.data());
        const auto* weightedNormals = reinterpret_cast<j3dmodel::NormalMatrix*>(weighted.normalMatrices.data());
        check(weightedDraws[0][3] == 103 && weightedDraws[1][3] == 104 && weightedNormals[0][0] == 0.5f &&
            weightedNormals[0][4] == 0.25f && weightedNormals[0][8] == 0.125f,
            "View calculation selects each weighted or joint source independently.");
        bool rejected = false;
        try { j3dmodel::calculateBasic(weighted, tables); }
        catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "Missing weighted-envelope calculation cannot silently become a rigid model.");
        j3dmodel::DrawBuffer buffer(8);
        std::array<j3dmodel::MaterialPacket, 4> packets;
        std::array<j3dmodel::ShapePacket, 4> shapes;
        for (unsigned i = 0; i < packets.size(); ++i) {
            packets[i].shapes = &shapes[i];
            packets[i].field34 = i == 1 ? 0 : i;
        }
        check(buffer.entryMaterial(packets[0], 0x80200002) && !buffer.entryMaterial(packets[1], 0x80200002),
            "Equal nonnegative material identities merge in their original texture-address bucket.");
        check(buffer.packets[2] == &packets[0] && packets[0].shapes == &shapes[1] && shapes[1].next == &shapes[0],
            "Merged shapes prepend without replacing the admitted material packet.");
        packets[2].field34 = packets[3].field34 = 0xffffffff;
        check(buffer.entryMaterial(packets[2], 2) && buffer.entryMaterial(packets[3], 2) &&
            packets[3].next == &packets[2] && packets[2].next == &packets[0],
            "Negative identities never merge and bucket collisions prepend.");
        buffer.clear();
        for (unsigned i = 0; i < packets.size(); ++i) packets[i].shapes = &shapes[i];
        buffer.nearPlane = 1;
        buffer.farPlane = 80;
        buffer.interval = 10;
        j3dmodel::Matrix joint{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, -25};
        const j3dmodel::Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        buffer.matrix = &joint;
        buffer.entryDepth(packets[0], identity);
        joint[11] = 0;
        buffer.entryDepth(packets[1], identity);
        joint[11] = -81;
        buffer.entryDepth(packets[2], identity);
        check(buffer.packets[5] == &packets[0] && buffer.packets[7] == &packets[1] && buffer.packets[0] == &packets[2],
            "Depth admission preserves near/far clamps, division without near subtraction, and reverse buckets.");
        std::vector<j3dmodel::MaterialPacket*> order;
        buffer.visit([&](auto& packet) { order.push_back(&packet); });
        check(order == std::vector<j3dmodel::MaterialPacket*>{&packets[2], &packets[0], &packets[1]},
            "Forward draw traversal visits buckets in ascending order.");
        order.clear();
        buffer.drawType = 1;
        buffer.visit([&](auto& packet) { order.push_back(&packet); });
        check(order == std::vector<j3dmodel::MaterialPacket*>{&packets[1], &packets[0], &packets[2]},
            "Reverse draw traversal reverses buckets without reversing their links.");
        buffer.clear();
        buffer.entryUnsorted(packets[0]);
        buffer.entryUnsorted(packets[1]);
        check(buffer.packets[0] == &packets[1] && packets[1].next == &packets[0] && !packets[0].next && !shapes[1].next,
            "Unsorted admission clears previous links and prepends to bucket zero.");
        check(!buffer.entryCallbackMaterial(packets[0]) && !buffer.entryCallbackShape(packets[1]),
            "Callback admission rejects missing callback owners after clearing packet links.");
        j3dmodel::MaterialPacket callback;
        buffer.callback = &callback;
        check(buffer.entryCallbackMaterial(packets[0]) && buffer.entryCallbackMaterial(packets[1]) &&
            callback.child == &packets[1] && packets[1].next == &packets[0],
            "Material callback admission prepends complete material packets.");
        callback.child = nullptr;
        check(buffer.entryCallbackShape(packets[0]) && buffer.entryCallbackShape(packets[1]) &&
            callback.child == &shapes[1] && shapes[1].next == &shapes[0],
            "Shape callback admission prepends shapes to its owner without merging materials.");
        std::cout << "Original basic joint and camera matrix checks passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
