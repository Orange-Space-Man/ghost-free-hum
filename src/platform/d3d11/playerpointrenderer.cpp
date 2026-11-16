#include "platform/d3d11/playerpointrenderer.h"

#include <d3dcompiler.h>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
    void check(HRESULT result, const char* message) {
        if (FAILED(result)) throw std::runtime_error(message);
    }

    const char source[] = R"(
struct Vertex { float4 position : POSITION; float4 color : COLOR0; };
struct Pixel { float4 position : SV_POSITION; float4 color : COLOR0; };
Pixel vertexMain(Vertex input) {
    Pixel output;
    output.position = input.position;
    output.color = input.color;
    return output;
}
float4 pixelMain(Pixel input) : SV_TARGET { return input.color; }
)";

    std::array<float, 4> transform(const std::array<float, 3>& position,
        const std::array<float, 12>& view, const std::array<float, 16>& projection) {
        std::array<float, 4> camera{}, clip{};
        camera[3] = 1;
        for (unsigned row = 0; row < 3; ++row)
            camera[row] = view[row * 4] * position[0] + view[row * 4 + 1] * position[1] +
                view[row * 4 + 2] * position[2] + view[row * 4 + 3];
        for (unsigned row = 0; row < 4; ++row)
            for (unsigned column = 0; column < 4; ++column) clip[row] += projection[row * 4 + column] * camera[column];
        return clip;
    }

    bool clipDepth(std::array<float, 4>& start, std::array<float, 4>& end) {
        for (unsigned plane = 0; plane < 2; ++plane) {
            const float first = plane ? start[3] - start[2] : start[2];
            const float second = plane ? end[3] - end[2] : end[2];
            if (first < 0 && second < 0) return false;
            if ((first < 0) != (second < 0)) {
                const float amount = first / (first - second);
                std::array<float, 4> intersection{};
                for (unsigned axis = 0; axis < 4; ++axis) intersection[axis] = start[axis] + amount * (end[axis] - start[axis]);
                (first < 0 ? start : end) = intersection;
            }
        }
        return start[3] > 0 && end[3] > 0;
    }
}

void playerpoints::Renderer::load(ID3D11Device* owner) {
    device = owner;
    capacity = 0;
    buffer.Reset();
    Microsoft::WRL::ComPtr<ID3DBlob> code, errors;
    auto result = D3DCompile(source, sizeof(source), nullptr, nullptr, nullptr, "vertexMain", "vs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS, 0, code.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(result)) throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Point vertex compilation failed.");
    check(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, vertexShader.ReleaseAndGetAddressOf()),
        "Cannot create point vertex shader.");
    const D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    check(device->CreateInputLayout(elements, 2, code->GetBufferPointer(), code->GetBufferSize(), layout.ReleaseAndGetAddressOf()),
        "Cannot create point vertex layout.");
    code.Reset();
    errors.Reset();
    result = D3DCompile(source, sizeof(source), nullptr, nullptr, nullptr, "pixelMain", "ps_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS, 0, code.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(result)) throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Point pixel compilation failed.");
    check(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, pixelShader.ReleaseAndGetAddressOf()),
        "Cannot create point pixel shader.");
    modelrenderer::PipelineState pipeline;
    pipeline.cull = 0;
    pipeline.depthComparison = 1;
    pipeline.depthWrite = false;
    pipeline.blendEnabled = true;
    pipeline.sourceBlend = 4;
    pipeline.destinationBlend = 1;
    state.load(device.Get(), pipeline);
}

void playerpoints::Renderer::draw(ID3D11DeviceContext* context, const playerresources::PointPool& pool,
    const playerresources::PointSettings& settings, const std::array<float, 12>& view,
    const std::array<float, 16>& projection, const std::array<float, 2>& originalViewport) {
    if (!(originalViewport[0] > 0 && originalViewport[1] > 0)) throw std::runtime_error("Point drawing requires the original viewport dimensions.");
    vertices.clear();
    pool.draw(settings, [&](const playerresources::PointLine& line) {
        auto start = transform(line.start, view, projection);
        auto end = transform(line.end, view, projection);
        if (!clipDepth(start, end)) return;
        const float dx = std::abs(end[0] / end[3] - start[0] / start[3]) * originalViewport[0];
        const float dy = std::abs(end[1] / end[3] - start[1] / start[3]) * originalViewport[1];
        const unsigned axis = dy > dx ? 0 : 1;
        const float halfWidth = float(line.width) / (6.0f * originalViewport[axis]);
        std::array<Vertex, 4> corners{{{start, line.color}, {start, line.color}, {end, line.color}, {end, line.color}}};
        for (unsigned i = 0; i < corners.size(); ++i)
            corners[i].position[axis] += (i & 1 ? halfWidth : -halfWidth) * corners[i].position[3];
        for (unsigned index : {0u, 1u, 2u, 2u, 1u, 3u}) vertices.push_back(corners[index]);
    });
    if (vertices.empty()) return;
    if (!device) throw std::runtime_error("Point renderer has not been loaded.");
    if (vertices.size() > std::numeric_limits<unsigned>::max() / sizeof(Vertex)) throw std::runtime_error("Point vertex buffer is too large.");
    if (vertices.size() > capacity) {
        capacity = static_cast<unsigned>(vertices.size());
        D3D11_BUFFER_DESC description{};
        description.ByteWidth = capacity * sizeof(Vertex);
        description.Usage = D3D11_USAGE_DYNAMIC;
        description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        check(device->CreateBuffer(&description, nullptr, buffer.ReleaseAndGetAddressOf()), "Cannot create point vertex buffer.");
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    check(context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped), "Cannot map point vertex buffer.");
    std::memcpy(mapped.pData, vertices.data(), vertices.size() * sizeof(Vertex));
    context->Unmap(buffer.Get(), 0);
    const unsigned stride = sizeof(Vertex), offset = 0;
    auto* input = buffer.Get();
    context->IASetVertexBuffers(0, 1, &input, &stride, &offset);
    context->IASetInputLayout(layout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);
    context->PSSetShader(pixelShader.Get(), nullptr, 0);
    state.bind(context);
    context->Draw(static_cast<unsigned>(vertices.size()), 0);
}
