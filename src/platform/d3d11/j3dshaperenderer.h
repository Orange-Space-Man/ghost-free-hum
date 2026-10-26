#pragma once

#include "jlib/j3d/j3dshapevertices.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace j3drenderer {
    class ShapeGeometry {
    public:
        void load(ID3D11Device* device, const j3dmodel::Geometry& geometry, const j3dmodel::Shape& shape, bool nbtEnabled);
        void drawGroup(ID3D11DeviceContext* context, unsigned group) const;
        std::size_t groupCount() const;

    private:
        struct Primitive {
            Microsoft::WRL::ComPtr<ID3D11Buffer> vertices, indices;
            D3D11_PRIMITIVE_TOPOLOGY topology;
            unsigned count;
        };
        Microsoft::WRL::ComPtr<ID3D11Device> owner;
        std::vector<std::vector<Primitive>> groups;
    };
}
