#include "platform/d3d11/modelmaterialrenderer.h"

#include <d3dcompiler.h>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
    std::string compare(unsigned function, unsigned reference) {
        static const char* operations[] = {"", "<", "==", "<=", ">", "!=", ">=", ""};
        if (function > 7 || reference > 255) throw std::runtime_error("Invalid original alpha comparison.");
        if (function == 0) return "false";
        if (function == 7) return "true";
        return "(opacity " + std::string(operations[function]) + " " + std::to_string(reference) + ")";
    }

    std::string swizzle(const std::array<unsigned, 4>& values) {
        std::string result;
        for (auto value : values) {
            if (value > 3) throw std::runtime_error("Invalid model combiner swap channel.");
            result += "rgba"[value];
        }
        return result;
    }

    std::string constant(unsigned selector, bool alpha) {
        if (selector <= 7) return std::to_string((8 - selector) * 32 - (selector == 0 ? 1 : 0));
        if (!alpha && selector >= 12 && selector <= 15) return "konst[" + std::to_string(selector - 12) + "].rgb";
        if (selector >= 16 && selector <= 31) return "konst[" + std::to_string((selector - 16) % 4) + "]." + "rgba"[(selector - 16) / 4];
        throw std::runtime_error("Unsupported model combiner constant selector.");
    }

    std::string color(unsigned selector) {
        if (selector < 8) return "r[" + std::to_string(selector / 2) + (selector & 1 ? "].aaa" : "].rgb");
        switch (selector) {
        case 8: return "texel.rgb";
        case 9: return "texel.aaa";
        case 10: return "raster.rgb";
        case 11: return "raster.aaa";
        case 12: return "int3(255,255,255)";
        case 13: return "int3(128,128,128)";
        case 14: return "kc";
        case 15: return "int3(0,0,0)";
        default: throw std::runtime_error("Invalid model combiner color selector.");
        }
    }

    std::string alpha(unsigned selector) {
        if (selector < 4) return "r[" + std::to_string(selector) + "].a";
        if (selector == 4) return "texel.a";
        if (selector == 5) return "raster.a";
        if (selector == 6) return "ka";
        if (selector == 7) return "0";
        throw std::runtime_error("Invalid model combiner alpha selector.");
    }
}

