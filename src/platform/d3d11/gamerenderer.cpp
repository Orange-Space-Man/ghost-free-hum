#include "platform/windows/debugoverlay.h"
#include "platform/d3d11/gamerenderer.h"
#include "platform/d3d11/gameshadowrenderer.h"
#include "game/models/s3dfog.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <stdexcept>

namespace {
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;
    ComPtr<ID3D11RenderTargetView> targetView;
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> constants;
    ComPtr<ID3D11Buffer> fogConstants;
    ComPtr<ID3D11Query> frameComplete;
    ComPtr<ID3D11RasterizerState> solidState;
    ComPtr<ID3D11PixelShader> interfaceShader;
    ComPtr<ID3D11PixelShader> opaqueShader;
    ComPtr<ID3D11Buffer> interfaceVertices;
    ComPtr<ID3D11Buffer> interfaceColors;
    ComPtr<ID3D11BlendState> interfaceBlend;
    ComPtr<ID3D11SamplerState> interfaceSampler;
    std::vector<ComPtr<ID3D11ShaderResourceView>> interfaceTextures;
    struct Preview {
        unsigned width = 148, height = 116;
        ComPtr<ID3D11RenderTargetView> target;
        ComPtr<ID3D11DepthStencilView> depth;
        std::vector<ComPtr<ID3D11ShaderResourceView>> textures;
    };
    std::vector<Preview> previews;
    ComPtr<ID3D11Buffer> modelVertices;
    std::size_t modelVertexCapacity = 0;
    std::size_t previewTextureStart = 0;
    struct SceneResources {
        std::vector<ComPtr<ID3D11ShaderResourceView>> textures;
        std::vector<Preview> previews;
        ComPtr<ID3D11Buffer> vertices;
        std::size_t capacity = 0, textureStart = 0;
    };
    std::map<std::string, SceneResources> sceneResources;
    ComPtr<ID3D11DepthStencilState> modelDepth[2];
    ComPtr<ID3D11SamplerState> modelSamplers[9];
    ComPtr<ID3D11RasterizerState> modelRasterizers[3];
    int width = 1;
    int height = 1;

    struct Vertex {
        std::array<float, 3> position{};
        std::array<float, 4> color{};
        std::array<float, 2> coordinate{};
        std::array<float, 4> secondary{0, 0, 0, 1};
        std::array<float, 2> secondCoordinate{};
    };

    struct Constants {
        DirectX::XMFLOAT4X4 matrix{};
    };

    struct FogConstants {
        std::array<float, 4> color{};
        std::array<float, 4> parameters{};
        std::array<unsigned, 4> control{};
        std::array<std::array<float, 4>, 3> ranges{};
    };

    void check(HRESULT result, const char* message) {
        if (FAILED(result)) throw std::runtime_error(std::string(message) + " (" + std::to_string(static_cast<unsigned long>(result)) + ")");
    }

    constexpr char shader[] = R"(
cbuffer Camera : register(b0) { row_major float4x4 viewProjection; };
cbuffer Shadow : register(b2) { row_major float4x4 shadowProjection; float4 shadowColor; float4 shadowState; };
struct VertexInput { float3 position : POSITION; float4 color : COLOR0; float2 coordinate : TEXCOORD0; float4 secondary : COLOR1; float2 secondCoordinate : TEXCOORD2; };
struct PixelInput { float4 position : SV_POSITION; float4 color : COLOR0; float2 coordinate : TEXCOORD0; float4 shadowCoordinate : TEXCOORD1; float4 secondary : COLOR1; float2 secondCoordinate : TEXCOORD2; };
PixelInput vertexMain(VertexInput input) {
    PixelInput output;
    output.position = mul(float4(input.position, 1), viewProjection);
    output.color = input.color;
    output.secondary = input.secondary;
    output.secondCoordinate = input.secondCoordinate;
    output.coordinate = input.coordinate;
    output.shadowCoordinate = mul(float4(input.position, 1), shadowProjection);
    return output;
}
)";
}

