#include "game/events/missioneventactivation.h"

#include <stdexcept>

gameevent::MissionActivation::MissionActivation(const mission::Resources& resources, const enemytypes::Catalog& types,
    const gameentry::State& entry, const gameflags::State& flags) : flags(flags) {
    if (!entry.initialized) throw std::invalid_argument("Mission event activation requires a selected save record.");
    active.reset();
    const auto* table = resources.getJmpResource("EventInfo");
    if (!table) return;
    std::string error;
    if (!readDefinitions(*table, types, definitions, error)) throw std::runtime_error(error);
    for (const auto& definition : definitions) {
        const auto number = static_cast<std::uint16_t>(definition.eventNumber);
        if (number >= 110) throw std::out_of_range("Original mission event number exceeds activation storage.");
        ConditionState record;
        record.eventNumber = static_cast<std::int16_t>(number);
        active.configure(record, definition.conditions, entry.inventory.at(0x20 + number), flags);
        records.push_back(record);
    }
}

void gameevent::MissionActivation::updateAppearance(std::int32_t elapsedSeconds) {
    for (const auto& record : records) active.updateAppearance(record, flags, elapsedSeconds);
}

const gameevent::ActivationState& gameevent::MissionActivation::activation() const { return active; }
const std::vector<gameevent::ConditionState>& gameevent::MissionActivation::conditions() const { return records; }

const gameevent::Definition* gameevent::MissionActivation::definition(unsigned number) const {
    for (const auto& value : definitions) if (value.eventNumber == number) return &value;
    return nullptr;
}

gameevent::ConditionState& gameevent::MissionActivation::condition(unsigned number) {
    for (auto& value : records) if (value.eventNumber == number) return value;
    throw std::out_of_range("Mission event is absent.");
}

void gameevent::MissionActivation::complete(unsigned number, std::uint8_t& completedCount) {
    auto& value = condition(number);
    value.flags = (value.flags & ~0x33u) | 8;
    active.clear(value.eventNumber);
    if (!(value.flags & 4)) {
        ++completedCount;
        if (value.loadLimit == completedCount) value.flags |= 0x40;
    }
    if (flags.value(static_cast<std::uint8_t>(value.disappearFlag))) value.flags |= 0x40;
}
