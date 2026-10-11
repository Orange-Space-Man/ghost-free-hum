#include "game/models/modelmaterialstate.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace {
    void range(const std::vector<std::uint8_t>& bytes, std::size_t offset, std::size_t size) {
        if (offset > bytes.size() || size > bytes.size() - offset)
            throw std::runtime_error("Model material data exceeds its resource.");
    }

    unsigned half(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        range(bytes, offset, 2);
        return (unsigned(bytes[offset]) << 8) | bytes[offset + 1];
    }

    int signedHalf(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        const auto value = half(bytes, offset);
        return value < 0x8000 ? int(value) : int(value) - 0x10000;
    }

    std::uint32_t word(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        return (std::uint32_t(half(bytes, offset)) << 16) | half(bytes, offset + 2);
    }
}

bool modelmaterial::read(const std::vector<std::uint8_t>& bytes, std::int16_t index, unsigned textureCount,
    Material& material, std::string& error) {
    error.clear();
    try {
        if (index < 0 || textureCount > 8) throw std::runtime_error("Unsupported model material index or texture count.");
        const auto offset = std::size_t(word(bytes, 0x34)) + std::size_t(index) * 0x28;
        range(bytes, offset, 0x28);
        Material result;
        result.colorSource = bytes[offset];
        std::copy_n(bytes.begin() + offset + 3, 4, result.color.begin());
        result.specular = bytes[offset + 0x18] != 0;
        const auto bits = word(bytes, offset + 0x1c);
        std::memcpy(&result.shininess, &bits, sizeof(bits));
        const auto samplers = word(bytes, 0x10), textures = word(bytes, 0xc);
        for (unsigned slot = 0; slot < textureCount; ++slot) {
            Sampler sampler;
            const int samplerIndex = signedHalf(bytes, offset + 8 + slot * 2);
            if (samplerIndex < -1) throw std::runtime_error("Invalid signed model sampler index.");
            if (samplerIndex != -1) {
                const auto record = std::size_t(samplers) + std::size_t(samplerIndex) * 0x14;
                range(bytes, record, 0x14);
                const int textureIndex = signedHalf(bytes, record);
                if (textureIndex < 0) throw std::runtime_error("Invalid signed model texture index.");
                const auto texture = std::size_t(textures) + std::size_t(textureIndex) * 12;
                range(bytes, texture, 12);
                sampler.present = true;
                sampler.wrapS = bytes[record + 4];
                sampler.wrapT = bytes[record + 5];
                sampler.pass = bytes[record + 6];
                sampler.mipmapped = bytes[record + 7];
                sampler.maximumLod = static_cast<std::int16_t>(signedHalf(bytes, record + 8));
                sampler.normalMapping = bytes[record + 10];
                sampler.width = static_cast<std::uint16_t>(half(bytes, texture));
                sampler.height = static_cast<std::uint16_t>(half(bytes, texture + 2));
                sampler.format = bytes[texture + 4];
                sampler.imageOffset = std::size_t(textures) + word(bytes, texture + 8);
                range(bytes, sampler.imageOffset, 0);
            }
            if (word(bytes, 0x48) & 1) {
                const int group = signedHalf(bytes, offset + 0x1a);
                if (group < 0) throw std::runtime_error("Invalid model texture-coordinate group.");
                const auto record = std::size_t(word(bytes, 0x40)) + std::size_t(group) * 0x20 + slot * 2 + 4;
                const int matrix = signedHalf(bytes, record);
                if (matrix < -1) throw std::runtime_error("Invalid model generated texture matrix.");
                if (matrix != -1 && (word(bytes, std::size_t(word(bytes, 0x44)) + std::size_t(matrix) * 0x30) & 1))
                    sampler.generatedMatrix = matrix;
            }
            result.samplers.push_back(sampler);
        }
        material = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool modelmaterial::configureChannels(const Material& material, std::uint8_t opacity, std::uint8_t lighting,
    LightingState& state, const std::function<void(float)>& configureSpecular) {
    if (material.specular) {
        if (!configureSpecular) throw std::runtime_error("Specular material requires the original light adapter.");
        configureSpecular(material.shininess);
        state.specularMask = 0x40;
        state.specularAttenuation = 0;
    }
    const unsigned source = material.colorSource != 1;
    state.channels[0] = {state.lightingFlags ? 0U : unsigned(lighting), source, source, state.lightMask, 2, state.attenuation};
    state.channels[2] = {0, source, source, state.lightMask, 2, state.attenuation};
    const bool appendSpecular = lighting == 1 && material.specular;
    if (appendSpecular) state.channels[1] = {lighting, source, source, state.specularMask, 2, state.specularAttenuation};
    state.color = state.overrideColor ? state.overrideColorValue : material.color;
    state.color[3] = std::min(state.color[3], opacity);
    state.channelCount = 2;
    state.channels[3] = {1, 0, 0, 0x80, 2, 1};
    return appendSpecular;
}

modelmaterial::SamplerParameters modelmaterial::samplerParameters(const Sampler& sampler) {
    if (!sampler.present) throw std::runtime_error("Cannot configure an absent model sampler.");
    SamplerParameters result;
    if (sampler.mipmapped) {
        result.minimumFilter = 5;
        result.maximumLod = static_cast<float>(sampler.maximumLod);
        result.lodBias = -0.3f;
    }
    return result;
}

modelmaterial::TexturePlan modelmaterial::texturePlan(const Material& material, unsigned textureCount, std::uint8_t normalMode) {
    textureCount &= 255;
    if (textureCount > material.samplers.size()) throw std::runtime_error("Model material sampler slots are not loaded.");
    TexturePlan result;
    result.untextured = textureCount == 0;
    result.appendNormalMapping = normalMode == 1;
    for (unsigned slot = 0; slot < textureCount; ++slot) {
        if (normalMode && material.samplers[slot].normalMapping) result.normalSlot = slot;
        else result.steps.push_back({slot, !normalMode && !slot ? 0U : 2U});
    }
    return result;
}
