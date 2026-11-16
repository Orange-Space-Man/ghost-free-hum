#include "platform/d3d11/transitionframecapture.h"
#include <d3dcompiler.h>
#include <stdexcept>

namespace {
    void check(HRESULT result) {
        if (FAILED(result)) throw std::runtime_error("Cannot create transition frame capture.");
    }
    constexpr char shader[] = R"(
Texture2D<float4> source : register(t0);
cbuffer Overlay : register(b0) { float4 overlayColor; };
float4 overlayMain() : SV_TARGET { return overlayColor; }
float4 imageMain(float4 position : SV_POSITION) : SV_TARGET { return source.Load(int3(int2(position.xy), 0)); }
float4 vertexMain(uint id : SV_VertexID) : SV_POSITION {
    return float4(id == 2 ? 3 : -1, id == 1 ? -3 : 1, 0, 1);
}
float4 pixelMain(float4 position : SV_POSITION) : SV_TARGET {
    uint3 bytes = uint3(saturate(source.Load(int3(int2(position.xy), 0)).rgb) * 255 + 0.5);
    uint3 packed = bytes >> uint3(3, 2, 3);
    bytes = (packed << uint3(3, 2, 3)) | (packed >> uint3(2, 4, 2));
    return float4(float3(bytes) / 255, 1);
}
)";
}

void scenetransition::FrameCapture::load(ID3D11Device* device) {
    captured = false;
    Microsoft::WRL::ComPtr<ID3DBlob> code, errors;
    check(D3DCompile(shader, sizeof(shader), nullptr, nullptr, nullptr, "vertexMain", "vs_5_0", 0, 0,
        code.GetAddressOf(), errors.GetAddressOf()));
    check(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, vertex.ReleaseAndGetAddressOf()));
    code.Reset(); errors.Reset();
    check(D3DCompile(shader, sizeof(shader), nullptr, nullptr, nullptr, "pixelMain", "ps_5_0", 0, 0,
        code.GetAddressOf(), errors.GetAddressOf()));
    check(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, pixel.ReleaseAndGetAddressOf()));
    code.Reset(); errors.Reset();
    check(D3DCompile(shader, sizeof(shader), nullptr, nullptr, nullptr, "overlayMain", "ps_5_0", 0, 0,
        code.GetAddressOf(), errors.GetAddressOf()));
    check(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, overlayPixel.ReleaseAndGetAddressOf()));
    code.Reset(); errors.Reset();
    check(D3DCompile(shader, sizeof(shader), nullptr, nullptr, nullptr, "imageMain", "ps_5_0", 0, 0,
        code.GetAddressOf(), errors.GetAddressOf()));
    check(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, imagePixel.ReleaseAndGetAddressOf()));
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = 16; buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    check(device->CreateBuffer(&buffer, nullptr, colorBuffer.ReleaseAndGetAddressOf()));
    D3D11_TEXTURE2D_DESC description{};
    description.Width = 640; description.Height = 480;
    description.MipLevels = description.ArraySize = description.SampleDesc.Count = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> resource;
    check(device->CreateTexture2D(&description, nullptr, resource.GetAddressOf()));
    check(device->CreateRenderTargetView(resource.Get(), nullptr, target.ReleaseAndGetAddressOf()));
    check(device->CreateShaderResourceView(resource.Get(), nullptr, image.ReleaseAndGetAddressOf()));
    modelrenderer::PipelineState pipeline;
    modelrenderer::initializeModelState(pipeline);
    pipeline.cull = 0;
    pipeline.depthEnabled = pipeline.depthWrite = pipeline.blendEnabled = false;
    state.load(device, pipeline);
    pipeline.blendEnabled = true;
    pipeline.sourceBlend = 4; pipeline.destinationBlend = 5;
    overlayState.load(device, pipeline);
}

