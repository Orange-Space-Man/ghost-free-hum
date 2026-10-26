#pragma once

#include "jlib/j3d/j3dmaterialloads.h"
#include "jlib/j3d/j3dpixelstate.h"

namespace j3dmodel {
    struct TevStageState {
        std::array<int, 3> order{};
        std::array<int, 4> colorInputs{}, alphaInputs{};
        std::array<int, 5> colorOperation{}, alphaOperation{};
        std::array<int, 2> swap{};
        std::array<int, 9> indirect{};
        int colorSelection = 0, alphaSelection = 0;
    };

    struct TevState {
        unsigned count = 0;
        std::array<unsigned, 8> textures{};
        std::array<TevStageState, 16> stages;
        std::array<std::array<int, 4>, 4> colors{}, constants{}, swaps{};
        std::array<std::optional<std::uint32_t>, 2> overflowColorWrites;
        void apply(const TevCommand& command);
    };

    struct TextureCoordinateState {
        std::array<std::uint8_t, 3> info{};
        unsigned normalize = 0, postMatrix = 125;
    };

    struct IndirectMatrixState {
        std::array<float, 6> matrix{};
        int exponent = 0;
    };

    class MaterialState : public MaterialLoadTarget {
    public:
        TevState combiner;
        PixelState pixels;
        unsigned indirectStages = 0, colorChannels = 0, textureCoordinates = 0, cullMode = 0;
        std::uint32_t mode = 0;
        std::array<IndirectMatrixState, 3> indirectMatrices;
        std::array<std::array<std::uint8_t, 2>, 4> indirectScales{}, indirectOrders{};
        std::array<std::array<std::uint8_t, 4>, 2> materialColors{}, ambientColors{};
        std::array<ChannelControl, 4> channels{};
        std::array<std::optional<MaterialLight>, 8> lights;
        std::array<TextureCoordinateState, 8> coordinates;
        std::array<std::array<float, 4>, 64> matrixRows{}, postMatrixRows{};
        const std::array<std::uint8_t, 16>* nbtScale = nullptr;

        void tev(const TevCommand& command) override;
        void indirectCount(unsigned count) override;
        void indirectMatrix(unsigned index, const std::array<float, 6>& matrix, int exponent) override;
        void indirectScale(unsigned index, const std::array<std::uint8_t, 2>& scale) override;
        void indirectOrder(unsigned index, const std::array<std::uint8_t, 2>& order) override;
        void pixel(const MaterialPixelBlock& block) override;
        void materialColor(unsigned index, const std::array<std::uint8_t, 4>& color) override;
        void ambientColor(unsigned index, const std::array<std::uint8_t, 4>& color) override;
        void channelCount(unsigned count) override;
        void channel(unsigned index, const ChannelControl& control) override;
        void light(unsigned index, const MaterialLight& value) override;
        void cull(unsigned mode) override;
        void textureCount(unsigned count) override;
        void textureCoordinate(unsigned index, const std::array<std::uint8_t, 3>& info,
            unsigned normalize, unsigned postMatrix) override;
        void textureMatrix(unsigned index, const std::array<float, 12>& matrix, unsigned projection) override;
        void materialMode(std::uint32_t mode) override;
        void normalScale(const std::array<std::uint8_t, 16>* scale) override;
    };
}
