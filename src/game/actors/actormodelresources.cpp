#include "game/actors/actormodelresources.h"

#include <stdexcept>

actormodel::Resources::Resources(const Catalog& catalog) : catalog(catalog) {}

void actormodel::Resources::initializeEmbedded(const gamearchive::Archive& common) {
    for (auto& model : models) {
        model.state = 0;
        model.archive.reset();
    }
    for (unsigned type = 0; type < 381; ++type) {
        const auto index = catalog.type(type).model;
        auto& model = models.at(index);
        if (model.archive) {
            continue;
        }
        const auto& info = catalog.model(index);
        auto path = info.path;
        const auto dot = path.find('.');
        if (dot < 32 && dot + 3 < path.size()) {
            path.replace(dot + 1, 3, "arc");
        }
        const auto resource = common.findPathResource(path);
        if (resource < 0) {
            continue;
        }
        model.state = 1;
        model.archive = std::make_unique<Archive>();
        std::string error;
        if (!model.archive->read(common.resource(static_cast<unsigned>(resource)), info.layout, error)) {
            throw std::runtime_error(info.path + ": " + error);
        }
        model.state = 3;
    }
}

const actormodel::Entry& actormodel::Resources::entry(unsigned model) const {
    return models.at(model);
}

actormodel::ResourceRef actormodel::Resources::path(unsigned model, unsigned key) const {
    const auto index = static_cast<std::int32_t>(catalog.model(model).keyMetadata.at(key)[2]);
    if (index < 0 || static_cast<unsigned>(index) >= catalog.model(model).layout.auxiliary[1]) return nullptr;
    const auto& entry = models.at(model);
    return entry.archive ? entry.archive->auxiliary[1].at(index) : nullptr;
}

actormodel::ResourceRef actormodel::Resources::key(unsigned type, unsigned animation) const {
    const auto& info = catalog.type(type);
    const auto& model = models.at(info.model);
    const auto index = info.animations.at(animation)[0];
    if (!model.archive) {
        if (index >= catalog.model(info.model).layout.keys) {
            throw std::out_of_range("Actor animation exceeds its original key table.");
        }
        return nullptr;
    }
    return model.archive->keys.at(index);
}

actormodel::ResourceRef actormodel::Resources::timing(unsigned model, unsigned key) const {
    const auto index = static_cast<std::int32_t>(catalog.model(model).keyMetadata.at(key)[3]);
    if (index < 0 || static_cast<unsigned>(index) >= catalog.model(model).layout.auxiliary[2]) return nullptr;
    const auto& entry = models.at(model);
    return entry.archive ? entry.archive->auxiliary[2].at(index) : nullptr;
}
