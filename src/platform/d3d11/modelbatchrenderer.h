#pragma once

#include "platform/d3d11/modelmaterialrenderer.h"
#include "platform/d3d11/modeltexturerenderer.h"
#include "game/models/modeldrawtraversal.h"

namespace modelrenderer {
    class Batches {
    public:
        void load(ID3D11Device* device, const doors::ModelResource& resource);
        void prepare(const modeldraw::DrawCommand& command, unsigned pass, modellighting::State& lighting,
            PipelineState& pipeline, std::array<StageRegisters, 16>& retained, bool shadow = false);
        void draw(ID3D11DeviceContext* context, const modeldraw::DrawCommand& command, unsigned pass, std::uint8_t mode,
            const VertexMatrices& matrices, const originalmath::FloatTrig& trig, modellighting::State& lighting,
            PipelineState& pipeline, const CombinerInputs& combiner, std::array<StageRegisters, 16>& retained, bool shadow);
        std::size_t shaderCount() const;

    private:
        MaterialShader& shader(const modeldraw::DrawCommand& command, const modelmaterial::StageProgram& program,
            bool specular, bool shadow, const PipelineState& rasterState, const std::array<StageRegisters, 16>& retained);
        Microsoft::WRL::ComPtr<ID3D11Device> owner;
        Geometry geometry;
        Textures textures;
        modeldraw::Resource drawing;
        std::map<std::int16_t, modelmaterial::Material> materials;
        std::map<int, modelmaterial::TextureTransform> transforms;
        std::map<std::vector<unsigned>, MaterialShader> shaders;
    };
}
