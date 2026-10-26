#include "platform/d3d11/j3dtexturerenderer.h"
#include "game/world/roommaterials.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {
    void check(HRESULT result, const char* message) { if (FAILED(result)) throw std::runtime_error(message); }
    unsigned half(const std::vector<std::uint8_t>& bytes, std::size_t offset) { return (bytes.at(offset) << 8) | bytes.at(offset + 1); }
    unsigned word(const std::vector<std::uint8_t>& bytes, std::size_t offset) { return (half(bytes, offset) << 16) | half(bytes, offset + 2); }
    int signedByte(unsigned value) { return value < 128 ? int(value) : int(value) - 256; }
    D3D11_TEXTURE_ADDRESS_MODE address(unsigned mode) {
        if (mode > 2) throw std::runtime_error("Invalid J3D texture wrap mode.");
        const D3D11_TEXTURE_ADDRESS_MODE modes[]{D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MIRROR};
        return modes[mode];
    }
}

void j3drenderer::Textures::load(ID3D11Device* device, const j3dmodel::Geometry& geometry) {
    if (!device || !geometry.bytes) throw std::runtime_error("J3D texture upload requires a device and owned resource.");
    Textures result;
    result.owner = device;
    const auto& bytes = *geometry.bytes;
    for (unsigned header : geometry.textureHeaders) {
        const auto byte = [&](unsigned index) { return bytes.at(std::size_t(header) + index); };
        const unsigned format = byte(0), baseWidth = half(bytes, header + 2), baseHeight = half(bytes, header + 4);
        if (!baseWidth || !baseHeight || baseWidth > 1024 || baseHeight > 1024 || (format > 6 && format != 14))
            throw std::runtime_error("Unsupported J3D texture dimensions or format.");
        if (byte(8) || byte(17) || byte(18) || byte(19))
            throw std::runtime_error("J3D texture requires palette or unsupported sampler controls.");
        const unsigned minification = byte(20), magnification = byte(21);
        if (minification > 5 || magnification > 1) throw std::runtime_error("Invalid J3D texture filter.");
        const unsigned levels = byte(16) ? byte(24) : 1;
        if (!levels || levels > 11) throw std::runtime_error("Invalid J3D texture mip count.");
        std::size_t cursor = std::size_t(header) + word(bytes, header + 28);
        std::vector<roommaterials::Texture> decoded;
        unsigned width = baseWidth, height = baseHeight;
        for (unsigned level = 0; level < levels; ++level) {
            const unsigned tileWidth = format <= 2 || format == 14 ? 8 : 4;
            const unsigned tileHeight = format == 1 || format == 2 ? 4 : tileWidth;
            const std::size_t length = std::size_t((width + tileWidth - 1) / tileWidth) * ((height + tileHeight - 1) / tileHeight) * (format == 6 ? 64 : 32);
            if (cursor > bytes.size() || length > bytes.size() - cursor) throw std::runtime_error("Truncated J3D texture mip data.");
            std::vector<std::uint8_t> encoded(bytes.begin() + cursor, bytes.begin() + cursor + length);
            roommaterials::Texture image;
            std::string error;
            if (!roommaterials::decodeTexture(encoded, width, height, format, image, error)) throw std::runtime_error(error);
            decoded.push_back(std::move(image));
            cursor += length;
            if (level + 1 < levels && width == 1 && height == 1) throw std::runtime_error("J3D texture mip chain exceeds its dimensions.");
            width = std::max(1U, width / 2); height = std::max(1U, height / 2);
        }
        std::vector<D3D11_SUBRESOURCE_DATA> data;
        for (const auto& image : decoded) data.push_back({image.pixels.data(), image.width * 4, 0});
        D3D11_TEXTURE2D_DESC description{};
        description.Width = baseWidth; description.Height = baseHeight; description.MipLevels = levels; description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM; description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_IMMUTABLE; description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> image;
        check(device->CreateTexture2D(&description, data.data(), image.GetAddressOf()), "Cannot upload J3D texture.");
        Texture texture;
        check(device->CreateShaderResourceView(image.Get(), nullptr, texture.view.GetAddressOf()), "Cannot create J3D texture view.");
        D3D11_SAMPLER_DESC sampling{};
        const unsigned minLinear = minification & 1, mipLinear = minification >= 4;
        sampling.Filter = static_cast<D3D11_FILTER>((minLinear << 4) | (magnification << 2) | mipLinear);
        sampling.AddressU = address(byte(6)); sampling.AddressV = address(byte(7)); sampling.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        const unsigned biasBits = half(bytes, header + 26);
        const float bias = (biasBits < 32768 ? int(biasBits) : int(biasBits) - 65536) * 0.01f;
        sampling.MipLODBias = float(int(std::clamp(bias, -4.0f, 3.99f) * 32)) / 32;
        sampling.MinLOD = std::clamp(signedByte(byte(22)) * 0.125f, 0.0f, 10.0f);
        sampling.MaxLOD = minification < 2 ? 0 : std::clamp(signedByte(byte(23)) * 0.125f, 0.0f, 10.0f);
        sampling.MaxAnisotropy = 1;
        check(device->CreateSamplerState(&sampling, texture.sampler.GetAddressOf()), "Cannot create J3D sampler.");
        result.textures.push_back(std::move(texture));
    }
    *this = std::move(result);
}

void j3drenderer::Textures::bind(ID3D11DeviceContext* context, const j3dmodel::MaterialState& state) const {
    if (!context || !owner) throw std::runtime_error("J3D textures are not ready.");
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());
    if (device.Get() != owner.Get()) throw std::runtime_error("J3D texture context belongs to another device.");
    std::array<ID3D11ShaderResourceView*, 8> views{};
    std::array<ID3D11SamplerState*, 8> samplers{};
    for (unsigned i = 0; i < state.combiner.count; ++i) {
        const int slot = state.combiner.stages.at(i).order[1];
        if (slot == 255) continue;
        if (slot < 0 || slot >= 8) throw std::runtime_error("Invalid J3D sampled texture slot.");
        const auto& texture = textures.at(state.combiner.textures[slot]);
        views[slot] = texture.view.Get(); samplers[slot] = texture.sampler.Get();
    }
    context->PSSetShaderResources(0, 8, views.data());
    context->PSSetSamplers(0, 8, samplers.data());
}

ID3D11ShaderResourceView* j3drenderer::Textures::view(unsigned index) const { return textures.at(index).view.Get(); }
ID3D11SamplerState* j3drenderer::Textures::sampler(unsigned index) const { return textures.at(index).sampler.Get(); }
