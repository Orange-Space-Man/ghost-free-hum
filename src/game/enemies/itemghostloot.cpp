#include "game/enemies/itemghostloot.h"

#include <stdexcept>

jmp::ToolDataRef itemghost::findLoot(const mission::Resources& mission, const enemytypes::Catalog& types, unsigned type) {
    const auto* table = mission.getJmpResource("iyapootable");
    if (!table) throw std::runtime_error("Original item ghost loot requires IyapooTable.");
    return {table, table->findEntryByValue(table->searchItemInfo("name"), types.name(type))};
}

itemghost::Loot itemghost::readLoot(const jmp::ToolDataRef& source) {
    if (!source.table || source.row < 0) throw std::runtime_error("Original item ghost loot row is not bound.");
    constexpr std::array<const char*, 12> names{
        "coin", "bill", "gold", "SPearl", "mPearl", "LPearl", "sapphire", "emerald", "ruby", "diamond", "cdiamond", "rdiamond"};
    constexpr std::array<unsigned, 12> types{0, 0, 0xcb, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca};
    Loot result;
    for (unsigned i = 0; i < names.size(); ++i) {
        std::uint32_t count = 0;
        if (!source.table->getValue(source.row, names[i], count)) continue;
        result.drops.push_back({i == 0 ? Drop::Kind::coin : i == 1 ? Drop::Kind::bill : Drop::Kind::actor,
            i < 2 ? static_cast<std::uint8_t>(count) : count, types[i]});
    }
    std::string_view other;
    if (source.table->getValue(source.row, "other", other)) result.other = std::string(other);
    return result;
}
