#include "platform/d3d11/modeltexturerenderer.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
    using Microsoft::WRL::ComPtr;

    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    void verify(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11ShaderResourceView* view,
        const modelmaterial::TextureImages& images) {
        ComPtr<ID3D11Resource> resource;
        view->GetResource(resource.GetAddressOf());
        ComPtr<ID3D11Texture2D> texture;
        require(SUCCEEDED(resource.As(&texture)), "Texture view does not contain a 2D texture.");
        D3D11_TEXTURE2D_DESC description{};
        texture->GetDesc(&description);
        require(description.MipLevels == images.levels.size(), "Uploaded mip count differs from stored images.");
        description.Usage = D3D11_USAGE_STAGING;
        description.BindFlags = 0;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;
        require(SUCCEEDED(device->CreateTexture2D(&description, nullptr, staging.GetAddressOf())), "Cannot create mip readback texture.");
        context->CopyResource(staging.Get(), texture.Get());
        for (unsigned mip = 0; mip < images.levels.size(); ++mip) {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            require(SUCCEEDED(context->Map(staging.Get(), mip, D3D11_MAP_READ, 0, &mapped)), "Cannot map uploaded mip level.");
            const auto& expected = images.levels[mip];
            bool equal = true;
            for (unsigned y = 0; y < expected.height; ++y) {
                const auto* row = static_cast<const std::uint8_t*>(mapped.pData) + y * mapped.RowPitch;
                equal = equal && std::equal(row, row + expected.width * 4, expected.pixels.begin() + y * expected.width * 4);
            }
            context->Unmap(staging.Get(), mip);
            require(equal, "D3D texture readback differs from original decoded pixels.");
        }
    }
}

int main(int argc, char** argv) {
    try {
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        require(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, context.GetAddressOf())), "Cannot create D3D11 WARP device.");
        doors::ModelResource model;
        modelmaterial::Sampler sampler;
        sampler.present = true;
        sampler.width = 8;
        sampler.height = 4;
        sampler.wrapS = 1;
        sampler.wrapT = 2;
        sampler.mipmapped = 1;
        sampler.maximumLod = 3;
        model.materials[0].samplers.push_back(sampler);
        modelmaterial::TextureImages images;
        unsigned width = 8, height = 4;
        for (unsigned mip = 0; mip < 4; ++mip) {
            images.levels.push_back({width, height, std::vector<std::uint8_t>(width * height * 4, static_cast<std::uint8_t>(mip * 63))});
            width = std::max(1U, width >> 1);
            height = std::max(1U, height >> 1);
        }
        model.textureImages[0].push_back(images);
        modelrenderer::Textures uploaded;
        uploaded.load(device.Get(), model);
        verify(device.Get(), context.Get(), uploaded.view(0, 0), images);
        D3D11_SAMPLER_DESC description{};
        uploaded.sampler(0, 0)->GetDesc(&description);
        require(description.AddressU == D3D11_TEXTURE_ADDRESS_WRAP && description.AddressV == D3D11_TEXTURE_ADDRESS_MIRROR &&
            description.Filter == D3D11_FILTER_MIN_MAG_MIP_LINEAR && description.MipLODBias == -0.28125f && description.MaxLOD == 3,
            "D3D sampler does not preserve original addressing, filtering and quantized bias.");
        auto program = modelmaterial::buildStages(model.materials[0], 1, 0, false, false, 0, 2);
        uploaded.bind(context.Get(), 0, program);
        ComPtr<ID3D11ShaderResourceView> bound;
        context->PSGetShaderResources(2, 1, bound.GetAddressOf());
        require(bound.Get() == uploaded.view(0, 0), "Stage texture slot was not bound to D3D.");
        auto empty = modelmaterial::buildStages(model.materials[0], 0, 0, false, false);
        uploaded.bind(context.Get(), 0, empty);
        bound.Reset();
        context->PSGetShaderResources(2, 1, bound.GetAddressOf());
        require(bound.Get() == uploaded.view(0, 0), "Untextured stage incorrectly cleared retained texture state.");
        model.textureImages[0][0].levels[1].pixels.clear();
        bool rejected = false;
        auto* retained = uploaded.view(0, 0);
        try { uploaded.load(device.Get(), model); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected && uploaded.view(0, 0) == retained, "Failed texture replacement did not retain the prior GPU resource.");
        if (argc > 1) {
            gameassets::Package package;
            std::string error;
            std::vector<std::uint8_t> compressed, archive, executable;
            require(package.open(argv[1], error) && package.read("files/Game/game_usa.szp", compressed, error) &&
                roomarchive::decompress(compressed, archive, error) && package.read("sys/main.dol", executable, error), error.c_str());
            doors::Resources resources;
            require(resources.load(archive, executable, error), error.c_str());
            unsigned count = 0;
            for (unsigned index = 0; index < 15; ++index) {
                const auto& source = resources.model(index);
                uploaded.load(device.Get(), source);
                for (const auto& material : source.materials) {
                    for (unsigned slot = 0; slot < material.second.samplers.size(); ++slot) {
                        if (!material.second.samplers[slot].present) continue;
                        verify(device.Get(), context.Get(), uploaded.view(material.first, slot), source.textureImages.at(material.first)[slot]);
                        ++count;
                    }
                }
            }
            std::cout << "Verified " << count << " owned texture bindings across fifteen models by D3D readback.\n";
        }
        context->ClearState();
        std::cout << "D3D mip uploads, sampler state, binding and replacement checks passed.\n";
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
