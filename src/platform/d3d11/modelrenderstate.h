#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

namespace modelrenderer {
    struct AlphaTest {
        unsigned first = 7, firstReference = 0, operation = 0, second = 7, secondReference = 0;
    };

    struct PipelineState {
        AlphaTest alpha;
        bool depthEnabled = true, depthWrite = true, earlyDepth = false;
        unsigned depthComparison = 3, cull = 2;
        bool blendEnabled = false, subtract = false, colorWrite = true, alphaWrite = true;
        bool targetHasAlpha = true;
        bool scissor = false;
        unsigned sourceBlend = 4, destinationBlend = 5;
    };

    void initializeModelState(PipelineState& state);
    void selectModelPass(PipelineState& state, unsigned pass);
    void selectBatchCull(PipelineState& state, std::uint8_t batchCull, std::uint8_t mode);

    class RenderState {
    public:
        void load(ID3D11Device* device, const PipelineState& state);
        void bind(ID3D11DeviceContext* context) const;
        bool draws() const;

    private:
        Microsoft::WRL::ComPtr<ID3D11Device> owner;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth;
        Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> raster;
        bool visible = false;
    };
}