void gamerenderer::open(HWND window, const std::vector<roommaterials::Texture>& images) {
    DXGI_SWAP_CHAIN_DESC description{};
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = 1;
    description.OutputWindow = window;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0};
    HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, 1, D3D11_SDK_VERSION, &description, &swapChain, &device, nullptr, &context);
    if (FAILED(result)) {
        result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            levels, 1, D3D11_SDK_VERSION, &description, &swapChain, &device, nullptr, &context);
    }
    check(result, "Could not start Direct3D 11");
    ComPtr<IDXGIFactory> factory;
    check(swapChain->GetParent(IID_PPV_ARGS(&factory)), "Could not access DXGI");
    check(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER), "Could not set window mode");
    ComPtr<ID3DBlob> vertexCode;
    ComPtr<ID3DBlob> errors;
    result = D3DCompile(shader, sizeof(shader), nullptr, nullptr, nullptr, "vertexMain", "vs_4_0", 0, 0, &vertexCode, &errors);
    if (FAILED(result) && errors) throw std::runtime_error(static_cast<const char*>(errors->GetBufferPointer()));
    check(result, "Vertex shader compilation failed");
    check(device->CreateVertexShader(vertexCode->GetBufferPointer(), vertexCode->GetBufferSize(), nullptr, &vertexShader), "Could not create vertex shader");
    const D3D11_INPUT_ELEMENT_DESC elements[]{
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 52, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    check(device->CreateInputLayout(elements, 5, vertexCode->GetBufferPointer(), vertexCode->GetBufferSize(), &layout), "Could not create vertex layout");
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = sizeof(Constants);
    buffer.Usage = D3D11_USAGE_DEFAULT;
    buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    check(device->CreateBuffer(&buffer, nullptr, &constants), "Could not create camera buffer");
    buffer.ByteWidth = sizeof(FogConstants);
    check(device->CreateBuffer(&buffer, nullptr, &fogConstants), "Could not create fog buffer");
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster.DepthClipEnable = TRUE;
    check(device->CreateRasterizerState(&raster, &solidState), "Could not create solid rasterizer");
    setInterfaceTextures(images);
    gameshadowrenderer::open(device.Get(), context.Get());
    debugoverlay::open(window, device.Get(), context.Get());
    RECT client{};
    GetClientRect(window, &client);
    resize(std::max(1L, client.right), std::max(1L, client.bottom));
}

void gamerenderer::resize(int newWidth, int newHeight) {
    width = std::max(1, newWidth);
    height = std::max(1, newHeight);
    if (!device) return;
    context->OMSetRenderTargets(0, nullptr, nullptr);
    targetView.Reset();
    check(swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0), "Could not resize the render surface");
    ComPtr<ID3D11Texture2D> backBuffer;
    check(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)), "Could not access render surface");
    check(device->CreateRenderTargetView(backBuffer.Get(), nullptr, &targetView), "Could not create render target");
}

void gamerenderer::capture(const std::filesystem::path& path) {
    ComPtr<ID3D11Texture2D> source;
    check(swapChain->GetBuffer(0, IID_PPV_ARGS(&source)), "Could not access capture surface");
    D3D11_TEXTURE2D_DESC description{};
    source->GetDesc(&description);
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    check(device->CreateTexture2D(&description, nullptr, &staging), "Could not create capture surface");
    context->CopyResource(staging.Get(), source.Get());
    const std::size_t stride = (static_cast<std::size_t>(width) * 3 + 3) & ~std::size_t(3);
    std::vector<std::uint8_t> pixels(stride * height);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    check(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Could not read captured pixels");
    for (int y = 0; y < height; ++y) {
        const auto* row = static_cast<const std::uint8_t*>(mapped.pData) + y * mapped.RowPitch;
        for (int x = 0; x < width; ++x) {
            const std::size_t output = (height - 1 - y) * stride + x * 3;
            pixels[output] = row[x * 4 + 2];
            pixels[output + 1] = row[x * 4 + 1];
            pixels[output + 2] = row[x * 4];
        }
    }
    context->Unmap(staging.Get(), 0);
    BITMAPFILEHEADER header{};
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + static_cast<DWORD>(pixels.size());
    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info);
    info.biWidth = width;
    info.biHeight = height;
    info.biPlanes = 1;
    info.biBitCount = 24;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(&info), sizeof(info));
    file.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
    if (!file) throw std::runtime_error("Could not save the render capture.");
}

