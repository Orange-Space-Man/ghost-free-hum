#include "platform/d3d11/gameshadowrenderer.h"

#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <stdexcept>

namespace {
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Texture2D> depthTexture, packedTexture, lookupTexture;
    ComPtr<ID3D11DepthStencilView> depthTarget;
    ComPtr<ID3D11RenderTargetView> packedTarget;
    ComPtr<ID3D11ShaderResourceView> depthView, packedView, lookupView;
    ComPtr<ID3D11VertexShader> casterShader, copyVertex;
    ComPtr<ID3D11PixelShader> copyPixel;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> vertices, constants;
    ComPtr<ID3D11DepthStencilState> depthState;
    ComPtr<ID3D11RasterizerState> rasterizers[3];
    ComPtr<ID3D11SamplerState> samplers[2];
    std::size_t capacity = 0;
    bool active = false;
    struct Constants {
        DirectX::XMFLOAT4X4 projection{};
        std::array<float, 4> color{};
        std::array<float, 4> state{};
    } values;

    void check(HRESULT result, const char* message) {
        if (FAILED(result)) throw std::runtime_error(message);
    }

    constexpr char shader[] = R"(
cbuffer Shadow : register(b2) { row_major float4x4 projection; float4 color; float4 state; };
float4 caster(float3 position : POSITION) : SV_POSITION { return mul(float4(position, 1), projection); }
float4 copyVertex(uint id : SV_VertexID) : SV_POSITION {
    return float4(id == 2 ? 3 : -1, id == 1 ? 3 : -1, 0, 1);
}
Texture2D<float> depthImage : register(t0);
float2 copyPixel(float4 position : SV_POSITION) : SV_TARGET {
    uint depth = (uint)round(saturate(depthImage.Load(int3(int2(position.xy), 0))) * 16777215.0);
    return float2((depth >> 8) & 255, (depth >> 16) & 255) / 255.0;
}
)";

    ComPtr<ID3DBlob> compile(const char* entry, const char* profile) {
        ComPtr<ID3DBlob> code, errors;
        const auto result = D3DCompile(shader, sizeof(shader), nullptr, nullptr, nullptr, entry, profile,
            D3DCOMPILE_IEEE_STRICTNESS, 0, &code, &errors);
        if (FAILED(result) && errors) throw std::runtime_error(static_cast<const char*>(errors->GetBufferPointer()));
        check(result, "Could not compile shadow shader.");
        return code;
    }
}

