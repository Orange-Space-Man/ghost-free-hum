#pragma once

#include "game/world/doorresources.h"
#include "game/models/modeltexturestages.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace modelrenderer {
    class Textures {
    public:
        void load(ID3D11Device* device, const doors::ModelResource& model);
        void bind(ID3D11DeviceContext* context, std::int16_t material, const modelmaterial::StageProgram& program) const;
        ID3D11ShaderResourceView* view(std::int16_t material, unsigned slot) const;
        ID3D11SamplerState* sampler(std::int16_t material, unsigned slot) const;

    private:
        struct Texture {
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
            Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
        };
        Microsoft::WRL::ComPtr<ID3D11Device> owner;
        std::map<std::int16_t, std::vector<Texture>> materials;
    };
}
