#include "game/world/doorresources.h"

#include <stdexcept>
#include <utility>

bool doors::ModelResource::read(const std::vector<std::uint8_t>& source, const dooranimation::PoseTables& tables,
    std::string& error) {
    error.clear();
    try {
        ModelResource result;
        result.bytes = source;
        if (!result.hierarchy.read(source, error) || !result.drawing.read(source, error)) return false;
        const dooranimation::Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        result.initialMatrices = result.hierarchy.staticPose(tables, identity);
        for (const auto& entry : result.drawing.nodes()) for (const auto& binding : entry.second.bindings) {
            const auto textureCount = result.drawing.batch(binding.batch).textureCount;
            auto material = result.materials.find(binding.material);
            if (material == result.materials.end() || material->second.samplers.size() < textureCount) {
                modelmaterial::Material decoded;
                if (!modelmaterial::read(result.bytes, binding.material, textureCount, decoded, error)) return false;
                std::vector<modelmaterial::TextureImages> images(decoded.samplers.size());
                for (unsigned slot = 0; slot < decoded.samplers.size(); ++slot) {
                    if (!decoded.samplers[slot].present) continue;
                    if (!modelmaterial::readImages(result.bytes, decoded.samplers[slot], images[slot], error)) return false;
                }
                result.textureImages[binding.material] = std::move(images);
                for (const auto& sampler : decoded.samplers) {
                    if (sampler.generatedMatrix < 0 || result.textureTransforms.count(sampler.generatedMatrix)) continue;
                    modelmaterial::TextureTransform transform;
                    if (!modelmaterial::readTransform(result.bytes, sampler.generatedMatrix, transform, error)) return false;
                    result.textureTransforms.emplace(sampler.generatedMatrix, transform);
                }
                result.materials[binding.material] = std::move(decoded);
            }
            if (result.geometry.count(binding.batch)) continue;
            std::vector<modeldraw::Primitive> primitives;
            if (!modeldraw::decodeVertices(result.bytes, result.drawing.batch(binding.batch), primitives, error)) return false;
            result.geometry.emplace(binding.batch, std::move(primitives));
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
