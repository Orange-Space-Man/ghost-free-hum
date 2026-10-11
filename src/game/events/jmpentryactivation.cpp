#include "game/events/jmpentryactivation.h"

#include <stdexcept>

jmp::EntryActivation::EntryActivation(MessageSender& sender, ActivationRuntime& runtime, const gameflags::State& flags,
    const gameevent::ActivationState& events) : sender(sender), runtime(runtime), flags(flags), events(events) {
}

bool jmp::EntryActivation::add(const ToolData* table) { return tables.add(table); }

bool jmp::EntryActivation::remove(const ToolData* table) {
    const int index = tables.find(table);
    if (index < 0) return false;
    tables.erase(index);
    scan(table, false, true);
    return true;
}

void jmp::EntryActivation::update() {
    for (unsigned i = 0; i < tables.size(); ++i) scan(tables.at(i), false);
    for (unsigned i = 0; i < tables.size(); ++i) scan(tables.at(i), true);
}

void jmp::EntryActivation::scan(const ToolData* table, bool create, bool force) {
    if (!table) return;
    for (unsigned row = 0; row < table->count(); ++row) {
        std::array<float, 3> position{};
        if (!table->getValue(row, "pos_x", position[0]) || !table->getValue(row, "pos_y", position[1]) ||
            !table->getValue(row, "pos_z", position[2])) throw std::runtime_error("Original JMP entry has no position.");
        std::uint8_t stay = 0;
        table->getValue(row, "stay", stay);
        const unsigned room = runtime.resolveRoom(position);
        const bool visible = runtime.roomVisible(room & 255) || stay;
        std::uint32_t appear = 0, disappear = 0, event = 0;
        table->getValue(row, "appear_flag", appear);
        table->getValue(row, "disappear_flag", disappear);
        table->getValue(row, "event_set_no", event);
        const bool eventActive = event && events.value(static_cast<std::int16_t>(event));
        const bool appeared = !appear || flags.value(static_cast<std::uint8_t>(appear));
        const bool disappeared = disappear && flags.value(static_cast<std::uint8_t>(disappear));
        const bool eligible = eventActive || (appeared && !disappeared);
        ToolDataRef source{table, static_cast<int>(row)};
        const int index = active.find(source);
        if (create) {
            if (index < 0 && ((visible && eligible) || force) && active.append({1, source, room})) {
                sender.dispatch(&MessageReceiver::create, source);
            }
        } else if (index >= 0 && (force || !visible || !eligible)) {
            sender.dispatch(&MessageReceiver::remove, active.at(index).source);
            active.erase(index);
        }
    }
}

const jmp::ActiveEntries& jmp::EntryActivation::entries() const { return active; }
