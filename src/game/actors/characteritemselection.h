#pragma once

#include "game/events/missionresources.h"
#include "game/enemies/enemytypeparameters.h"
#include "game/enemies/enemymanager.h"

namespace characteritems {
    std::uint32_t tableId(const jmp::ToolDataRef& source, unsigned type, const enemytypes::Manager& parameters);
    std::int32_t adjustTable(std::int32_t table, std::int16_t playerLife);
    jmp::ToolDataRef find(const mission::Resources& mission, std::string_view name);
    jmp::ToolDataRef select(const mission::Resources& mission, std::int32_t table, std::int16_t playerLife,
        enemies::Runtime& runtime);
}
