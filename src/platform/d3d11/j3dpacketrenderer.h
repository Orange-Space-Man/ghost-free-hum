#pragma once

#include "jlib/j3d/j3dmaterialdisplaylist.h"
#include "platform/d3d11/j3dcombinerrenderer.h"
#include "platform/d3d11/j3dshaperenderer.h"
#include "platform/d3d11/j3dtexturerenderer.h"
#include "platform/d3d11/j3dvertexrenderer.h"
#include <map>

namespace j3drenderer {
    class PacketRenderer {
    public:
        void load(ID3D11Device* device, std::shared_ptr<const j3dmodel::ModelData> model);
        void draw(ID3D11DeviceContext* context, j3dmodel::ModelInstance& model, j3dmodel::MaterialPacket& packet,
            j3dmodel::MaterialState& state, const std::array<float, 16>& projection,
            std::uint32_t systemFlags, std::uint32_t drawMode);

    private:
        Microsoft::WRL::ComPtr<ID3D11Device> owner;
        std::shared_ptr<const j3dmodel::ModelData> resource;
        Textures textures;
        VertexShader vertex;
        std::map<std::pair<const j3dmodel::Shape*, bool>, ShapeGeometry> shapes;
        std::map<std::vector<int>, CombinerShader> shaders;
    };
}
