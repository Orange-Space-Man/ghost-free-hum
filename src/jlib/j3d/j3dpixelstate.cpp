#include "jlib/j3d/j3dpixelstate.h"

#include <stdexcept>

void j3dmodel::loadPixelState(const MaterialPixelBlock& block, PixelState& state) {
    switch (block.kind) {
    case MaterialPixelKind::Opaque:
        state.alpha = {7, 0, 0, 7, 0};
        state.blend = {0, 1, 0, 3};
        state.depth = {1, 3, 1};
        state.depthLocation = 1;
        break;
    case MaterialPixelKind::TextureEdge:
        state.alpha = {6, 128, 0, 3, 255};
        state.blend = {0, 1, 0, 3};
        state.depth = {1, 3, 1};
        state.depthLocation = 0;
        break;
    case MaterialPixelKind::Translucent:
        state.alpha = {7, 0, 0, 7, 0};
        state.blend = {1, 4, 5, 3};
        state.depth = {1, 3, 0};
        state.depthLocation = 1;
        break;
    case MaterialPixelKind::Full:
        if (block.fog) state.fog = *block.fog;
        if (block.alpha != 65535) {
            if (block.alpha >= 256) throw std::runtime_error("Invalid original J3D alpha comparison table index.");
            state.alpha = {static_cast<std::uint8_t>(block.alpha / 32), block.alphaReferences[0],
                static_cast<std::uint8_t>((block.alpha / 8) % 4), static_cast<std::uint8_t>(block.alpha % 8),
                block.alphaReferences[1]};
        }
        state.blend = block.blend;
        if (block.depth != 65535) {
            if (block.depth >= 32) throw std::runtime_error("Invalid original J3D depth table index.");
            state.depth = {static_cast<std::uint8_t>(block.depth / 16), static_cast<std::uint8_t>((block.depth / 2) % 8),
                static_cast<std::uint8_t>(block.depth % 2)};
        }
        if (block.depthLocation != 255) state.depthLocation = block.depthLocation;
        if (block.dither != 255) state.dither = block.dither;
        break;
    default:
        throw std::runtime_error("Unknown original J3D pixel block.");
    }
}
