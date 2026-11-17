#pragma once

#include "game/effects/particleeffectfactory.h"
#include <functional>

namespace particles {
    class TrainingFireworks {
    public:
        TrainingFireworks(const std::vector<std::uint8_t>& executable, ArchiveResources& resources,
            EmitterPool& pool, std::function<void(unsigned)> sound);
        bool start(std::string_view command);
        void update(unsigned map);

    private:
        ArchiveResources& resources;
        EmitterPool& pool;
        std::function<void(unsigned)> sound;
        std::array<std::array<float, 3>, 27> positions{};
        std::array<Resource, 4> parameters;
        Resource texture;
        unsigned variant = 0, frame = 0, repeats = 0;
        bool active = false;
    };
}
