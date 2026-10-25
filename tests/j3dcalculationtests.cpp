#include "jlib/j3d/j3dmodelcalculation.h"
#include "jlib/j3d/j3ddrawbuffer.h"
#include "jlib/j3d/j3dmaterialcalculation.h"
#include "jlib/j3d/j3dpixelstate.h"
#include "jlib/j3d/j3dtevcommands.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
    bool near(float a, float b) { return std::abs(a - b) < 0.00001f; }
    void number(j3dmodel::TextureMatrix& texture, unsigned offset, float value) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        for (unsigned i = 0; i < 4; ++i) texture.info[offset + i] = static_cast<std::uint8_t>(bits >> (24 - i * 8));
    }
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Original executable required for transform tables.");
        std::ifstream file(argv[1], std::ios::binary);
        check(bool(file), "Cannot open original executable.");
        const std::vector<std::uint8_t> executable{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        j3dmodel::TransformTables tables(executable);
        using j3dmodel::TevOperation;
        j3dmodel::MaterialTevBlock tev;
        tev.capacity = 1;
        tev.count = 1;
        tev.textures = {9};
        tev.stages.resize(1);
        tev.stages[0].order = {2, 3, 4};
        tev.stages[0].registers = {0xdb, 0x12, 0x34, 0, 0x6c, 0x2b, 0xde, 0};
        tev.stages[0].indirect = {0, 1, 2, 3, 4, 5, 6, 7, 8};
        auto commands = j3dmodel::buildTevCommands(tev);
        check(commands.size() == 9 && commands[0].operation == TevOperation::StageCount &&
            commands[1].operation == TevOperation::Texture && commands[2].operation == TevOperation::Order,
            "Single-stage block emits count, texture and order before arithmetic without overwriting retained constants.");
        check(commands[3].values == std::array<int, 9>{1, 2, 3, 4} &&
            commands[4].values == std::array<int, 9>{10, 3, 1, 1, 3} &&
            commands[5].values == std::array<int, 9>{1, 2, 7, 5} &&
            commands[6].values == std::array<int, 9>{1, 0, 2, 1, 1} &&
            commands[7].values == std::array<int, 9>{2, 3} &&
            commands[8].values == std::array<int, 9>{0, 1, 2, 3, 4, 5, 6, 7, 8},
            "Packed combiner inputs, comparison operations, bias/scale/output, swaps and indirect operands are preserved.");
        tev.count = 0;
        tev.textures[0] = 65535;
        commands = j3dmodel::buildTevCommands(tev);
        check(commands.size() == 1 && commands[0].values[0] == 0, "Zero-stage block still writes its count and skips absent textures.");
        for (unsigned capacity : {2U, 4U, 16U}) {
            tev.capacity = capacity;
            tev.count = 2;
            tev.textures.assign(std::min(capacity, 8U), 65535);
            tev.textures[0] = 11;
            tev.stages.resize(capacity);
            tev.stages[0].colorSelection = 17;
            tev.stages[1].alphaSelection = 23;
            tev.colors[0] = {0xffff, 0x8000, 0x7fff, 1023};
            tev.constants[0] = {1, 2, 3, 4};
            tev.swaps[0] = 0xe4;
            commands = j3dmodel::buildTevCommands(tev);
            check(commands[0].operation == (capacity == 16 ? TevOperation::Texture : TevOperation::StageCount),
                "Sixteen-stage blocks load textures before count; smaller blocks load count first.");
            check(commands[4].operation == TevOperation::ColorRegister && commands[4].index == 1 &&
                commands[4].values == std::array<int, 9>{-1, -32768, 32767, 1023} && commands[7].index == 4,
                "Original signed color writes retain register IDs one through four without remapping the fourth write.");
            check(commands[commands.size() - 6].operation == TevOperation::ConstantColor &&
                commands[commands.size() - 5].operation == TevOperation::ConstantAlpha &&
                commands[commands.size() - 4].values == std::array<int, 9>{3, 2, 1, 0},
                "Sentinel selectors emit no replacement, color selectors precede alpha, and swap tables decode in order.");
        }
        j3dmodel::PixelState pixel;
        j3dmodel::MaterialPixelBlock pixelBlock;
        for (unsigned first = 0; first < 8; ++first) {
            for (unsigned operation = 0; operation < 4; ++operation) {
                for (unsigned second = 0; second < 8; ++second) {
                    pixelBlock.alpha = static_cast<std::uint16_t>(first * 32 + operation * 8 + second);
                    pixelBlock.alphaReferences = {19, 201};
                    j3dmodel::loadPixelState(pixelBlock, pixel);
                    check(pixel.alpha == std::array<std::uint8_t, 5>{static_cast<std::uint8_t>(first), 19,
                        static_cast<std::uint8_t>(operation), static_cast<std::uint8_t>(second), 201},
                        "Every alpha table entry preserves both comparisons, operation and references.");
                }
            }
        }
        for (unsigned enabled = 0; enabled < 2; ++enabled) {
            for (unsigned comparison = 0; comparison < 8; ++comparison) {
                for (unsigned write = 0; write < 2; ++write) {
                    pixelBlock.depth = static_cast<std::uint16_t>(enabled * 16 + comparison * 2 + write);
                    j3dmodel::loadPixelState(pixelBlock, pixel);
                    check(pixel.depth == std::array<std::uint8_t, 3>{static_cast<std::uint8_t>(enabled),
                        static_cast<std::uint8_t>(comparison), static_cast<std::uint8_t>(write)},
                        "Every depth table entry preserves enable, comparison and write state.");
                }
            }
        }
        pixelBlock.alpha = pixelBlock.depth = 65535;
        pixelBlock.blend = {2, 3, 6, 9};
        pixel.dither = 1;
        pixel.fog = std::array<std::uint8_t, 44>{42};
        const auto savedAlpha = pixel.alpha;
        const auto savedDepth = pixel.depth;
        j3dmodel::loadPixelState(pixelBlock, pixel);
        check(pixel.alpha == savedAlpha && pixel.depth == savedDepth && pixel.blend == pixelBlock.blend &&
            pixel.dither == 1 && (*pixel.fog)[0] == 42, "Full sentinel fields retain state while blend always loads.");
        pixelBlock.kind = j3dmodel::MaterialPixelKind::TextureEdge;
        j3dmodel::loadPixelState(pixelBlock, pixel);
        check(pixel.alpha == std::array<std::uint8_t, 5>{6, 128, 0, 3, 255} && !pixel.depthLocation && pixel.depth[2] == 1,
            "Texture-edge preset retains original alpha thresholds and late depth testing.");
        pixelBlock.kind = j3dmodel::MaterialPixelKind::Translucent;
        j3dmodel::loadPixelState(pixelBlock, pixel);
        check(pixel.blend == std::array<std::uint8_t, 4>{1, 4, 5, 3} && pixel.depth[2] == 0 && pixel.depthLocation == 1,
            "Translucent preset blends without writing depth.");
        pixelBlock.kind = j3dmodel::MaterialPixelKind::Opaque;
        j3dmodel::loadPixelState(pixelBlock, pixel);
        check(pixel.depth[2] == 1 && pixel.blend[0] == 0 && pixel.dither == 1 && (*pixel.fog)[0] == 42,
            "Preset loads restore opaque state without clearing retained fog or dither.");
        j3dmodel::Material material;
        material.texture = std::make_unique<j3dmodel::MaterialTextureBlock>();
        material.texture->matrices[9] = std::make_unique<j3dmodel::TextureMatrix>();
        auto& texture = *material.texture->matrices[9];
        number(texture, 16, 1);
        number(texture, 20, 1);
        for (unsigned i = 0; i < 4; ++i) number(texture, 36 + i * 20, 1);
        const j3dmodel::Matrix textureJoint{1, 0, 0, 7, 0, 1, 0, 11, 0, 0, 1, 13};
        const j3dmodel::Matrix textureCamera{2, 0, 0, 17, 0, 3, 0, 19, 0, 0, 4, 23};
        texture.effectMatrix.fill(99);
        j3dmodel::calculateMaterial(material, textureJoint, textureCamera, 0, tables);
        check(texture.matrix == j3dmodel::Matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0} && texture.effectMatrix[0] == 99,
            "Ordinary texture transforms ignore retained effect storage and process slots beyond the active coordinate count.");
        for (unsigned mode = 1; mode <= 3; ++mode) {
            texture.info[1] = static_cast<std::uint8_t>(mode);
            j3dmodel::calculateMaterial(material, textureJoint, textureCamera, 0, tables);
            check(texture.matrix[3] == (mode == 1 ? 0 : mode == 2 ? 7 : 31),
                "Texture modes select translation-free camera/joint, joint, or full camera/joint transforms.");
            j3dmodel::calculateMaterial(material, textureJoint, textureCamera, 12, tables);
            check(texture.matrix[3] == (mode == 3 ? 17 : 0) && texture.matrix[0] == (mode == 2 ? 1 : 2),
                "Both original system bits together select camera-only or identity transforms.");
        }
        texture.info[1] = 5;
        texture.effectMatrix = textureJoint;
        j3dmodel::calculateMaterial(material, textureJoint, textureCamera, 0, tables);
        check(texture.matrix == textureJoint, "Basic mode five retains the effect matrix.");
        material.texture->full = true;
        j3dmodel::calculateMaterial(material, textureJoint, textureCamera, 0, tables);
        check(texture.matrix[3] == 0 && texture.matrix[7] == 0 && texture.matrix[11] == 0,
            "Full mode five strips joint translation.");
        texture.effectMatrix = textureCamera;
        j3dmodel::calculateMaterial(material, textureJoint, textureCamera, 12, tables);
        check(texture.matrix == textureCamera, "Full mode five retains effect storage when both system flags are set.");
        texture.info[1] = 4;
        number(texture, 36 + 3 * 4, 5);
        j3dmodel::calculateMaterial(material, textureJoint, textureCamera, 0, tables);
        check(texture.matrix[3] == 5 && texture.matrix[0] == 1, "Mode four applies the embedded projection without the effect matrix.");
        texture.info[1] = 0;
        texture.info[24] = 0x40;
        number(texture, 4, 0.5f);
        number(texture, 8, 0.25f);
        number(texture, 16, 2);
        number(texture, 20, 3);
        number(texture, 28, 4);
        number(texture, 32, 5);
        j3dmodel::calculateTexture(texture, tables);
        check(near(texture.matrix[1], -2) && near(texture.matrix[4], 3) && near(texture.matrix[3], 5) &&
            near(texture.matrix[7], 3.75f), "Texture SRT uses original angle tables and pivot arithmetic.");
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
