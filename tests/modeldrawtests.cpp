#include "game/models/modeldrawtraversal.h"
#include "game/models/modelvertexstream.h"
#include "game/models/modeldrawmatrices.h"
#include "game/models/modelmaterialstate.h"
#include "game/models/modeltexturestages.h"
#include "game/models/modeltexturematrix.h"
#include "game/models/modeltextureimages.h"

#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    void half(std::vector<std::uint8_t>& bytes, unsigned offset, unsigned value) {
        bytes.at(offset) = static_cast<std::uint8_t>(value >> 8);
        bytes.at(offset + 1) = static_cast<std::uint8_t>(value);
    }

    void word(std::vector<std::uint8_t>& bytes, unsigned offset, unsigned value) {
        half(bytes, offset, value >> 16);
        half(bytes, offset + 2, value);
    }
}

int main() {
    try {
        std::vector<std::uint8_t> bytes(0x900);
        word(bytes, 0x10, 0x60);
        word(bytes, 0x34, 0x100);
        word(bytes, 0x38, 0x200);
        word(bytes, 0x3c, 0x400);
        for (unsigned i = 0; i < 4; ++i) {
            const unsigned node = 0x400 + i * 0x8c;
            word(bytes, node + 2, 0xffffffff);
            half(bytes, node + 0x4c, 1);
            word(bytes, node + 0x50, 0x300 + i * 4);
            half(bytes, 0x700 + i * 4, i);
            half(bytes, 0x702 + i * 4, i);
            half(bytes, 0x108 + i * 0x28, 0xffff);
            word(bytes, 0x20c + i * 0x18, 0x100);
        }
        half(bytes, 0x402, 1);
        half(bytes, 0x404, 2);
        half(bytes, 0x400 + 2 * 0x8c + 2, 3);
        modeldraw::Resource resource;
        std::string error;
        require(resource.read(bytes, error), error.c_str());
        modeldraw::TraversalState state;
        std::vector<modeldraw::DrawCommand> commands;
        const auto submit = [&](const modeldraw::DrawCommand& command) { commands.push_back(command); };
        const auto draw = [&](unsigned pass, unsigned opacity) {
            commands.clear();
            modeldraw::traverse(resource, pass, static_cast<std::uint8_t>(opacity), state, submit);
        };
        draw(0, 255);
        require(commands.size() == 4 && commands[0].node == 2 && commands[1].node == 3 &&
            commands[2].node == 0 && commands[3].node == 1, "Drawing visits siblings and their children before the current node.");
        commands.clear();
        modeldraw::traverse(resource, 0, 255, state, submit, 0, true);
        require(commands.size() == 2 && commands[0].node == 0 && commands[1].node == 1,
            "Single-root mode suppresses only the root sibling traversal.");
        half(bytes, 0x408, 8);
        require(resource.read(bytes, error), error.c_str());
        draw(0, 255);
        require(commands.size() == 2 && commands[0].node == 2 && commands[1].node == 3,
            "Translucent-only node suppresses its children on opaque passes, not its siblings.");
        draw(2, 255);
        require(commands.size() == 2 && commands[0].node == 0 && commands[1].node == 1 &&
            commands[0].opacity == 100 && commands[1].opacity == 100, "Opacity cap is inherited by children.");
        half(bytes, 0x408, 4);
        require(resource.read(bytes, error), error.c_str());
        state.flaggedOpacity = 0;
        draw(0, 255);
        require(commands.size() == 2, "Zero flagged opacity suppresses the node and its descendants.");
        state.overrideOpacity = true;
        draw(0, 255);
        require(commands.size() == 4, "Override bypasses node opacity suppression.");
        state.overrideOpacity = false;
        state.flaggedOpacity = 170;
        draw(2, 255);
        require(commands.size() == 2 && commands.back().opacity == 170, "Flagged opacity replaces incoming opacity and reaches children.");
        half(bytes, 0x408, 0xc0);
        require(resource.read(bytes, error), error.c_str());
        state.nodeLightingOverride = true;
        state.limitedOpacity = 80;
        state.lightingFlags = 5;
        draw(2, 140);
        require(commands.size() == 4 && commands[2].opacity == 80 && commands[2].lightingFlags == 7 &&
            commands[3].opacity == 80 && commands[3].lightingFlags == 5 && state.lightingFlags == 5,
            "Lighting bit changes persist in traversal order while preserving unrelated bits.");
        half(bytes, 0x408, 0x10);
        require(resource.read(bytes, error), error.c_str());
        draw(0, 255);
        require(commands.size() == 3 && commands.back().node == 1, "Normal-mode hidden batches retain child traversal.");
        state.mode = 1;
        draw(0, 255);
        require(commands.empty(), "Alternate mode requires node bit two.");
        half(bytes, 0x408, 2);
        require(resource.read(bytes, error), error.c_str());
        draw(0, 255);
        require(commands.size() == 1 && commands[0].alternateMaterial, "Opaque alternate mode uses alternate material processing.");
        draw(2, 100);
        require(commands.size() == 1 && !commands[0].alternateMaterial, "Partial opacity keeps normal material processing in alternate mode.");
        bytes[0x102] = 2;
        half(bytes, 0x108 + 0x28, 0);
        bytes[0x66] = 1;
        require(resource.read(bytes, error), error.c_str());
        require(resource.materialPass(0) == 2 && resource.materialPass(1) == 1 && resource.materialPass(2) == 0,
            "Explicit material pass precedes sampler pass, with absent sampler returning zero.");
        state.mode = 0;
        std::vector<std::int16_t> prepared;
        commands.clear();
        modeldraw::traverse(resource, 7, 255, state, submit, 0, false,
            [&](std::int16_t index) { prepared.push_back(index); });
        require(commands.empty() && prepared == std::vector<std::int16_t>{2, 3, 0, 1},
            "Node matrix preparation precedes material rejection, even when no batches match the pass.");
        bytes[0x102] = 255;
        require(resource.read(bytes, error), error.c_str());
        draw(255, 255);
        require(commands.empty(), "Signed material-pass return cannot match an unsigned pass of 255.");
        bytes[0x102] = 2;
        require(resource.read(bytes, error), error.c_str());
        half(bytes, 0x404, 0);
        require(!resource.read(bytes, error) && resource.materialPass(0) == 2,
            "Invalid hierarchy replacement retains the prior draw resource.");
        std::uint8_t cull = 9;
        require(modeldraw::cullMode(1, 1, cull) && cull == 2 && modeldraw::cullMode(2, 1, cull) && cull == 1 &&
            !modeldraw::cullMode(3, 1, cull) && cull == 1, "Alternate culling swaps front/back and preserves state for other values.");

        word(bytes, 0x14, 0x100);
        word(bytes, 0x18, 0x140);
        word(bytes, 0x1c, 0x180);
        for (unsigned i = 0; i < 4; ++i) word(bytes, 0x24 + i * 4, 0x190 + i * 8);
        half(bytes, 0x100, 0xffff);
        half(bytes, 0x102, 2);
        half(bytes, 0x104, 0x8000);
        word(bytes, 0x140, 0x3f800000);
        word(bytes, 0x14c + 4, 0x40000000);
        word(bytes, 0x158 + 8, 0x40400000);
        word(bytes, 0x180, 0x12345678);
        for (unsigned i = 0; i < 4; ++i) word(bytes, 0x190 + i * 8, 0x3f800000 + i * 0x800000);
        modeldraw::Batch batch;
        batch.attributes = 0x1e800;
        batch.normalMode = 1;
        batch.displayList.resize(32);
        batch.displayList[0] = 0xb8;
        half(batch.displayList, 1, 1);
        half(batch.displayList, 7, 1);
        half(batch.displayList, 9, 2);
        std::vector<modeldraw::Primitive> primitives;
        require(modeldraw::decodeVertices(bytes, batch, primitives, error), error.c_str());
        const auto& vertex = primitives.at(0).vertices.at(0);
        require(vertex.position == std::array<float, 3>{-1, 2, -32768} && vertex.normals[0][0] == 1 &&
            vertex.normals[1][1] == 2 && vertex.normals[2][2] == 3 && vertex.color[3] == 0x78 &&
            vertex.coordinates[3][0] == 8, "Vertex decoding retains signed positions, three normal indices, RGBA and four UV streams.");
        word(bytes, 0x48, 2);
        word(bytes, 0x100, 0x3fc00000);
        word(bytes, 0x104, 0xc0200000);
        word(bytes, 0x108, 0x40600000);
        require(modeldraw::decodeVertices(bytes, batch, primitives, error) &&
            primitives[0].vertices[0].position == std::array<float, 3>{1.5f, -2.5f, 3.5f}, "Float position header selects the original 12-byte stride.");
        batch.displayList.resize(4);
        require(!modeldraw::decodeVertices(bytes, batch, primitives, error) && primitives[0].vertices[0].position[0] == 1.5f,
            "Truncated display lists fail without replacing decoded vertices.");
        const dooranimation::Matrix matrix{2, 0, 0, 10, 0, 4, 0, 20, 0, 0, -8, 30};
        dooranimation::Matrix normal{};
        require(modeldraw::normalMatrix(matrix, normal) && normal[0] == 0.5f && normal[5] == 0.25f &&
            normal[10] == -0.125f && normal[3] == 0 && normal[7] == 0 && normal[11] == 0,
            "Original normal matrix applies inverse scale and reflection without translation.");
        const auto retainedNormal = normal;
        require(!modeldraw::normalMatrix({}, normal) && normal == retainedNormal,
            "Singular normal matrices return failure without writing the destination.");
        require(modeldraw::textureMatrix(matrix) == dooranimation::Matrix{2, 0, 0, 0, 0, 4, 0, 0, 0, 0, -8, 0},
            "Texture matrix retains the model-view basis and clears translation.");
        modelmaterial::Material material;
        material.colorSource = 1;
        material.color = {20, 30, 40, 200};
        material.specular = true;
        material.shininess = 12.5f;
        modelmaterial::LightingState lighting;
        lighting.lightMask = 7;
        lighting.attenuation = 1;
        lighting.lightingFlags = 2;
        lighting.channels[1].lightMask = 99;
        unsigned specularCalls = 0;
        const auto specular = [&](float value) {
            require(value == 12.5f, "Specular light receives the material's original shininess.");
            ++specularCalls;
        };
        require(!modelmaterial::configureChannels(material, 100, 0, lighting, specular) && specularCalls == 1 &&
            lighting.specularMask == 0x40 && lighting.channels[1].lightMask == 99 && lighting.color[3] == 100,
            "Specular setup occurs even for unlit batches, preserving untouched channel state.");
        require(modelmaterial::configureChannels(material, 255, 1, lighting, specular) &&
            lighting.color[3] == 200 && lighting.channels[0].enabled == 0 && lighting.channels[1].enabled == 1 &&
            lighting.channels[0].materialSource == 0 && lighting.channels[3].lightMask == 0x80,
            "Node lighting flags suppress primary lighting without suppressing the specular channel.");
        lighting.overrideColor = true;
        lighting.overrideColorValue = {1, 2, 3, 240};
        modelmaterial::configureChannels(material, 10, 1, lighting, specular);
        modelmaterial::configureChannels(material, 255, 1, lighting, specular);
        require(lighting.color[3] == 240 && lighting.overrideColorValue[3] == 240,
            "Per-draw opacity does not mutate the model's stored override color.");
        material.samplers.resize(3);
        material.samplers[0].normalMapping = 1;
        material.samplers[2].normalMapping = 1;
        auto plan = modelmaterial::texturePlan(material, 3, 0);
        require(plan.steps.size() == 3 && plan.steps[0].combine == 0 && plan.steps[1].combine == 2 && !plan.appendNormalMapping,
            "Ordinary textured batches select the first and subsequent original combine modes.");
        plan = modelmaterial::texturePlan(material, 3, 1);
        require(plan.steps.size() == 1 && plan.steps[0].slot == 1 && plan.steps[0].combine == 2 &&
            plan.normalSlot == 2 && plan.appendNormalMapping, "Normal-mapped batches retain the last marked sampler.");
        plan = modelmaterial::texturePlan(material, 0, 1);
        require(plan.untextured && plan.normalSlot == 0 && plan.appendNormalMapping,
            "Zero-texture normal-mode batches preserve the original default slot and later-stage request.");
        plan = modelmaterial::texturePlan(material, 3, 2);
        require(plan.normalSlot == 2 && !plan.appendNormalMapping,
            "Nonzero normal modes filter slots, but only mode one requests the extra stages.");
        word(bytes, 0x34, 0x400);
        word(bytes, 0xc, 0x80);
        word(bytes, 0x10, 0xc0);
        bytes[0x400] = 1;
        word(bytes, 0x403, 0x12345678);
        half(bytes, 0x408, 0);
        half(bytes, 0x40a, 0xffff);
        bytes[0x418] = 1;
        word(bytes, 0x41c, 0x41480000);
        half(bytes, 0xc0, 0);
        bytes[0xc4] = 2;
        bytes[0xc5] = 1;
        bytes[0xc6] = 2;
        bytes[0xc7] = 1;
        half(bytes, 0xc8, 0xffff);
        bytes[0xca] = 1;
        half(bytes, 0x80, 16);
        half(bytes, 0x82, 32);
        bytes[0x84] = 14;
        word(bytes, 0x88, 0x500);
        require(modelmaterial::read(bytes, 0, 2, material, error), error.c_str());
        require(material.color[3] == 0x78 && material.specular && material.shininess == 12.5f &&
            material.samplers[0].imageOffset == 0x580 && material.samplers[0].maximumLod == -1 &&
            material.samplers[0].width == 16 && material.samplers[0].height == 32 && !material.samplers[1].present,
            "Material import preserves relative image offsets, signed LOD and absent sampler slots.");
        require(!modelmaterial::read(bytes, 0, 9, material, error) && material.color[3] == 0x78,
            "Unsupported sampler counts fail without replacing a loaded material.");
        material.samplers.resize(3, material.samplers[0]);
        for (auto& sampler : material.samplers) {
            sampler.present = true;
            sampler.normalMapping = 0;
        }
        material.samplers[1].generatedMatrix = 4;
        auto program = modelmaterial::buildStages(material, 2, 0, true, true);
        require(program.stageCount == 5 && program.coordinateCount == 2 && program.textures.size() == 2 &&
            program.stages[0].color == std::array<unsigned, 4>{15, 10, 8, 15} &&
            program.stages[1].color == std::array<unsigned, 4>{15, 8, 12, 0},
            "Original first/later texture combine inputs precede specular and shadow append stages.");
        require(program.coordinates[1].source == 1 && program.coordinates[1].matrix == 33 &&
            program.coordinates[1].generatedMatrix == 4 && program.stages[2].raster == 5 &&
            program.stages[3].alphaOutput == 2 && program.stages[4].setKonst && program.stages[4].konstAlpha == 28,
            "Generated coordinates and specular/shadow stage registers retain the traced assignments.");
        require(program.stages[0].resetSwap && !program.stages[2].resetSwap && !program.stages[3].setKonst,
            "Stage descriptions preserve unmodified swap and konst state.");
        material.samplers[1].normalMapping = 1;
        program = modelmaterial::buildStages(material, 2, 1, false, false, 2, 1);
        require(program.stageCount == 6 && program.coordinateCount == 4 && program.stages[0].index == 2 &&
            program.coordinates[1].source == 5 && program.coordinates[1].generatedMatrix == -1 &&
            program.coordinates[2].type == 2 && program.coordinates[2].source == 14,
            "Normal mapping uses raw sampler coordinates and derives the bump source from the preceding generated coordinate.");
        require(!program.stages[1].colorClamp && program.stages[1].colorOutput == 2 &&
            program.stages[2].colorOperation == 1 && program.stages[2].colorClamp && !program.stages[2].alphaClamp &&
            program.stages[3].color == std::array<unsigned, 4>{15, 4, 0, 15},
            "Normal mapping retains its unclamped intermediate, subtract stage and final register combination.");
        program = modelmaterial::buildStages(material, 0, 0, false, false);
        require(program.stages.size() == 1 && program.stages[0].raster == 4 && program.coordinates.empty(),
            "Untextured materials use raster color without a generated texture coordinate.");
        bool exceeded = false;
        try { modelmaterial::buildStages(material, 2, 1, true, true, 14); }
        catch (const std::runtime_error&) { exceeded = true; }
        require(exceeded, "Stage overflow reports unsupported data instead of dropping material effects.");
        word(bytes, 0x48, 1);
        word(bytes, 0x40, 0x600);
        word(bytes, 0x44, 0x680);
        half(bytes, 0x41a, 0);
        half(bytes, 0x604, 1);
        half(bytes, 0x606, 0xffff);
        word(bytes, 0x6b0, 1);
        require(modelmaterial::read(bytes, 0, 2, material, error) && material.samplers[0].generatedMatrix == 1 &&
            material.samplers[1].generatedMatrix == -1, "Generated coordinate lookup follows group and matrix flags, preserving the absent sentinel.");
        modelmaterial::TextureTransform transform;
        require(modelmaterial::readTransform(bytes, 1, transform, error) && transform.flags == 1,
            "Generated matrix import follows its resource-relative record.");
        originalmath::FloatTrig unloadedTrig;
        require(modelmaterial::evaluateTransform(transform, matrix, unloadedTrig) == matrix,
            "Untransformed generated coordinates return the supplied basis without requiring trigonometry.");
        require(!modelmaterial::readTransform(bytes, -1, transform, error) && transform.flags == 1,
            "Invalid generated matrix attachment retains the prior record.");
        auto samplerState = modelmaterial::samplerParameters(material.samplers[0]);
        require(samplerState.minimumFilter == 5 && samplerState.maximumFilter == 1 && samplerState.maximumLod == -1 &&
            samplerState.lodBias == -0.3f && !samplerState.biasClamp && !samplerState.edgeLod && !samplerState.anisotropy,
            "Mipmap sampler setup retains original filters, signed LOD and bias.");
        material.samplers[0].mipmapped = 0;
        samplerState = modelmaterial::samplerParameters(material.samplers[0]);
        require(samplerState.minimumFilter == 1 && samplerState.maximumLod == 0 && samplerState.lodBias == 0,
            "Non-mipmap samplers reset both LOD bounds and bias.");
        modelmaterial::Sampler imageSampler;
        imageSampler.present = true;
        imageSampler.width = 8;
        imageSampler.height = 4;
        imageSampler.format = 1;
        imageSampler.mipmapped = 1;
        imageSampler.maximumLod = 3;
        imageSampler.imageOffset = 16;
        std::vector<std::uint8_t> imageBytes(16 + 4 * 32);
        for (unsigned level = 0; level < 4; ++level)
            for (unsigned byte = 0; byte < 32; ++byte) imageBytes[16 + level * 32 + byte] = static_cast<std::uint8_t>(level * 40 + 10);
        modelmaterial::TextureImages images;
        require(modelmaterial::readImages(imageBytes, imageSampler, images, error), error.c_str());
        require(images.levels.size() == 4 && images.encodedSize == 128 && images.offsets[3] == 112 &&
            images.levels[1].width == 4 && images.levels[1].height == 2 && images.levels[3].width == 1 &&
            images.levels[3].height == 1 && images.levels[2].pixels[0] == 90 && images.levels[3].pixels[3] == 130,
            "Mip levels use stored pixels and tile-padded offsets, preserving rectangular dimensions.");
        imageBytes.pop_back();
        require(!modelmaterial::readImages(imageBytes, imageSampler, images, error) && images.levels.size() == 4,
            "Truncated mip chains fail transactionally rather than generating replacement levels.");
        imageSampler.mipmapped = 0;
        require(modelmaterial::readImages(imageBytes, imageSampler, images, error) && images.levels.size() == 1,
            "Non-mipmap images load only their base level regardless of stored maximum LOD.");
        std::cout << "Model hierarchy, opacity inheritance, material passes, culling and vertex streams verified.\n";
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