void gameshadowrenderer::open(ID3D11Device* sourceDevice, ID3D11DeviceContext* sourceContext) {
    device = sourceDevice;
    context = sourceContext;
    D3D11_TEXTURE2D_DESC texture{};
    texture.Width = 640;
    texture.Height = 480;
    texture.MipLevels = texture.ArraySize = 1;
    texture.SampleDesc.Count = 1;
    texture.Format = DXGI_FORMAT_R24G8_TYPELESS;
    texture.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    check(device->CreateTexture2D(&texture, nullptr, &depthTexture), "Could not create shadow depth texture.");
    D3D11_DEPTH_STENCIL_VIEW_DESC depth{};
    depth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    check(device->CreateDepthStencilView(depthTexture.Get(), &depth, &depthTarget), "Could not create shadow depth target.");
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipLevels = 1;
    check(device->CreateShaderResourceView(depthTexture.Get(), &view, &depthView), "Could not create shadow depth view.");
    texture.Format = DXGI_FORMAT_R8G8_UNORM;
    texture.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    check(device->CreateTexture2D(&texture, nullptr, &packedTexture), "Could not create packed shadow texture.");
    check(device->CreateRenderTargetView(packedTexture.Get(), nullptr, &packedTarget), "Could not create shadow copy target.");
    check(device->CreateShaderResourceView(packedTexture.Get(), nullptr, &packedView), "Could not create shadow copy view.");
    texture.Width = texture.Height = 256;
    texture.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texture.Usage = D3D11_USAGE_IMMUTABLE;
    std::vector<std::uint8_t> lookup(256 * 256 * 2);
    for (unsigned y = 0; y < 256; ++y) for (unsigned x = 0; x < 256; ++x) {
        lookup[(y * 256 + x) * 2] = static_cast<std::uint8_t>(y);
        lookup[(y * 256 + x) * 2 + 1] = static_cast<std::uint8_t>(x);
    }
    const D3D11_SUBRESOURCE_DATA initial{lookup.data(), 512, 0};
    check(device->CreateTexture2D(&texture, &initial, &lookupTexture), "Could not create shadow lookup texture.");
    check(device->CreateShaderResourceView(lookupTexture.Get(), nullptr, &lookupView), "Could not create shadow lookup view.");
    for (unsigned i = 0; i < 2; ++i) {
        D3D11_SAMPLER_DESC sampler{};
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.AddressV = i ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.MaxLOD = D3D11_FLOAT32_MAX;
        check(device->CreateSamplerState(&sampler, &samplers[i]), "Could not create shadow sampler.");
    }
    auto code = compile("caster", "vs_4_0");
    check(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &casterShader), "Could not create caster shader.");
    const D3D11_INPUT_ELEMENT_DESC element{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};
    check(device->CreateInputLayout(&element, 1, code->GetBufferPointer(), code->GetBufferSize(), &layout), "Could not create caster layout.");
    code = compile("copyVertex", "vs_4_0");
    check(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &copyVertex), "Could not create shadow copy vertex shader.");
    code = compile("copyPixel", "ps_4_0");
    check(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &copyPixel), "Could not create shadow copy pixel shader.");
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = sizeof(Constants);
    buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    check(device->CreateBuffer(&buffer, nullptr, &constants), "Could not create shadow constants.");
    D3D11_DEPTH_STENCIL_DESC state{};
    state.DepthEnable = TRUE;
    state.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    state.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    check(device->CreateDepthStencilState(&state, &depthState), "Could not create shadow depth state.");
    for (unsigned i = 0; i < 3; ++i) {
        D3D11_RASTERIZER_DESC raster{};
        raster.FillMode = D3D11_FILL_SOLID;
        raster.CullMode = i == 0 ? D3D11_CULL_NONE : i == 1 ? D3D11_CULL_BACK : D3D11_CULL_FRONT;
        raster.DepthClipEnable = TRUE;
        raster.ScissorEnable = i != 0;
        check(device->CreateRasterizerState(&raster, &rasterizers[i]), "Could not create shadow culling state.");
    }
    bind(false);
}

void gameshadowrenderer::reserve(std::size_t count) {
    if (count <= capacity) return;
    if (count > 1000000) throw std::runtime_error("Shadow vertex buffer is too large.");
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = static_cast<UINT>(count * sizeof(std::array<float, 3>));
    buffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer.Usage = D3D11_USAGE_DYNAMIC;
    buffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    check(device->CreateBuffer(&buffer, nullptr, vertices.ReleaseAndGetAddressOf()), "Could not create shadow vertex buffer.");
    capacity = count;
}

