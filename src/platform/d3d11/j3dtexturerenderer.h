#pragma once

#include "jlib/j3d/j3dmodelgeometry.h"
#include "jlib/j3d/j3dmaterialstate.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace j3drenderer {
    class Textures {
    public:
        void load(ID3D11Device* device, const j3dmodel::Geometry& geometry);
        void bind(ID3D11DeviceContext* context, const j3dmodel::MaterialState& state) const;
        ID3D11ShaderResourceView* view(unsigned index) const;
        ID3D11SamplerState* sampler(unsigned index) const;

    private:
        struct Texture {
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
            Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
        };
        Microsoft::WRL::ComPtr<ID3D11Device> owner;
        std::vector<Texture> textures;
    };
}
