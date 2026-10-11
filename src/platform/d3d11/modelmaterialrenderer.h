#pragma once

#include "game/models/modeltexturestages.h"
#include "platform/d3d11/modelgeometryrenderer.h"
#include "platform/d3d11/modelrenderstate.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace modelrenderer {
    struct CombinerInputs {
        std::array<std::array<int, 4>, 4> registers{}, constants{};
    };

    struct StageRegisters {
        std::array<unsigned, 4> textureSwap{0, 1, 2, 3}, rasterSwap{0, 1, 2, 3};
        unsigned colorConstant = 0, alphaConstant = 0;
    };

    class MaterialShader {
    public:
        void load(ID3D11Device* device, const modelmaterial::StageProgram& program,
            const std::array<StageRegisters, 16>& retained, const PipelineState& state);
        void bind(ID3D11DeviceContext* context, const CombinerInputs& inputs) const;
        void draw(ID3D11DeviceContext* context, const CombinerInputs& inputs, const Geometry& geometry,
            std::int16_t batch, const VertexMatrices& matrices, const TextureCoordinates& coordinates,
            const modellighting::Inputs& lighting) const;

    private:
        Microsoft::WRL::ComPtr<ID3D11Device> owner;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
        Microsoft::WRL::ComPtr<ID3D11Buffer> constants;
        RenderState raster;
    };
}
