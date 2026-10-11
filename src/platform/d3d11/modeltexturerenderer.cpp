#include "platform/d3d11/modeltexturerenderer.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {
    using Microsoft::WRL::ComPtr;

    void check(HRESULT result, const char* message) {
        if (FAILED(result)) throw std::runtime_error(message);
    }

    D3D11_TEXTURE_ADDRESS_MODE address(unsigned value) {
        if (value == 0) return D3D11_TEXTURE_ADDRESS_CLAMP;
        if (value == 1) return D3D11_TEXTURE_ADDRESS_WRAP;
        if (value == 2) return D3D11_TEXTURE_ADDRESS_MIRROR;
        throw std::runtime_error("Unsupported original texture address mode.");
    }
}

void modelrenderer::Textures::load(ID3D11Device* device, const doors::ModelResource& model) {
    if (!device) throw std::runtime_error("Model texture upload requires a D3D device.");
    Textures result;
    result.owner = device;
    for (const auto& entry : model.materials) {
        auto& textures = result.materials[entry.first];
        textures.resize(entry.second.samplers.size());
        for (unsigned slot = 0; slot < textures.size(); ++slot) {
            const auto& source = entry.second.samplers[slot];
            if (!source.present) continue;
            const auto& images = model.textureImages.at(entry.first).at(slot);
            if (images.levels.empty() || images.levels.size() > 11)
                throw std::runtime_error("Model texture has no supported mip chain.");
            std::vector<D3D11_SUBRESOURCE_DATA> data;
            unsigned width = source.width, height = source.height;
            for (const auto& level : images.levels) {
                if (level.width != width || level.height != height || level.pixels.size() != std::size_t(width) * height * 4)
                    throw std::runtime_error("Model texture mip dimensions or pixels are inconsistent.");
                data.push_back({level.pixels.data(), width * 4, 0});
                width = std::max(1U, width >> 1);
                height = std::max(1U, height >> 1);
            }
            D3D11_TEXTURE2D_DESC description{};
            description.Width = source.width;
            description.Height = source.height;
            description.MipLevels = static_cast<UINT>(data.size());
            description.ArraySize = 1;
            description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            description.SampleDesc.Count = 1;
            description.Usage = D3D11_USAGE_IMMUTABLE;
            description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            ComPtr<ID3D11Texture2D> image;
            check(device->CreateTexture2D(&description, data.data(), image.GetAddressOf()), "Cannot upload original model texture mips.");
            auto& texture = textures[slot];
            check(device->CreateShaderResourceView(image.Get(), nullptr, texture.view.GetAddressOf()), "Cannot create model texture view.");
            const auto parameters = modelmaterial::samplerParameters(source);
            D3D11_SAMPLER_DESC sampler{};
            sampler.Filter = source.mipmapped ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            sampler.AddressU = address(source.wrapS);
            sampler.AddressV = address(source.wrapT);
            sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.MipLODBias = static_cast<float>(static_cast<int>(parameters.lodBias * 32)) / 32;
            sampler.MinLOD = 0;
            sampler.MaxLOD = std::clamp(parameters.maximumLod, 0.0f, 10.0f);
            sampler.MaxAnisotropy = 1;
            sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
            check(device->CreateSamplerState(&sampler, texture.sampler.GetAddressOf()), "Cannot create original model sampler.");
        }
    }
    *this = std::move(result);
}

void modelrenderer::Textures::bind(ID3D11DeviceContext* context, std::int16_t material,
    const modelmaterial::StageProgram& program) const {
    if (!context || !owner) throw std::runtime_error("Model textures are not ready for binding.");
    ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());
    if (device.Get() != owner.Get()) throw std::runtime_error("Model texture context belongs to another device.");
    for (const auto& binding : program.textures) {
        if (binding.texture >= 8 || !view(material, binding.slot) || !sampler(material, binding.slot))
            throw std::runtime_error("Model stage references an unavailable texture binding.");
    }
    for (const auto& binding : program.textures) {
        auto* image = view(material, binding.slot);
        auto* state = sampler(material, binding.slot);
        context->PSSetShaderResources(binding.texture, 1, &image);
        context->PSSetSamplers(binding.texture, 1, &state);
    }
}

ID3D11ShaderResourceView* modelrenderer::Textures::view(std::int16_t material, unsigned slot) const {
    return materials.at(material).at(slot).view.Get();
}

ID3D11SamplerState* modelrenderer::Textures::sampler(std::int16_t material, unsigned slot) const {
    return materials.at(material).at(slot).sampler.Get();
}
