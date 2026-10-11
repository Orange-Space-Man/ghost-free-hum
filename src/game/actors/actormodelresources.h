#pragma once

#include "game/actors/actormodelcatalog.h"

namespace actormodel {
    struct Entry {
        unsigned state = 0;
        std::unique_ptr<Archive> archive;
    };

    class Resources {
    public:
        explicit Resources(const Catalog& catalog);
        void initializeEmbedded(const gamearchive::Archive& common);
        const Entry& entry(unsigned model) const;
        ResourceRef key(unsigned type, unsigned animation) const;
        ResourceRef path(unsigned model, unsigned key) const;
        ResourceRef timing(unsigned model, unsigned key) const;

    private:
        const Catalog& catalog;
        std::array<Entry, 262> models;
    };
}
