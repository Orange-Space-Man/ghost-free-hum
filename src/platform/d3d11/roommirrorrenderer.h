#pragma once

#include "game/world/roommirror.h"
#include "platform/d3d11/modelmaterialrenderer.h"

namespace roommirror {
    class Renderer {
    public:
        void load(ID3D11Device* device, const Owner& owner);
        void capture(ID3D11DeviceContext* context, ID3D11ShaderResourceView* scene);
        void draw(ID3D11DeviceContext* context, const actormodel::Matrix& view, const std::array<float, 16>& projection);

    private:
        unsigned width = 0, height = 0;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> copyVertex;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> copyPixel;
        modelrenderer::RenderState copyState;
        modelrenderer::Geometry geometry;
        modelrenderer::MaterialShader material;
        modelrenderer::TextureCoordinates coordinates;
        std::vector<modeldraw::Vertex> vertices;
    };
}