void modelrenderer::MaterialShader::load(ID3D11Device* device, const modelmaterial::StageProgram& program,
    const std::array<StageRegisters, 16>& retained, const PipelineState& state) {
    if (!device || !program.stageCount || program.stageCount > 16 || program.stages.size() != program.stageCount)
        throw std::runtime_error("Material shader requires a complete original stage program.");
    MaterialShader result;
    result.raster.load(device, state);
    if (state.earlyDepth && device->GetFeatureLevel() < D3D_FEATURE_LEVEL_11_0)
        throw std::runtime_error("Original early depth ordering requires Direct3D feature level 11.");
    std::ostringstream source;
    source << R"(
cbuffer Combiner : register(b1) { int4 initial[4]; int4 konst[4]; };
struct Input {
    float4 position : SV_POSITION; float4 color : COLOR0;
    float3 normal : NORMAL0; float3 binormal : NORMAL1; float3 tangent : NORMAL2;
    float2 uv0 : TEXCOORD0; float2 uv1 : TEXCOORD1; float2 uv2 : TEXCOORD2; float2 uv3 : TEXCOORD3;
    float3 viewPosition : TEXCOORD4;
    float2 generated[8] : TEXCOORD5;
    float4 secondaryColor : COLOR1;
};
int combine(int a, int b, int c, int d, int subtract) {
    a &= 255; b &= 255; c &= 255;
    int weight = c + (c >> 7);
    int fraction = (a * (256 - weight) + b * weight + 128 - subtract) >> 8;
    return d + (subtract != 0 ? -fraction : fraction);
}
)";
    for (unsigned slot = 0; slot < 8; ++slot)
        source << "Texture2D image" << slot << " : register(t" << slot << "); SamplerState sampler" << slot << " : register(s" << slot << ");\n";
    if (state.earlyDepth) source << "[earlydepthstencil]\n";
    source << "float4 main(Input input) : SV_TARGET { int4 r[4]; for(int i=0;i<4;++i) r[i]=initial[i];\n";
    unsigned expected = 0;
    for (const auto& stage : program.stages) {
        if (stage.index != expected++ || stage.colorOperation > 1 || stage.alphaOperation > 1 || stage.colorOutput > 3 || stage.alphaOutput > 3)
            throw std::runtime_error("Unsupported model combiner stage ordering or operation.");
        auto settings = retained[stage.index];
        if (stage.resetSwap) settings.textureSwap = settings.rasterSwap = {0, 1, 2, 3};
        if (stage.setKonst) {
            settings.colorConstant = stage.konstColor;
            settings.alphaConstant = stage.konstAlpha;
        }
        source << "{ int4 texel=int4(255,255,255,255);\n";
        if (stage.texture != 255) {
            if (stage.texture >= 8) throw std::runtime_error("Invalid model texture stage binding.");
            const modelmaterial::Coordinate* coordinate = nullptr;
            for (const auto& value : program.coordinates) if (value.index == stage.coordinate) coordinate = &value;
            if (!coordinate || coordinate->index >= 8)
                throw std::runtime_error("Model material references an absent texture coordinate.");
            source << "texel=int4(round(image" << stage.texture << ".Sample(sampler" << stage.texture << ",input.generated[" << coordinate->index << "])*255));\n";
        }
        source << "texel=texel." << swizzle(settings.textureSwap) << "; int4 raster=";
        if (stage.raster == 4) source << "int4(round(saturate(input.color)*255))";
        else if (stage.raster == 5) source << "int4(round(saturate(input.secondaryColor)*255))";
        else if (stage.raster == 255) source << "int4(0,0,0,0)";
        else throw std::runtime_error("Unsupported model raster color source.");
        source << "; raster=raster." << swizzle(settings.rasterSwap) << ";\n";
        source << "int3 kc=" << constant(settings.colorConstant, false) << "; int ka=" << constant(settings.alphaConstant, true) << ";\n";
        for (unsigned i = 0; i < 4; ++i)
            source << "int3 c" << i << "=" << color(stage.color[i]) << "; int a" << i << "=" << alpha(stage.alpha[i]) << ";\n";
        source << "int3 result; for(int channel=0;channel<3;++channel) result[channel]=combine(c0[channel],c1[channel],c2[channel],c3[channel]," << stage.colorOperation << ");\n";
        source << "int opacity=combine(a0,a1,a2,a3," << stage.alphaOperation << ");\n";
        source << "r[" << stage.colorOutput << "].rgb=clamp(result," << (stage.colorClamp ? 0 : -1024) << "," << (stage.colorClamp ? 255 : 1023) << ");\n";
        source << "r[" << stage.alphaOutput << "].a=clamp(opacity," << (stage.alphaClamp ? 0 : -1024) << "," << (stage.alphaClamp ? 255 : 1023) << "); }\n";
    }
    const auto& last = program.stages.back();
    source << "int opacity = r[" << last.alphaOutput << "].a & 255;\n";
    const char* alphaOperations[] = {"&&", "||", "!=", "=="};
    source << "if (!(" << compare(state.alpha.first, state.alpha.firstReference) << alphaOperations[state.alpha.operation] <<
        compare(state.alpha.second, state.alpha.secondReference) << ")) discard;\n";
    source << "return float4(r[" << last.colorOutput << "].rgb & 255, opacity)/255.0; }";
    const auto text = source.str();
    Microsoft::WRL::ComPtr<ID3DBlob> code, errors;
    if (FAILED(D3DCompile(text.data(), text.size(), nullptr, nullptr, nullptr, "main", state.earlyDepth ? "ps_5_0" : "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS,
        0, code.GetAddressOf(), errors.GetAddressOf())))
        throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Model material shader compilation failed.");
    result.owner = device;
    if (FAILED(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, result.shader.GetAddressOf())))
        throw std::runtime_error("Cannot create model material pixel shader.");
    static_assert(sizeof(CombinerInputs) == 128);
    D3D11_BUFFER_DESC description{};
    description.ByteWidth = sizeof(CombinerInputs);
    description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(device->CreateBuffer(&description, nullptr, result.constants.GetAddressOf())))
        throw std::runtime_error("Cannot create model combiner register buffer.");
    *this = std::move(result);
}

void modelrenderer::MaterialShader::bind(ID3D11DeviceContext* context, const CombinerInputs& inputs) const {
    if (!context || !owner) throw std::runtime_error("Model material shader is not ready.");
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());
    if (device.Get() != owner.Get()) throw std::runtime_error("Model material context belongs to another device.");
    context->UpdateSubresource(constants.Get(), 0, nullptr, &inputs, 0, 0);
    context->PSSetConstantBuffers(1, 1, constants.GetAddressOf());
    context->PSSetShader(shader.Get(), nullptr, 0);
    raster.bind(context);
}

void modelrenderer::MaterialShader::draw(ID3D11DeviceContext* context, const CombinerInputs& inputs, const Geometry& geometry,
    std::int16_t batch, const VertexMatrices& matrices, const TextureCoordinates& coordinates, const modellighting::Inputs& lighting) const {
    bind(context, inputs);
    geometry.draw(context, batch, matrices, coordinates, lighting, !raster.draws());
}