void gamerenderer::setInterfaceTextures(const std::vector<roommaterials::Texture>& images) {
    previews.clear();
    previewTextureStart = 0;
    if (!interfaceShader) {
        constexpr char source[] = R"(
cbuffer Colors : register(b1) { float4 black; float4 white; float4 opacity; };
cbuffer Fog : register(b3) { float4 fogColor; float4 fogParameters; uint4 fogControl; float4 fogRanges[3]; };
cbuffer Shadow : register(b2) { row_major float4x4 shadowProjection; float4 shadowColor; float4 shadowState; };
Texture2D image : register(t0);
Texture2D shadowImage : register(t1);
Texture2D shadowLookup : register(t2);
Texture2D secondImage : register(t3);
SamplerState filtering : register(s0);
SamplerState shadowFiltering : register(s1);
SamplerState lookupFiltering : register(s2);
SamplerState secondFiltering : register(s3);
float4 pixelMain(float4 position : SV_POSITION, float4 color : COLOR0, float2 coordinate : TEXCOORD0, float4 shadowCoordinate : TEXCOORD1, float4 secondary : COLOR1, float2 secondCoordinate : TEXCOORD2) : SV_TARGET {
        float4 result = lerp(black, white, image.Sample(filtering, coordinate)) * color;
        result.rgb = saturate(result.rgb * secondary.a + secondary.rgb);
        if (opacity.z != 0) {
            float4 layer = secondImage.Sample(secondFiltering, secondCoordinate);
            result.rgb = lerp(result.rgb, layer.rgb, layer.a);
        }
        if (shadowState.x != 0) {
            float3 projected = shadowCoordinate.xyz / shadowCoordinate.w;
            float2 reference = round(shadowLookup.Sample(lookupFiltering, float2(projected.z, projected.z * 256)).rg * 255);
            float2 stored = round(shadowImage.Sample(shadowFiltering, projected.xy * float2(0.5, -0.5) + 0.5).rg * 255);
            float amount = dot(reference, float2(1, 256)) > dot(stored, float2(1, 256)) ? shadowColor.a * color.a : 0;
            result.rgb = lerp(result.rgb, shadowColor.rgb, amount);
            result.a *= shadowState.y;
        }
        result.a *= opacity.x;
        clip(result.a - opacity.y);
        if (fogControl.x != 0) {
            uint depth = uint(saturate(position.z) * 16777215.0);
            float denominator = float(int(fogControl.y) - int(depth >> fogControl.z));
            float distance = fogParameters.x * 16777216.0 / denominator;
            if (fogControl.w != 0) {
                float horizontal = (2.0 * position.x - fogParameters.z) / fogParameters.w;
                float samplePoint = clamp(9.0 * (1.0 - abs(horizontal)), 0.0, 9.0);
                uint lower = uint(samplePoint);
                uint upper = min(lower + 1, 9u);
                float scale = lerp(fogRanges[lower / 4][lower % 4], fogRanges[upper / 4][upper % 4], frac(samplePoint));
                distance *= sqrt(1.0 + (horizontal * horizontal) / (scale * scale));
            }
            float amount = saturate(distance - fogParameters.y);
            if (fogControl.x == 4) amount = 1.0 - exp2(-8.0 * amount);
            else if (fogControl.x == 5) amount = 1.0 - exp2(-8.0 * amount * amount);
            else if (fogControl.x == 6) amount = exp2(-8.0 * (1.0 - amount));
            else if (fogControl.x == 7) amount = exp2(-8.0 * (1.0 - amount) * (1.0 - amount));
            uint weight = uint(round(amount * 256.0));
            uint3 original = uint3(round(saturate(result.rgb) * 255.0));
            uint3 target = uint3(round(fogColor.rgb * 255.0));
            result.rgb = float3((original * (256u - weight) + target * weight) >> 8) / 255.0;
        }
        return result;
}
[earlydepthstencil]
float4 pixelEarly(float4 position : SV_POSITION, float4 color : COLOR0, float2 coordinate : TEXCOORD0, float4 shadowCoordinate : TEXCOORD1, float4 secondary : COLOR1, float2 secondCoordinate : TEXCOORD2) : SV_TARGET {
    return pixelMain(position, color, coordinate, shadowCoordinate, secondary, secondCoordinate);
}
)";
        ComPtr<ID3DBlob> code;
        ComPtr<ID3DBlob> errors;
        const auto result = D3DCompile(source, sizeof(source), nullptr, nullptr, nullptr, "pixelMain", "ps_4_0", 0, 0, &code, &errors);
        if (FAILED(result) && errors) throw std::runtime_error(static_cast<const char*>(errors->GetBufferPointer()));
        check(result, "Could not compile interface shader");
        check(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &interfaceShader), "Could not create interface shader");
        code.Reset();
        errors.Reset();
        const auto earlyResult = D3DCompile(source, sizeof(source), nullptr, nullptr, nullptr, "pixelEarly", "ps_5_0", 0, 0, &code, &errors);
        if (FAILED(earlyResult) && errors) throw std::runtime_error(static_cast<const char*>(errors->GetBufferPointer()));
        check(earlyResult, "Could not compile opaque material shader");
        check(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &opaqueShader), "Could not create opaque material shader");
        D3D11_BUFFER_DESC buffer{};
        buffer.ByteWidth = sizeof(Vertex) * 6;
        buffer.Usage = D3D11_USAGE_DEFAULT;
        buffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        check(device->CreateBuffer(&buffer, nullptr, &interfaceVertices), "Could not create interface vertices");
        buffer.ByteWidth = sizeof(float) * 12;
        buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        check(device->CreateBuffer(&buffer, nullptr, &interfaceColors), "Could not create interface colors");
        D3D11_BLEND_DESC blend{};
        auto& target = blend.RenderTarget[0];
        target.BlendEnable = TRUE;
        target.SrcBlend = D3D11_BLEND_SRC_ALPHA;
        target.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        target.BlendOp = D3D11_BLEND_OP_ADD;
        target.SrcBlendAlpha = D3D11_BLEND_ONE;
        target.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        check(device->CreateBlendState(&blend, &interfaceBlend), "Could not create interface blending");
        D3D11_SAMPLER_DESC sampler{};
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.MaxLOD = D3D11_FLOAT32_MAX;
        check(device->CreateSamplerState(&sampler, &interfaceSampler), "Could not create interface filtering");
    }
    interfaceTextures.clear();
    for (const auto& image : images) {
        if (image.width == 0 || image.height == 0 || image.pixels.size() != static_cast<std::size_t>(image.width) * image.height * 4) {
            throw std::runtime_error("Invalid interface texture.");
        }
        D3D11_TEXTURE2D_DESC description{};
        description.Width = image.width;
        description.Height = image.height;
        description.MipLevels = description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_IMMUTABLE;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA pixels{};
        pixels.pSysMem = image.pixels.data();
        pixels.SysMemPitch = image.width * 4;
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11ShaderResourceView> view;
        check(device->CreateTexture2D(&description, &pixels, &texture), "Could not upload interface texture");
        check(device->CreateShaderResourceView(texture.Get(), nullptr, &view), "Could not create interface texture view");
        interfaceTextures.push_back(std::move(view));
    }
}

