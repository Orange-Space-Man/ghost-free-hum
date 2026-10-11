#pragma once

#include "resources/gameassetpackage.h"
#include <array>

namespace openingscene {
    struct Resource {
        std::string name;
        std::vector<std::uint8_t> data;
    };

    struct ModelResources {
        unsigned flags = 0;
        Resource model;
        std::array<std::vector<Resource>, 4> animations;
        std::vector<Resource> soundAnimations;
    };

    struct Segment {
        unsigned id = 0;
        std::string archive;
        Resource scene;
        Resource settings;
        std::vector<ModelResources> models;
    };

    struct Resources {
        unsigned cutscene = 0;
        unsigned mode = 0;
        unsigned initialValue = 0;
        std::vector<Segment> segments;
        std::vector<std::array<std::uint8_t, 12>> events;
    };

    bool read(gameassets::Package& package, unsigned cutscene, Resources& resources, std::string& error);
}
