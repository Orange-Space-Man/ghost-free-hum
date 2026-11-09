#pragma once

#include "game/effects/particlebillboards.h"
#include "platform/d3d11/modelmaterialrenderer.h"

namespace particlerenderer {
    class Renderer {
    public:
        void load(ID3D11Device* device);
        void prepare(const particles::EmitterPool& pool);
        void draw(ID3D11DeviceContext* context, const std::vector<particles::Billboard>& particles,
            const std::array<float, 12>& view, const std::array<float, 16>& projection);

    private:
        struct Texture {
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
        };
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertices, matrices;
        std::array<Microsoft::WRL::ComPtr<ID3D11SamplerState>, 9> samplers;
        std::map<particles::Resource, Texture, std::owner_less<particles::Resource>> textures;
        std::map<unsigned, modelrenderer::MaterialShader> materials;
        Texture& texture(const particles::Resource& source);
        modelrenderer::MaterialShader& material(const particles::Billboard& particle);
    };
}
