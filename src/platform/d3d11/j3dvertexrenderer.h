#pragma once

#include "jlib/j3d/j3dmaterialstate.h"
#include "jlib/j3d/j3dmodelinstance.h"
#include "game/models/modellighting.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace j3drenderer {
    modellighting::Inputs prepareLighting(const j3dmodel::MaterialState& state);

    class VertexShader {
    public:
        void load(ID3D11Device* device);
        void bind(ID3D11DeviceContext* context, const j3dmodel::MaterialState& state,
            const j3dmodel::ShapePacket& packet, std::size_t matrixCount, const j3dmodel::ShapeGroup& group,
            const std::array<float, 16>& projection) const;
        void bindMatrices(ID3D11DeviceContext* context, const j3dmodel::MaterialState& state,
            const j3dmodel::Matrix& position, const j3dmodel::NormalMatrix& normal,
            const std::array<float, 16>& projection) const;

    private:
        Microsoft::WRL::ComPtr<ID3D11Device> owner;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
        Microsoft::WRL::ComPtr<ID3D11Buffer> constants, lighting;
    };
}
