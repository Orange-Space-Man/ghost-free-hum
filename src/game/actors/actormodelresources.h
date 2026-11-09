#pragma once

#include "game/actors/actormodelcatalog.h"
#include <functional>

namespace actormodel {
    struct Entry {
        unsigned state = 0;
        std::unique_ptr<Archive> archive;
        bool embedded = false;
    };

    class Resources {
    public:
        explicit Resources(const Catalog& catalog);
        void initializeEmbedded(const gamearchive::Archive& common);
        bool request(unsigned model);
        void complete(unsigned model, const std::vector<std::uint8_t>& bytes);
        bool release(unsigned model);
        void update(const std::array<std::uint8_t, 262>& mask,
            const std::function<std::vector<std::uint8_t>(std::string_view)>& load);
        const Entry& entry(unsigned model) const;
        ResourceRef key(unsigned type, unsigned animation) const;
        ResourceRef path(unsigned model, unsigned key) const;
        ResourceRef timing(unsigned model, unsigned key) const;

    private:
        const Catalog& catalog;
        std::array<Entry, 262> models;
    };
}
