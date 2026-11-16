#pragma once

#include "game/player/playerresourcepools.h"
#include "platform/d3d11/modelrenderstate.h"
#include <vector>

namespace playerpoints {
    class Renderer {
    public:
        void load(ID3D11Device* device);
        void draw(ID3D11DeviceContext* context, const playerresources::PointPool& pool,
            const playerresources::PointSettings& settings, const std::array<float, 12>& view,
            const std::array<float, 16>& projection, const std::array<float, 2>& originalViewport);

    private:
        struct Vertex {
            std::array<float, 4> position;
            std::array<std::uint8_t, 4> color;
        };
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
        modelrenderer::RenderState state;
        unsigned capacity = 0;
        std::vector<Vertex> vertices;
    };
}
