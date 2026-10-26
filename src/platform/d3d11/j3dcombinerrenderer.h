#pragma once

#include "jlib/j3d/j3dmaterialstate.h"
#include "platform/d3d11/modelmaterialrenderer.h"

namespace j3drenderer {
    struct CombinerProgram {
        modelmaterial::StageProgram program;
        std::array<modelrenderer::StageRegisters, 16> registers;
    };

    CombinerProgram buildCombinerProgram(const j3dmodel::MaterialState& state);

    class CombinerShader {
    public:
        void load(ID3D11Device* device, const j3dmodel::MaterialState& state,
            const modelrenderer::PipelineState& pipeline);
        void bind(ID3D11DeviceContext* context, const j3dmodel::MaterialState& state) const;

    private:
        modelrenderer::MaterialShader shader;
    };
}
