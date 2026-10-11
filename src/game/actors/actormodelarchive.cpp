#include "game/actors/actormodelarchive.h"

#include <stdexcept>

s3dmodel::Model& actormodel::Archive::surface() const {
    if (!decodedSurface) {
        auto decoded = std::make_unique<s3dmodel::Model>();
        std::string error;
        if (!model || !decoded->read(model->bytes, error)) throw std::runtime_error("Actor surface: " + error);
        decodedSurface = std::move(decoded);
    }
    return *decodedSurface;
}

actormodel::ResourceRef actormodel::Archive::resolve(std::string_view name) {
    const auto index = source.findNameResource(name);
    if (index < 0) {
        throw std::runtime_error("Actor archive resource lookup failed.");
    }
    const auto& entry = source.entry(static_cast<unsigned>(index));
    for (const auto& resource : resources) {
        if (resource->offset == entry.offset) {
            return resource;
        }
    }
    auto resource = std::make_shared<Resource>();
    resource->name = entry.name;
    resource->offset = entry.offset;
    resource->bytes = source.resource(static_cast<unsigned>(index));
    resources.push_back(resource);
    return resource;
}

std::vector<unsigned> actormodel::Archive::entries(std::string_view path) const {
    auto result = source.directory(path);
    result.resize(result.size() > 2 ? result.size() - 2 : 0);
    return result;
}

bool actormodel::Archive::read(const std::vector<std::uint8_t>& bytes, const ArchiveLayout& layout, std::string& error) {
    error.clear();
    try {
        Archive result;
        if (!result.source.read(bytes, error)) {
            return false;
        }
        const auto keyEntries = result.entries("/key");
        if (keyEntries.size() > layout.keys) {
            throw std::runtime_error("Actor archive exceeds its original key allocation.");
        }
        for (const auto index : keyEntries) {
            result.keys.push_back(result.resolve(result.source.entry(index).name));
        }
        const std::array<const char*, 2> morphPaths{"/sls", "/slk"};
        for (unsigned side = 0; side < morphPaths.size(); ++side) {
            const auto files = result.entries(morphPaths[side]);
            if (files.size() > result.morphs.size()) {
                result.morphs.resize(files.size());
            }
            for (unsigned i = 0; i < files.size(); ++i) {
                result.morphs[i][side] = result.resolve(result.source.entry(files[i]).name);
            }
            if (side == 1) {
                result.morphCount = static_cast<unsigned>(files.size());
            }
        }
        for (const auto index : result.entries("/txp")) {
            result.resolve(result.source.entry(index).name);
        }
        const std::array<const char*, 5> auxiliaryPaths{"/txp", "/pth", "/tmb", "/clk", "/bas"};
        for (unsigned group = 0; group < auxiliaryPaths.size(); ++group) {
            auto& destination = result.auxiliary[group];
            destination.resize(layout.auxiliary[group]);
            const auto files = result.entries(auxiliaryPaths[group]);
            if (files.empty()) {
                continue;
            }
            if (files.size() < destination.size()) {
                throw std::runtime_error("Actor archive has fewer auxiliary entries than its original table requires.");
            }
            for (unsigned i = 0; i < destination.size(); ++i) {
                destination[i] = result.resolve(result.source.entry(files[i]).name);
            }
        }
        const auto gebEntries = result.entries("/geb");
        if (!gebEntries.empty()) {
            result.geb = result.resolve(result.source.entry(gebEntries.front()).name);
        }
        const auto models = result.source.directory("/model");
        if (models.empty()) {
            throw std::runtime_error("Actor archive has no original model entry.");
        }
        result.model = result.resolve(result.source.entry(models.front()).name);
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
