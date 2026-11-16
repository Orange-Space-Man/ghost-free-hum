#pragma once

#include "platform/d3d11/modelmaterialrenderer.h"
#include "game/startup/scenetransition.h"

namespace scenetransition {
    class FrameCapture {
    public:
        void load(ID3D11Device* device);
        void capture(ID3D11DeviceContext* context, ID3D11ShaderResourceView* source);
        ID3D11ShaderResourceView* texture() const;
        void draw(ID3D11DeviceContext* context, ID3D11RenderTargetView* destination, const ParlorWipe& wipe);

    private:
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> image;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> overlayPixel;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> imagePixel;
        Microsoft::WRL::ComPtr<ID3D11Buffer> colorBuffer;
        modelrenderer::RenderState overlayState;
        modelrenderer::RenderState state;
        bool captured = false;
    };
}
