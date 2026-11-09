#pragma once

#include "game/player/playersheetgeometry.h"
#include "platform/d3d11/modelmaterialrenderer.h"

namespace playersheets {
    class Renderer {
    public:
        void load(ID3D11Device* device);
        void draw(ID3D11DeviceContext* context, const playermodels::GridModels& grid, unsigned pass,
            const modelrenderer::VertexMatrices& matrices, const modellighting::State& lighting);

    private:
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
        std::map<std::shared_ptr<const std::vector<std::uint8_t>>, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>,
            std::owner_less<std::shared_ptr<const std::vector<std::uint8_t>>>> textures;
        modelrenderer::Geometry geometry;
        std::array<modelrenderer::MaterialShader, 2> materials;
        std::array<modelrenderer::TextureCoordinates, 2> coordinates;
        ID3D11ShaderResourceView* texture(const playermodels::GridTexture& source);
    };
}