void gamerenderer::cacheScene(const std::string& name) {
    sceneResources[name] = {interfaceTextures, previews, modelVertices, modelVertexCapacity, previewTextureStart};
}

bool gamerenderer::useScene(const std::string& name) {
    const auto found = sceneResources.find(name);
    if (found == sceneResources.end()) return false;
    context->OMSetRenderTargets(0, nullptr, nullptr);
    const auto& cached = found->second;
    interfaceTextures = cached.textures;
    previews = cached.previews;
    modelVertices = cached.vertices;
    modelVertexCapacity = cached.capacity;
    previewTextureStart = cached.textureStart;
    return true;
}

void gamerenderer::drawInterface(const std::vector<startupscene::Picture>& pictures, bool present) {
    context->OMSetDepthStencilState(nullptr, 0);
    if (!context || !targetView || !interfaceShader) return;
    const FogConstants noFog;
    context->UpdateSubresource(fogConstants.Get(), 0, nullptr, &noFog, 0, 0);
    context->PSSetConstantBuffers(3, 1, fogConstants.GetAddressOf());
    gameshadowrenderer::bind(false);
    const float background[]{0, 0, 0, 1};
    context->ClearRenderTargetView(targetView.Get(), background);
    context->OMSetRenderTargets(1, targetView.GetAddressOf(), nullptr);
    context->OMSetBlendState(interfaceBlend.Get(), nullptr, 0xffffffff);
    const float scale = std::min(width / 640.0f, height / 480.0f);
    const D3D11_VIEWPORT viewport{(width - 640 * scale) / 2, (height - 480 * scale) / 2, 640 * scale, 480 * scale, 0, 1};
    context->RSSetViewports(1, &viewport);
    context->RSSetState(solidState.Get());
    Constants camera;
    DirectX::XMStoreFloat4x4(&camera.matrix, DirectX::XMMatrixOrthographicOffCenterLH(0, 640, 480, 0, 0, 1));
    context->UpdateSubresource(constants.Get(), 0, nullptr, &camera, 0, 0);
    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, constants.GetAddressOf());
    context->PSSetShader(interfaceShader.Get(), nullptr, 0);
    context->PSSetConstantBuffers(1, 1, interfaceColors.GetAddressOf());
    context->PSSetSamplers(0, 1, interfaceSampler.GetAddressOf());
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, interfaceVertices.GetAddressOf(), &stride, &offset);
    context->IASetInputLayout(layout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (const auto& picture : pictures) {
        const float x = picture.x, y = picture.y, right = x + picture.width, bottom = y + picture.height;
        const std::array<std::array<float, 2>, 4> positions{{{x, y}, {right, y}, {x, bottom}, {right, bottom}}};
        const unsigned indices[]{0, 1, 2, 2, 1, 3};
        Vertex data[6];
        for (unsigned i = 0; i < 6; ++i) {
            const auto index = indices[i];
            const auto& point = positions[index];
            const auto& matrix = picture.transform;
            data[i] = {{matrix[0] * point[0] + matrix[2] * point[1] + matrix[4],
                matrix[1] * point[0] + matrix[3] * point[1] + matrix[5], 0}, picture.colors[index], picture.coordinates[index]};
        }
        const std::array<std::array<float, 4>, 3> colors{picture.black, picture.white, {picture.alpha, 0, 0, 0}};
        context->UpdateSubresource(interfaceVertices.Get(), 0, nullptr, data, 0, 0);
        context->UpdateSubresource(interfaceColors.Get(), 0, nullptr, colors.data(), 0, 0);
        context->PSSetShaderResources(0, 1, interfaceTextures.at(picture.texture).GetAddressOf());
        context->Draw(6, 0);
    }
    debugoverlay::draw();
    if (present) check(swapChain->Present(1, 0), "Could not present startup");
}

