#include "platform/d3d11/j3dpacketrenderer.h"

#include <functional>
#include <stdexcept>

namespace {
    struct Draw : j3dmodel::PacketDrawTarget {
        std::function<void()> bind;
        std::function<void(j3dmodel::ShapePacket&)> submit;
        void select(j3dmodel::MaterialPacket&) override {}
        void flush() override { bind(); }
        void shape(j3dmodel::ShapePacket& packet) override { submit(packet); }
    };
    modelrenderer::PipelineState pipeline(const j3dmodel::MaterialState& state) {
        const auto& pixels = state.pixels;
        if (pixels.blend[0] == 2 || pixels.blend[0] > 3 || pixels.dither || (pixels.fog && (*pixels.fog)[0]))
            throw std::runtime_error("J3D raster state requires unimplemented logic, dither or fog processing.");
        modelrenderer::PipelineState result;
        result.alpha = {pixels.alpha[0], pixels.alpha[1], pixels.alpha[2], pixels.alpha[3], pixels.alpha[4]};
        result.depthEnabled = pixels.depth[0] != 0; result.depthComparison = pixels.depth[1]; result.depthWrite = pixels.depth[2] != 0;
        result.earlyDepth = pixels.depthLocation != 0; result.cull = state.cullMode;
        result.blendEnabled = pixels.blend[0] == 1; result.subtract = pixels.blend[0] == 3;
        result.sourceBlend = pixels.blend[1]; result.destinationBlend = pixels.blend[2];
        return result;
    }
}

void j3drenderer::PacketRenderer::load(ID3D11Device* device, std::shared_ptr<const j3dmodel::ModelData> model) {
    if (!device || !model) throw std::runtime_error("J3D packet renderer requires an owned model.");
    PacketRenderer result;
    result.owner = device; result.resource = std::move(model);
    result.textures.load(device, result.resource->geometry);
    result.vertex.load(device);
    for (const auto& material : result.resource->materials.entries) {
        if (material->shape < 0) continue;
        const auto* shape = result.resource->geometry.shapes.at(material->shape).get();
        const auto& bytes = *result.resource->geometry.bytes;
        const auto word = [&](std::size_t offset) {
            return (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
                (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
        };
        for (std::size_t offset = shape->descriptorOffset; word(offset) != 255; offset += 8)
            if (word(offset) <= 8 && word(offset + 4)) throw std::runtime_error("J3D packet renderer requires a rigid vertex matrix binding.");
        const bool nbt = material->texture->nbtScale[0] == 1;
        const auto key = std::make_pair(shape, nbt);
        if (!result.shapes.count(key)) result.shapes[key].load(device, result.resource->geometry, *shape, nbt);
    }
    *this = std::move(result);
}

void j3drenderer::PacketRenderer::draw(ID3D11DeviceContext* context, j3dmodel::ModelInstance& model,
    j3dmodel::MaterialPacket& packet, j3dmodel::MaterialState& state, const std::array<float, 16>& projection,
    std::uint32_t systemFlags, std::uint32_t drawMode) {
    if (!owner || model.model.get() != resource.get() || packet.textures != &resource->geometry.textureHeaders)
        throw std::runtime_error("J3D packet does not belong to the uploaded resource.");
    Draw draw;
    draw.bind = [&] {
        const auto raster = pipeline(state);
        const auto key = combinerKey(state);
        auto found = shaders.find(key);
        if (found == shaders.end()) {
            CombinerShader shader;
            shader.load(owner.Get(), state, raster);
            found = shaders.emplace(key, std::move(shader)).first;
        }
        found->second.bind(context, state);
        textures.bind(context, state);
    };
    draw.submit = [&](j3dmodel::ShapePacket& shape) {
        if (state.cullMode == 3) return;
        const auto& geometry = shapes.at({shape.shape, state.nbtScale != nullptr});
        for (unsigned i = 0; i < shape.shape->groups.size(); ++i) {
            vertex.bind(context, state, shape, resource->geometry.drawMatrices.size(), shape.shape->groups[i], projection);
            geometry.drawGroup(context, i);
        }
    };
    j3dmodel::drawMaterialPacket(packet, systemFlags, drawMode, state, draw);
}
