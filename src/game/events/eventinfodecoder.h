#pragma once

#include "game/enemies/enemytypecatalog.h"
#include "game/events/eventconditions.h"
#include "game/events/jmptooldata.h"

namespace gameevent {
    struct Definition {
        std::array<float, 3> position{};
        std::uint32_t eventNumber = 0;
        std::uint32_t trigger = 0;
        std::uint32_t area = 0;
        Conditions conditions;
    };

    bool readDefinitions(const jmp::ToolData& table, const enemytypes::Catalog& types,
        std::vector<Definition>& definitions, std::string& error);
}
