#include "game/models/modeltextureimages.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

bool modelmaterial::readImages(const std::vector<std::uint8_t>& model, const Sampler& sampler,
    TextureImages& images, std::string& error) {
    error.clear();
    try {
        if (!sampler.present || !sampler.width || !sampler.height || sampler.width > 1024 || sampler.height > 1024)
            throw std::runtime_error("Invalid model texture image dimensions or sampler.");
        if (sampler.format > 6 && sampler.format != 14)
            throw std::runtime_error("Unsupported model texture image format.");
        if (sampler.mipmapped && (sampler.maximumLod < 0 || sampler.maximumLod > 10))
            throw std::runtime_error("Model texture LOD requires unsupported GX clamping behavior.");
        const unsigned lastLevel = sampler.mipmapped ? unsigned(sampler.maximumLod) : 0;
        const unsigned tileWidth = sampler.format <= 2 || sampler.format == 14 ? 8 : 4;
        const unsigned tileHeight = sampler.format == 1 || sampler.format == 2 ? 4 : tileWidth;
        const unsigned tileBytes = sampler.format == 6 ? 64 : 32;
        unsigned width = sampler.width, height = sampler.height;
        std::size_t cursor = sampler.imageOffset;
        TextureImages result;
        for (unsigned level = 0; level <= lastLevel; ++level) {
            const std::size_t size = std::size_t((width + tileWidth - 1) / tileWidth) *
                ((height + tileHeight - 1) / tileHeight) * tileBytes;
            if (cursor > model.size() || size > model.size() - cursor)
                throw std::runtime_error("Truncated model texture mip level.");
            std::vector<std::uint8_t> encoded(model.begin() + cursor, model.begin() + cursor + size);
            roommaterials::Texture decoded;
            if (!roommaterials::decodeTexture(encoded, width, height, sampler.format, decoded, error)) return false;
            result.offsets.push_back(cursor);
            result.levels.push_back(std::move(decoded));
            cursor += size;
            if (level == lastLevel) break;
            if (width == 1 && height == 1)
                throw std::runtime_error("Model mip range exceeds its image dimensions.");
            width = std::max(1U, width >> 1);
            height = std::max(1U, height >> 1);
        }
        result.encodedSize = cursor - sampler.imageOffset;
        images = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
