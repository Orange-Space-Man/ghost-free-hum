#pragma once

#include "playersheetdraw.h"
#include "platform/d3d11/j3dvertexrenderer.h"
#include "platform/d3d11/j3dcombinerrenderer.h"
#include "platform/d3d11/j3dtexturerenderer.h"
#include <map>

namespace playersheets {
    class Renderer {
    public:
        explicit Renderer(ID3D11Device* device);
        void bindTexture(ID3D11DeviceContext* context, unsigned slot, const playermodels::GridTexture& texture);
        void draw(ID3D11DeviceContext* context, const std::vector<std::array<DrawVertex, 4>>& quads,
            const j3dmodel::MaterialState& state, const actormodel::Matrix& camera,
            const std::array<float, 16>& projection);

    private:
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertices;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
        std::map<std::shared_ptr<const std::vector<std::uint8_t>>, j3drenderer::Textures> textures;
        j3drenderer::VertexShader vertexShader;
        std::map<std::vector<int>, j3drenderer::CombinerShader> shaders;
        std::size_t capacity = 0;
    };
}
