#include "jlib/j3d/j3dmaterialloads.h"

#include <cstring>
#include <stdexcept>

void j3dmodel::loadColorBlock(const MaterialColorBlock& block, MaterialLoadTarget& target) {
    if (block.count > 2) throw std::runtime_error("J3D color channels exceed their original storage.");
    for (unsigned i = 0; i < 2; ++i) target.materialColor(i, block.material[i]);
    if (block.lighting)
        for (unsigned i = 0; i < 2; ++i) target.ambientColor(i, block.ambient[i]);
    target.channelCount(block.count);
    constexpr unsigned channels[]{0, 2, 1, 3};
    constexpr unsigned attenuation[]{2, 0, 2, 1};
    for (unsigned i = 0; i < unsigned(block.count) * 2; ++i) {
        const auto bits = block.channels[i];
        target.channel(channels[i], {(bits >> 1) & 1U, (bits >> 6) & 1U, bits & 1U,
            ((bits >> 2) & 15U) | ((bits >> 7) & 240U), (bits >> 7) & 3U, attenuation[(bits >> 9) & 3]});
    }
    if (block.lighting)
        for (unsigned i = 0; i < block.lights.size(); ++i)
            if (block.lights[i]) target.light(i, *block.lights[i]);
    if (block.cull != 255) target.cull(block.cull);
}

void j3dmodel::loadTextureBlock(const MaterialTextureBlock& block, MaterialLoadTarget& target) {
    if (block.count > 8) throw std::runtime_error("J3D texture coordinates exceed their original storage.");
    target.textureCount(block.count);
    for (unsigned i = 0; i < block.count; ++i) target.textureCoordinate(i, block.coordinates[i].info, 0, 125);
    if (block.full) {
        for (unsigned i = 0; i < block.count; ++i) {
            const auto& coordinate = block.postCoordinates[i];
            if (coordinate.matrix != 65535)
                target.textureCoordinate(i, coordinate.info, coordinate.matrix >> 8, coordinate.matrix & 255);
        }
    }
    for (unsigned i = 0; i < block.matrices.size(); ++i)
        if (block.matrices[i]) target.textureMatrix(30 + i * 3, block.matrices[i]->matrix, block.matrices[i]->info[0]);
    if (block.full)
        for (unsigned i = 0; i < block.postMatrices.size(); ++i)
            if (block.postMatrices[i]) target.textureMatrix(64 + i * 3, block.postMatrices[i]->matrix, block.postMatrices[i]->info[0]);
}

void j3dmodel::loadIndirectBlock(const MaterialIndirectBlock& block, MaterialLoadTarget& target) {
    if (!block.full) return;
    if (block.count > block.matrices.size()) throw std::runtime_error("J3D indirect matrix traversal exceeds its original storage.");
    target.indirectCount(block.count);
    for (unsigned i = 0; i < block.count; ++i) {
        const auto& bytes = block.matrices[i];
        std::array<float, 6> matrix;
        for (unsigned j = 0; j < matrix.size(); ++j) {
            const auto offset = j * 4;
            const std::uint32_t bits = (std::uint32_t(bytes[offset]) << 24) | (std::uint32_t(bytes[offset + 1]) << 16) |
                (std::uint32_t(bytes[offset + 2]) << 8) | bytes[offset + 3];
            std::memcpy(&matrix[j], &bits, sizeof(bits));
        }
        target.indirectMatrix(i + 1, matrix, bytes[24] < 128 ? int(bytes[24]) : int(bytes[24]) - 256);
    }
    for (unsigned i = 0; i < block.count; ++i) target.indirectScale(i, block.scales[i]);
    for (unsigned i = 0; i < block.count; ++i) target.indirectOrder(i, block.orders[i]);
}

void j3dmodel::loadMaterialBlocks(const Material& material, MaterialLoadTarget& target) {
    if (!material.tev || !material.indirect || !material.pixel || !material.color || !material.texture)
        throw std::runtime_error("J3D material loading requires all original material blocks.");
    for (const auto& command : buildTevCommands(*material.tev)) target.tev(command);
    loadIndirectBlock(*material.indirect, target);
    target.pixel(*material.pixel);
    loadColorBlock(*material.color, target);
    loadTextureBlock(*material.texture, target);
}

void j3dmodel::loadMaterialDirect(const Material& material, std::uint32_t systemFlags, MaterialLoadTarget& target) {
    target.materialMode(material.mode);
    if (systemFlags & 2) return;
    loadMaterialBlocks(material, target);
    const auto& scale = material.texture->nbtScale;
    target.normalScale(scale[0] == 1 ? &scale : nullptr);
}
