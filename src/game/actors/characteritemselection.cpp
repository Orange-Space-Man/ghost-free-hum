#include "game/actors/characteritemselection.h"

#include <stdexcept>

std::uint32_t characteritems::tableId(const jmp::ToolDataRef& source, unsigned type, const enemytypes::Manager& parameters) {
    std::uint32_t table = 0;
    if (!source.table || source.row < 0 || !source.table->getValue(source.row, "item_table", table) || !table) {
        return parameters.parameters(type).entries().at(21).bits;
    }
    return table;
}

std::int32_t characteritems::adjustTable(std::int32_t table, std::int16_t playerLife) {
    if (playerLife < 31) {
        if (table == 2) return 12;
        if (table == 3) return 13;
    }
    return table;
}

jmp::ToolDataRef characteritems::find(const mission::Resources& mission, std::string_view name) {
    const auto* table = mission.getJmpResource("iteminfotable");
    if (!table) throw std::runtime_error("Original item lookup requires ItemInfoTable.");
    const auto row = table->findEntryByValue(table->searchItemInfo("name"), name);
    return row < 0 ? jmp::ToolDataRef{} : jmp::ToolDataRef{table, row};
}

jmp::ToolDataRef characteritems::select(const mission::Resources& mission, std::int32_t table, std::int16_t playerLife,
    enemies::Runtime& runtime) {
    const auto* choices = mission.getJmpResource("itemappeartable");
    if (!choices) return {};
    const auto row = adjustTable(table, playerLife);
    if (row >= static_cast<std::int32_t>(choices->count())) return {};
    const auto random = runtime.random();
    const auto count = choices->fieldCount();
    if (!count) throw std::runtime_error("Original item selection has no choice fields.");
    return find(mission, choices->fieldString(static_cast<unsigned>(row), random % count));
}
