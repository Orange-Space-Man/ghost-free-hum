#include "game/events/eventinfodecoder.h"

#include <stdexcept>
#include <utility>

bool gameevent::readDefinitions(const jmp::ToolData& table, const enemytypes::Catalog& types,
    std::vector<Definition>& definitions, std::string& error) {
    error.clear();
    try {
        std::vector<Definition> result;
        Definition definition;
        for (unsigned index = 0; index < table.count(); ++index) {
            const unsigned row = index & 0xffff;
            if (!table.getValue(row, "pos_x", definition.position[0]) ||
                !table.getValue(row, "pos_y", definition.position[1]) ||
                !table.getValue(row, "pos_z", definition.position[2]) ||
                !table.getValue(row, "EventNo", definition.eventNumber))
                throw std::runtime_error("Missing required EventInfo position or number.");
            table.getValue(row, "EventIf", definition.trigger);
            table.getValue(row, "EventArea", definition.area);
            std::uint32_t startMinutes = 0, startSeconds = 0, endMinutes = 0, endSeconds = 0;
            table.getValue(row, "EventTime", startMinutes);
            table.getValue(row, "EventTime2", startSeconds);
            table.getValue(row, "EventTime3", endMinutes);
            table.getValue(row, "EventTime4", endSeconds);
            auto& conditions = definition.conditions;
            table.getValue(row, "EventLoad", conditions.loadLimit);
            table.getValue(row, "EventFlag", conditions.appearFlag);
            table.getValue(row, "EventLock", conditions.lock);
            table.getValue(row, "PlayerStop", conditions.playerStop);
            std::string_view characterName;
            if (!table.getValue(row, "CharacterName", characterName))
                throw std::runtime_error("Missing required EventInfo character name.");
            table.getValue(row, "disappear_flag", conditions.disappearFlag);
            if (!table.getValue(row, "event_parameter", conditions.parameter))
                throw std::runtime_error("Missing required EventInfo parameter.");
            conditions.characterType = types.find(characterName);
            conditions.startTime = startSeconds + startMinutes * 60;
            conditions.endTime = endSeconds + endMinutes * 60;
            result.push_back(definition);
        }
        definitions = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
