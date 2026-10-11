#pragma once

#include "game/scene/gamearchive.h"
#include "game/models/s3dmodel.h"

#include <array>
#include <memory>

namespace actormodel {
    struct Resource {
        std::string name;
        unsigned offset = 0;
        std::vector<std::uint8_t> bytes;
    };

    using ResourceRef = std::shared_ptr<const Resource>;

    struct ArchiveLayout {
        unsigned keys = 0;
        std::array<unsigned, 5> auxiliary{};
    };

    class Archive {
    public:
        bool read(const std::vector<std::uint8_t>& bytes, const ArchiveLayout& layout, std::string& error);
        ResourceRef model;
        std::vector<ResourceRef> keys;
        std::vector<std::array<ResourceRef, 2>> morphs;
        unsigned morphCount = 0;
        std::array<std::vector<ResourceRef>, 5> auxiliary;
        ResourceRef geb;
        s3dmodel::Model& surface() const;

    private:
        gamearchive::Archive source;
        std::vector<ResourceRef> resources;
        mutable std::unique_ptr<s3dmodel::Model> decodedSurface;
        ResourceRef resolve(std::string_view name);
        std::vector<unsigned> entries(std::string_view path) const;
    };
}
