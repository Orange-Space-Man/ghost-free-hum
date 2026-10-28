#pragma once

#include "game/world/doorresources.h"
#include "game/scene/gamearchive.h"
#include "game/events/jmptooldata.h"
#include "game/events/runtimeflags.h"

namespace roomfurniture {
    std::vector<std::uint16_t> selectRows(const jmp::ToolData& table, unsigned room, const gameflags::State& flags);

    struct Placement {
        std::uint16_t row = 0, model = 0xffff;
        std::uint32_t behavior = 0;
    };

    struct Animation {
        std::string name;
        dooranimation::Resource animation;
        std::vector<std::uint8_t> sound;
    };

    class Resources {
    public:
        void load(const gamearchive::Archive& archive, const jmp::ToolData& table, unsigned room,
            const gameflags::State& flags, const dooranimation::PoseTables& poseTables);
        const std::vector<Placement>& placements() const;
        const std::vector<doors::ModelResource>& models() const;
        const std::vector<Animation>& animations() const;

    private:
        std::vector<Placement> entries;
        std::vector<doors::ModelResource> modelResources;
        std::vector<Animation> animationResources;
    };
}