void gameshadowrenderer::begin(const s3dshadow::Projection& projection, float aspect) {
    active = projection.nearPlane > 0 && projection.farPlane > projection.nearPlane;
    values.color = projection.color;
    values.state = {};
    if (active) {
        const auto vector = [](const std::array<float, 3>& value) { return DirectX::XMVectorSet(value[0], value[1], value[2], 0); };
        const auto view = DirectX::XMMatrixLookAtRH(vector(projection.position), vector(projection.target), DirectX::XMVectorSet(0,1,0,0));
        const auto perspective = DirectX::XMMatrixPerspectiveFovRH(projection.fieldOfView * DirectX::XM_PI / 180,
            aspect, projection.nearPlane, projection.farPlane);
        DirectX::XMStoreFloat4x4(&values.projection, view * perspective);
    }
    ID3D11ShaderResourceView* empty[3]{};
    context->PSSetShaderResources(0, 3, empty);
    context->UpdateSubresource(constants.Get(), 0, nullptr, &values, 0, 0);
    context->VSSetConstantBuffers(2, 1, constants.GetAddressOf());
    context->ClearDepthStencilView(depthTarget.Get(), D3D11_CLEAR_DEPTH, 1, 0);
    context->OMSetRenderTargets(0, nullptr, depthTarget.Get());
    context->OMSetDepthStencilState(depthState.Get(), 0);
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    const D3D11_VIEWPORT viewport{0,0,640,480,0,1};
    const D3D11_RECT scissor{1,1,640,480};
    context->RSSetViewports(1, &viewport);
    context->RSSetScissorRects(1, &scissor);
    context->VSSetShader(casterShader.Get(), nullptr, 0);
    context->PSSetShader(nullptr, nullptr, 0);
    context->IASetInputLayout(layout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void gameshadowrenderer::draw(const std::vector<s3dmodel::Batch>& batches) {
    if (!active) return;
    std::size_t count = 0;
    for (const auto& batch : batches) if (batch.cull == 1 || batch.cull == 2) count += batch.vertices.size();
    if (!count) return;
    reserve(count);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    check(context->Map(vertices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped), "Could not upload shadow vertices.");
    auto* destination = static_cast<std::array<float,3>*>(mapped.pData);
    for (const auto& batch : batches) if (batch.cull == 1 || batch.cull == 2)
        for (const auto& vertex : batch.vertices) *destination++ = vertex.position;
    context->Unmap(vertices.Get(), 0);
    const UINT stride = sizeof(std::array<float,3>), offset = 0;
    context->IASetVertexBuffers(0, 1, vertices.GetAddressOf(), &stride, &offset);
    UINT first = 0;
    for (const auto& batch : batches) if (batch.cull == 1 || batch.cull == 2) {
        context->RSSetState(rasterizers[batch.cull].Get());
        context->Draw(static_cast<UINT>(batch.vertices.size()), first);
        first += static_cast<UINT>(batch.vertices.size());
    }
}

void gameshadowrenderer::finish() {
    context->OMSetRenderTargets(1, packedTarget.GetAddressOf(), nullptr);
    context->OMSetDepthStencilState(nullptr, 0);
    context->RSSetState(rasterizers[0].Get());
    context->IASetInputLayout(nullptr);
    context->VSSetShader(copyVertex.Get(), nullptr, 0);
    context->PSSetShader(copyPixel.Get(), nullptr, 0);
    context->PSSetShaderResources(0, 1, depthView.GetAddressOf());
    context->Draw(3, 0);
    context->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* empty = nullptr;
    context->PSSetShaderResources(0, 1, &empty);
}

void gameshadowrenderer::bind(bool receiver, float alpha) {
    values.state = {active && receiver ? 1.0f : 0.0f, alpha, 0, 0};
    context->UpdateSubresource(constants.Get(), 0, nullptr, &values, 0, 0);
    context->VSSetConstantBuffers(2, 1, constants.GetAddressOf());
    context->PSSetConstantBuffers(2, 1, constants.GetAddressOf());
    ID3D11ShaderResourceView* views[]{packedView.Get(), lookupView.Get()};
    ID3D11SamplerState* filters[]{samplers[0].Get(), samplers[1].Get()};
    context->PSSetShaderResources(1, 2, views);
    context->PSSetSamplers(1, 2, filters);
}

void gameshadowrenderer::close() {
    active = false;
    capacity = 0;
    depthTexture.Reset(); packedTexture.Reset(); lookupTexture.Reset();
    depthTarget.Reset(); packedTarget.Reset();
    depthView.Reset(); packedView.Reset(); lookupView.Reset();
    casterShader.Reset(); copyVertex.Reset(); copyPixel.Reset(); layout.Reset();
    vertices.Reset(); constants.Reset(); depthState.Reset();
    for (auto& state : rasterizers) state.Reset();
    for (auto& sampler : samplers) sampler.Reset();
    context.Reset(); device.Reset();
}
