#pragma once

#include "game/scene/gamearchive.h"
#include "game/effects/particleeffectfactory.h"

#include <map>

namespace gamearchive {
    class Resources final : public particles::ArchiveResources {
    public:
        explicit Resources(const Archive& archive);
        particles::Resource get(std::uint32_t type, std::string_view name) override;

    private:
        const Archive& archive;
        std::map<unsigned, particles::Resource> entries;
    };
}
