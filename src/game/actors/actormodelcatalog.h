#pragma once

#include "game/actors/actormodelarchive.h"

namespace actormodel {
    struct ModelInfo {
        std::string path;
        ArchiveLayout layout;
        unsigned capacity = 0;
        std::vector<std::array<std::uint32_t, 6>> keyMetadata;
    };

    struct TypeInfo {
        unsigned model = 0;
        std::uint16_t field4 = 0;
        std::int16_t field14 = 0;
        std::uint32_t flags = 0;
        std::int32_t texture = -1;
        std::vector<std::array<std::uint32_t, 8>> animations;
        std::vector<std::string> animationNames;
    };

    class Catalog {
    public:
        bool read(const std::vector<std::uint8_t>& executable, std::string& error);
        const ModelInfo& model(unsigned index) const;
        const TypeInfo& type(unsigned index) const;
        int findAnimation(unsigned type, std::string_view name) const;

    private:
        std::array<ModelInfo, 262> models;
        std::array<TypeInfo, 381> types;
        bool loaded = false;
    };
}