std::vector<std::size_t> gamerenderer::loadPreviews(const std::vector<s3dmodel::Model>& models, unsigned previewWidth, unsigned previewHeight) {
    if (!previews.empty()) interfaceTextures.resize(previewTextureStart);
    previewTextureStart = interfaceTextures.size();
    previews.clear();
    modelVertexCapacity = 0;
    modelVertices.Reset();
    for (auto& state : modelDepth) state.Reset();
    for (auto& sampler : modelSamplers) sampler.Reset();
    for (auto& state : modelRasterizers) state.Reset();
    std::vector<std::size_t> indices;
    std::size_t capacity = 1;
    for (const auto& model : models) {
        std::size_t shadowVertices = 0;
        for (const auto& batch : model.shadowBatches) shadowVertices += batch.vertices.size();
        gameshadowrenderer::reserve(shadowVertices);
        Preview preview;
        preview.width = previewWidth;
        preview.height = previewHeight;
        D3D11_TEXTURE2D_DESC description{};
        description.Width = previewWidth;
        description.Height = previewHeight;
        description.MipLevels = description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11ShaderResourceView> view;
        check(device->CreateTexture2D(&description, nullptr, &texture), "Could not create menu preview surface");
        check(device->CreateRenderTargetView(texture.Get(), nullptr, &preview.target), "Could not create menu preview target");
        check(device->CreateShaderResourceView(texture.Get(), nullptr, &view), "Could not create menu preview view");
        indices.push_back(interfaceTextures.size());
        interfaceTextures.push_back(std::move(view));
        texture.Reset();
        description.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        description.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        check(device->CreateTexture2D(&description, nullptr, &texture), "Could not create preview depth buffer");
        check(device->CreateDepthStencilView(texture.Get(), nullptr, &preview.depth), "Could not create preview depth view");
        for (const auto& image : model.textures) {
            description.Width = image.width;
            description.Height = image.height;
            description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            description.Usage = D3D11_USAGE_IMMUTABLE;
            D3D11_SUBRESOURCE_DATA pixels{};
            pixels.pSysMem = image.pixels.data();
            pixels.SysMemPitch = image.width * 4;
            texture.Reset();
            view.Reset();
            check(device->CreateTexture2D(&description, &pixels, &texture), "Could not upload S3D texture");
            check(device->CreateShaderResourceView(texture.Get(), nullptr, &view), "Could not create S3D texture view");
            preview.textures.push_back(std::move(view));
        }
        std::size_t vertices = 0;
        for (const auto& batch : model.batches) vertices += batch.vertices.size();
        capacity = std::max(capacity, vertices);
        previews.push_back(std::move(preview));
    }
    if (capacity > 1000000) throw std::runtime_error("S3D preview batch is too large.");
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = static_cast<UINT>(capacity * sizeof(Vertex));
    buffer.Usage = D3D11_USAGE_DYNAMIC;
    buffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    check(device->CreateBuffer(&buffer, nullptr, &modelVertices), "Could not create S3D vertex buffer");
    modelVertexCapacity = capacity;
    for (unsigned index = 0; index < 2; ++index) {
        D3D11_DEPTH_STENCIL_DESC depth{};
        depth.DepthEnable = TRUE;
        depth.DepthWriteMask = index == 0 ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        depth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        check(device->CreateDepthStencilState(&depth, &modelDepth[index]), "Could not create S3D depth state");
    }
    const D3D11_TEXTURE_ADDRESS_MODE modes[]{D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MIRROR};
    const D3D11_CULL_MODE culling[]{D3D11_CULL_NONE, D3D11_CULL_FRONT, D3D11_CULL_BACK};
    for (unsigned index = 0; index < 3; ++index) {
        D3D11_RASTERIZER_DESC raster{};
        raster.FillMode = D3D11_FILL_SOLID;
        raster.CullMode = culling[index];
        raster.FrontCounterClockwise = FALSE;
        raster.DepthClipEnable = TRUE;
        check(device->CreateRasterizerState(&raster, &modelRasterizers[index]), "Could not create S3D culling state");
    }
    for (unsigned index = 0; index < 9; ++index) {
        D3D11_SAMPLER_DESC sampler{};
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = modes[index / 3];
        sampler.AddressV = modes[index % 3];
        sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.MaxLOD = D3D11_FLOAT32_MAX;
        check(device->CreateSamplerState(&sampler, &modelSamplers[index]), "Could not create S3D texture sampler");
    }
    return indices;
}

