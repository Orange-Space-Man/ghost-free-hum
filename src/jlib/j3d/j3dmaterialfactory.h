#pragma once

#include "jlib/j3d/j3danimationresource.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace j3dmodel {
    struct MaterialDefaults {
        std::vector<std::uint8_t> formats, registers, colors;
        bool read(const std::vector<std::uint8_t>& executable, std::string& error);
    };

    struct TextureMatrix {
        TextureMatrix() {}
        std::array<std::uint8_t, 100> info{};
        std::array<float, 12> matrix;
        std::array<float, 12> effectMatrix;
    };

    struct TextureCoordinate {
        std::array<std::uint8_t, 3> info{};
        std::uint16_t matrix = 65535;
    };

    struct MaterialColorBlock {
        bool lighting = false;
        std::uint8_t count = 0, cull = 255;
        std::array<std::array<std::uint8_t, 4>, 2> material{}, ambient{};
        std::array<std::uint16_t, 4> channels{};
        std::array<void*, 8> lights{};
    };

    struct MaterialTextureBlock {
        bool full = false;
        std::uint8_t count = 0;
        std::array<TextureCoordinate, 8> coordinates{}, postCoordinates{};
        std::array<std::unique_ptr<TextureMatrix>, 10> matrices;
        std::array<std::unique_ptr<TextureMatrix>, 20> postMatrices;
        std::array<std::uint8_t, 16> nbtScale{};
    };

    struct MaterialTevStage {
        std::array<std::uint8_t, 3> order{};
        std::array<std::uint8_t, 8> registers{};
        std::array<std::uint8_t, 9> indirect{};
        std::uint8_t colorSelection = 255, alphaSelection = 255;
    };

    struct MaterialTevBlock {
        unsigned capacity = 0;
        std::uint8_t count = 0;
        std::vector<std::uint16_t> textures;
        std::vector<MaterialTevStage> stages;
        std::array<std::array<std::uint16_t, 4>, 4> colors{};
        std::array<std::array<std::uint8_t, 4>, 4> constants{};
        std::array<std::uint8_t, 4> swaps{};
    };

    struct MaterialIndirectBlock {
        bool full = false;
        std::uint8_t count = 0;
        std::array<std::array<std::uint8_t, 2>, 4> orders{}, scales{};
        std::array<std::array<std::uint8_t, 28>, 3> matrices{};
    };

    enum class MaterialPixelKind {
        Opaque,
        TextureEdge,
        Translucent,
        Full
    };

    struct MaterialPixelBlock {
        MaterialPixelKind kind = MaterialPixelKind::Full;
        std::unique_ptr<std::array<std::uint8_t, 44>> fog;
        std::uint16_t alpha = 65535, depth = 65535;
        std::array<std::uint8_t, 2> alphaReferences{};
        std::array<std::uint8_t, 4> blend{};
        std::uint8_t depthLocation = 255, dither = 255;
    };

    struct Material {
        Material* next = nullptr;
        int shape = -1;
        std::uint32_t mode = 1, flags = 0;
        std::uint16_t index = 65535;
        std::uint8_t fieldE = 0;
        std::uintptr_t identity = 0;
        std::uint32_t identityFlags = 0;
        Material* original = nullptr;
        std::unique_ptr<MaterialAnimation> animation;
        std::unique_ptr<MaterialColorBlock> color;
        std::unique_ptr<MaterialTextureBlock> texture;
        std::unique_ptr<MaterialTevBlock> tev;
        std::unique_ptr<MaterialIndirectBlock> indirect;
        std::unique_ptr<MaterialPixelBlock> pixel;
    };

    struct Materials {
        std::vector<Material> originals;
        std::vector<std::unique_ptr<Material>> entries;
        unsigned uniqueCount = 0;
    };

    bool readMaterials(const std::vector<std::uint8_t>& bytes, unsigned blockOffset, std::uint32_t flags,
        const MaterialDefaults& defaults, Materials& materials, std::string& error);
}