void scenetransition::FrameCapture::capture(ID3D11DeviceContext* context, ID3D11ShaderResourceView* source) {
    if (!target || !source) throw std::runtime_error("Transition capture requires a loaded target and source.");
    Microsoft::WRL::ComPtr<ID3D11Resource> resource, destination;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> input;
    source->GetResource(resource.GetAddressOf());
    image->GetResource(destination.GetAddressOf());
    check(resource.As(&input));
    D3D11_TEXTURE2D_DESC description{};
    input->GetDesc(&description);
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    source->GetDesc(&view);
    if (resource.Get() == destination.Get() || description.Width != 640 || description.Height != 480 ||
        description.SampleDesc.Count != 1 || view.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D ||
        view.Texture2D.MostDetailedMip != 0) {
        throw std::runtime_error("Transition capture requires a separate 640x480 single-sample source.");
    }
    context->OMSetRenderTargets(1, target.GetAddressOf(), nullptr);
    const D3D11_VIEWPORT viewport{0, 0, 640, 480, 0, 1};
    context->RSSetViewports(1, &viewport);
    state.bind(context);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->GSSetShader(nullptr, nullptr, 0);
    context->VSSetShader(vertex.Get(), nullptr, 0);
    context->PSSetShader(pixel.Get(), nullptr, 0);
    context->PSSetShaderResources(0, 1, &source);
    context->Draw(3, 0);
    ID3D11ShaderResourceView* empty = nullptr;
    context->PSSetShaderResources(0, 1, &empty);
    context->OMSetRenderTargets(0, nullptr, nullptr);
    captured = true;
}

ID3D11ShaderResourceView* scenetransition::FrameCapture::texture() const { return captured ? image.Get() : nullptr; }

void scenetransition::FrameCapture::draw(ID3D11DeviceContext* context, ID3D11RenderTargetView* destination, const ParlorWipe& wipe) {
    if (!wipe.active()) return;
    if (!destination || !overlayPixel || (wipe.drawsCapturedFrame() && !captured)) {
        throw std::runtime_error("Transition drawing requires loaded resources and an outgoing snapshot.");
    }
    Microsoft::WRL::ComPtr<ID3D11Resource> output, snapshot;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> outputTexture;
    destination->GetResource(output.GetAddressOf());
    image->GetResource(snapshot.GetAddressOf());
    check(output.As(&outputTexture));
    D3D11_TEXTURE2D_DESC description{};
    outputTexture->GetDesc(&description);
    D3D11_RENDER_TARGET_VIEW_DESC view{};
    destination->GetDesc(&view);
    if (output.Get() == snapshot.Get() || description.Width != 640 || description.Height != 480 ||
        description.SampleDesc.Count != 1 || view.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D || view.Texture2D.MipSlice != 0) {
        throw std::runtime_error("Transition drawing requires a separate 640x480 target.");
    }
    context->OMSetRenderTargets(1, &destination, nullptr);
    const D3D11_VIEWPORT viewport{0, 0, 640, 480, 0, 1};
    context->RSSetViewports(1, &viewport);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->GSSetShader(nullptr, nullptr, 0);
    context->VSSetShader(vertex.Get(), nullptr, 0);
    if (wipe.drawsCapturedFrame()) {
        state.bind(context);
        context->PSSetShader(imagePixel.Get(), nullptr, 0);
        context->PSSetShaderResources(0, 1, image.GetAddressOf());
        context->Draw(3, 0);
    }
    ID3D11ShaderResourceView* empty = nullptr;
    context->PSSetShaderResources(0, 1, &empty);
    const auto bytes = wipe.overlay();
    const std::array<float, 4> color{bytes[0] / 255.0f, bytes[1] / 255.0f, bytes[2] / 255.0f, bytes[3] / 255.0f};
    context->UpdateSubresource(colorBuffer.Get(), 0, nullptr, color.data(), 0, 0);
    context->PSSetConstantBuffers(0, 1, colorBuffer.GetAddressOf());
    overlayState.bind(context);
    context->PSSetShader(overlayPixel.Get(), nullptr, 0);
    context->Draw(3, 0);
    context->OMSetRenderTargets(0, nullptr, nullptr);
}