void gamerenderer::drawPreview(std::size_t index, const s3dmodel::Model& model, const std::vector<s3dmodel::Batch>& batches,
    const s3denvironment::Lighting& lighting, const s3denvironment::Camera* sceneCamera, bool clearTarget, unsigned passes, bool receiver,
    const std::vector<float>& trigonometry, const s3denvironment::Fog* fog) {
    auto& preview = previews.at(index);
    auto& surface = sceneCamera ? previews.at(0) : preview;
    ID3D11ShaderResourceView* empty = nullptr;
    context->PSSetShaderResources(0, 1, &empty);
    context->OMSetRenderTargets(1, surface.target.GetAddressOf(), surface.depth.Get());
    const float clear[]{0,0,0,1};
    if (clearTarget) {
        context->ClearRenderTargetView(surface.target.Get(), clear);
        context->ClearDepthStencilView(surface.depth.Get(), D3D11_CLEAR_DEPTH, 1, 0);
    }
    const D3D11_VIEWPORT viewport{0,0,float(surface.width),float(surface.height),0,1};
    context->RSSetViewports(1, &viewport);
    context->RSSetState(solidState.Get());
    Constants camera;
    s3denvironment::Camera parameters;
    parameters.position = {0,0,300};
    parameters.target = {0,100,0};
    parameters.aspect = 4.0f / 3.0f;
    if (sceneCamera) parameters = *sceneCamera;
    const auto vector = [](const std::array<float, 3>& value) { return DirectX::XMVectorSet(value[0],value[1],value[2],0); };
    const auto view = DirectX::XMMatrixLookAtRH(vector(parameters.position), vector(parameters.target), vector(parameters.up));
    DirectX::XMFLOAT4X4 viewValues;
    DirectX::XMStoreFloat4x4(&viewValues, view);
    const std::array<float, 12> normalView{viewValues._11,viewValues._21,viewValues._31,0,
        viewValues._12,viewValues._22,viewValues._32,0,viewValues._13,viewValues._23,viewValues._33,0};
    const auto projection = DirectX::XMMatrixPerspectiveFovRH(parameters.fieldOfView * DirectX::XM_PI / 180.0f, parameters.aspect, parameters.nearPlane, parameters.farPlane);
    FogConstants fogData;
    if (fog && fog->type) {
        DirectX::XMFLOAT4X4 projectionValues;
        DirectX::XMStoreFloat4x4(&projectionValues, DirectX::XMMatrixTranspose(projection));
        std::array<float, 16> projectionArray;
        for (unsigned row = 0; row < 4; ++row) for (unsigned column = 0; column < 4; ++column)
            projectionArray[row * 4 + column] = projectionValues.m[row][column];
        const auto state = s3dfog::prepare(*fog, projectionArray, parameters.nearPlane, parameters.farPlane);
        fogData.color = state.color;
        fogData.parameters = {state.a, state.c, 2.0f * (float(state.center) - 342.0f), float(surface.width)};
        fogData.control = {state.type, state.magnitude, state.shift, unsigned(state.rangeEnabled)};
        for (unsigned index = 0; index < state.range.size(); ++index)
            fogData.ranges[index / 4][index % 4] = float(state.range[index]) * 4.0f;
    }
    context->UpdateSubresource(fogConstants.Get(), 0, nullptr, &fogData, 0, 0);
    context->PSSetConstantBuffers(3, 1, fogConstants.GetAddressOf());
    DirectX::XMStoreFloat4x4(&camera.matrix, view * projection);
    context->UpdateSubresource(constants.Get(), 0, nullptr, &camera, 0, 0);
    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, constants.GetAddressOf());
    context->PSSetShader(interfaceShader.Get(), nullptr, 0);
    context->PSSetConstantBuffers(1, 1, interfaceColors.GetAddressOf());
    const UINT stride = sizeof(Vertex), offset = 0;
    context->IASetVertexBuffers(0, 1, modelVertices.GetAddressOf(), &stride, &offset);
    context->IASetInputLayout(layout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    struct Draw {
        const s3dmodel::Batch* batch;
        unsigned pass;
        UINT first;
        bool generated;
        s3dtexturestage::Matrix textureMatrix;
    };
    std::vector<Draw> draws;
    std::size_t vertices = 0;
    for (unsigned pass = 0; pass < 2; ++pass) {
        if (!(passes & (1U << pass))) continue;
        for (const auto& batch : batches) {
            const auto& material = model.materials.at(batch.material);
            if ((s3dmodel::renderMode(material) == 2) != (pass == 1) || batch.vertices.empty() || batch.cull == 3) continue;
            if (material.wrapS > 2 || material.wrapT > 2)
                throw std::runtime_error("Invalid S3D preview batch.");
            if (batch.cull > 3) throw std::runtime_error("Invalid S3D culling mode.");
            const bool generated = material.stages.size() == 2 && material.stages[1].kind == 2;
            const auto textureMatrix = generated ? s3dtexturestage::normalMatrix(material.stages[1], normalView, trigonometry) : s3dtexturestage::Matrix{};
            draws.push_back({&batch, pass, static_cast<UINT>(vertices), generated, textureMatrix});
            vertices += batch.vertices.size();
        }
    }
    if (vertices > modelVertexCapacity) throw std::runtime_error("Invalid S3D vertex upload size.");
    if (!draws.empty()) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        check(context->Map(modelVertices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped), "Could not update S3D vertices");
        auto* destination = static_cast<Vertex*>(mapped.pData);
        for (const auto& draw : draws) {
            const auto& batch = *draw.batch;
            const auto& material = model.materials.at(batch.material);
            for (const auto& source : batch.vertices) {
                const auto color = lighting.shade(source, material, batch.lighting);
                std::array<float, 4> secondary{0, 0, 0, 1};
                if (material.secondaryColor) {
                    const auto highlight = lighting.shadeSecondary(source, material, batch.lighting, parameters);
                    secondary = {highlight[0], highlight[1], highlight[2], material.texture ? 0.5f : 0.0f};
                }
                const auto secondCoordinate = draw.generated ? s3dtexturestage::coordinate(draw.textureMatrix, source.normal) : source.coordinate;
                *destination++ = {source.position, color, source.coordinate, secondary, secondCoordinate};
            }
        }
        context->Unmap(modelVertices.Get(), 0);
        const std::array<std::array<float, 4>, 3> colors{{{0,0,0,0}, {1,1,1,1}, {1, 1.0f / 255.0f,0,0}}};
        context->UpdateSubresource(interfaceColors.Get(), 0, nullptr, colors.data(), 0, 0);
        context->OMSetBlendState(interfaceBlend.Get(), nullptr, 0xffffffff);
        for (const auto& draw : draws) {
            const auto& batch = *draw.batch;
            const auto& material = model.materials.at(batch.material);
            auto stageColors = colors;
            if (material.stages.size() == 2) {
                const auto& sampler = model.samplers.at(material.stages[1].sampler);
                context->PSSetShaderResources(3, 1, preview.textures.at(sampler.texture).GetAddressOf());
                context->PSSetSamplers(3, 1, modelSamplers[sampler.wrapS * 3 + sampler.wrapT].GetAddressOf());
                stageColors[2][2] = 1;
            }
            context->UpdateSubresource(interfaceColors.Get(), 0, nullptr, stageColors.data(), 0, 0);
            context->OMSetDepthStencilState(modelDepth[draw.pass].Get(), 0);
            context->PSSetShader(s3dmodel::renderMode(material) == 0 ? opaqueShader.Get() : interfaceShader.Get(), nullptr, 0);
            gameshadowrenderer::bind(receiver, material.color[3]);
            context->RSSetState(modelRasterizers[batch.cull].Get());
            context->PSSetShaderResources(0, 1, preview.textures.at(material.texture).GetAddressOf());
            context->PSSetSamplers(0, 1, modelSamplers[material.wrapS * 3 + material.wrapT].GetAddressOf());
            context->Draw(static_cast<UINT>(batch.vertices.size()), draw.first);
        }
    }
    context->OMSetRenderTargets(0, nullptr, nullptr);
}

