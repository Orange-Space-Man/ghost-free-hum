#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace modelmaterial {
    struct Sampler {
        bool present = false;
        std::uint8_t wrapS = 0, wrapT = 0, pass = 0, mipmapped = 0, normalMapping = 0, format = 0;
        std::int16_t maximumLod = 0;
        std::uint16_t width = 0, height = 0;
        std::size_t imageOffset = 0;
        int generatedMatrix = -1;
    };

    struct Material {
        std::uint8_t colorSource = 0;
        std::array<std::uint8_t, 4> color{};
        bool specular = false;
        float shininess = 0;
        std::vector<Sampler> samplers;
    };

    struct SamplerParameters {
        unsigned minimumFilter = 1, maximumFilter = 1;
        float minimumLod = 0, maximumLod = 0, lodBias = 0;
        bool biasClamp = false, edgeLod = false;
        unsigned anisotropy = 0;
    };

    struct Channel {
        unsigned enabled = 0, ambientSource = 0, materialSource = 0, lightMask = 0, diffuse = 0, attenuation = 0;
    };

    struct LightingState {
        unsigned lightMask = 0, attenuation = 0, specularMask = 0, specularAttenuation = 0;
        std::uint8_t lightingFlags = 0;
        bool overrideColor = false;
        std::array<std::uint8_t, 4> color{}, overrideColorValue{}, secondaryColor{};
        std::array<Channel, 4> channels{};
        unsigned channelCount = 0;
    };

    struct TextureStep {
        unsigned slot = 0, combine = 0;
    };

    struct TexturePlan {
        bool untextured = false, appendNormalMapping = false;
        unsigned normalSlot = 0;
        std::vector<TextureStep> steps;
    };

    bool read(const std::vector<std::uint8_t>& bytes, std::int16_t index, unsigned textureCount,
        Material& material, std::string& error);
    bool configureChannels(const Material& material, std::uint8_t opacity, std::uint8_t lighting,
        LightingState& state, const std::function<void(float)>& configureSpecular);
    TexturePlan texturePlan(const Material& material, unsigned textureCount, std::uint8_t normalMode);
    SamplerParameters samplerParameters(const Sampler& sampler);
}
