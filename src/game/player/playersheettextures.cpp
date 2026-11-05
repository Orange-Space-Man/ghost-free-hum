#include "playersheettextures.h"

#include <stdexcept>
#include <utility>

namespace {
    unsigned half(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 8) | bytes.at(offset + 1);
    }
}

const playermodels::GridTexture& playersheets::bindTextures(const Layout& layout, const playermodels::SharedModels& shared,
    playermodels::GridModels& grid) {
    if (layout.mode > 3) throw std::runtime_error("Invalid original sheet model mode.");
    playermodels::GridTexture result;
    result.primary = shared.texture(layout.texture);
    result.secondary = shared.texture(layout.secondaryTexture);
    const auto& bytes = *result.primary;
    if (bytes.size() < 32) throw std::runtime_error("Truncated original sheet texture header.");
    result.format = bytes[0];
    result.width = half(bytes, 2);
    result.height = half(bytes, 4);
    result.imageOffset = (half(bytes, 28) << 16) | half(bytes, 30);
    if (!result.width || !result.height || result.imageOffset >= bytes.size())
        throw std::runtime_error("Invalid original sheet texture image.");
    if (layout.mode) {
        if (bytes[8]) throw std::runtime_error("Paletted sheet replacement requires original destination-relative palette binding.");
        auto& geometry = grid.instances.at(layout.mode - 1)->model->geometry;
        geometry.textureReplacements.assign(geometry.textureHeaders.size(), result.primary);
    }
    grid.texture = std::move(result);
    return grid.texture;
}