ID3D11Device* gamerenderer::modelDevice() { return device.Get(); }
ID3D11DeviceContext* gamerenderer::modelContext() { return context.Get(); }

void gamerenderer::bindSceneTarget(bool clear) {
    auto& surface = previews.at(0);
    ID3D11ShaderResourceView* empty = nullptr;
    context->PSSetShaderResources(0, 1, &empty);
    context->OMSetRenderTargets(1, surface.target.GetAddressOf(), surface.depth.Get());
    if (clear) {
        const float color[]{0, 0, 0, 1};
        context->ClearRenderTargetView(surface.target.Get(), color);
        context->ClearDepthStencilView(surface.depth.Get(), D3D11_CLEAR_DEPTH, 1, 0);
    }
    const D3D11_VIEWPORT viewport{0, 0, float(surface.width), float(surface.height), 0, 1};
    context->RSSetViewports(1, &viewport);
}

void gamerenderer::waitForFrame() {
    if (!frameComplete) {
        const D3D11_QUERY_DESC description{D3D11_QUERY_EVENT, 0};
        check(device->CreateQuery(&description, &frameComplete), "Could not create frame completion query");
    }
    context->End(frameComplete.Get());
    context->Flush();
    const auto deadline = GetTickCount64() + 5000;
    HRESULT result;
    while ((result = context->GetData(frameComplete.Get(), nullptr, 0, 0)) == S_FALSE) {
        if (GetTickCount64() >= deadline) throw std::runtime_error("Frame completion timed out.");
        SwitchToThread();
    }
    check(result, "Frame completion failed");
}

void gamerenderer::close() {
    debugoverlay::close();
    if (context) context->ClearState();
    gameshadowrenderer::close();
    interfaceTextures.clear();
    previews.clear();
    sceneResources.clear();
    modelVertices.Reset();
    for (auto& state : modelDepth) state.Reset();
    for (auto& sampler : modelSamplers) sampler.Reset();
    for (auto& state : modelRasterizers) state.Reset();
    interfaceShader.Reset();
    opaqueShader.Reset();
    interfaceVertices.Reset();
    interfaceColors.Reset();
    interfaceBlend.Reset();
    interfaceSampler.Reset();
    solidState.Reset();
    constants.Reset();
    fogConstants.Reset();
    frameComplete.Reset();
    layout.Reset();
    vertexShader.Reset();
    targetView.Reset();
    swapChain.Reset();
    context.Reset();
    device.Reset();
}
