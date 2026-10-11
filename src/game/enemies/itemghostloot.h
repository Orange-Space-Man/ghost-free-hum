#pragma once

#include "game/events/missionresources.h"
#include "game/enemies/enemytypecatalog.h"

#include <optional>

namespace itemghost {
    struct Drop {
        enum class Kind { coin, bill, actor };
        Kind kind;
        std::uint32_t count;
        unsigned type;
    };

    struct Loot {
        std::vector<Drop> drops;
        std::optional<std::string> other;
    };

    jmp::ToolDataRef findLoot(const mission::Resources& mission, const enemytypes::Catalog& types, unsigned type);
    Loot readLoot(const jmp::ToolDataRef& source);
}
