#include "game/events/jmpeventstate.h"
#include <stdexcept>

bool jmp::TableList::add(const ToolData* table) {
    if (!table || count >= tables.size()) return false;
    tables[count++] = table;
    return true;
}

int jmp::TableList::find(const ToolData* table) const {
    for (unsigned i = 0; i < count; ++i) if (tables[i] == table) return static_cast<int>(i);
    return -1;
}

unsigned jmp::TableList::erase(unsigned index) {
    if (index >= count) throw std::out_of_range("JMP table index.");
    for (unsigned i = index; i + 1 < count; ++i) tables[i] = tables[i + 1];
    --count;
    return index;
}

unsigned jmp::TableList::size() const { return count; }
const jmp::ToolData* jmp::TableList::at(unsigned index) const {
    if (index >= count) throw std::out_of_range("JMP table index.");
    return tables[index];
}

bool jmp::ActiveEntries::append(const ActiveEntry& entry) {
    if (count >= entries.size()) return false;
    entries[count++] = entry;
    return true;
}

int jmp::ActiveEntries::find(const ToolDataRef& entry) const {
    for (unsigned i = 0; i < count; ++i) if (sameEntry(entries[i].source, entry)) return static_cast<int>(i);
    return -1;
}

unsigned jmp::ActiveEntries::erase(unsigned index) {
    if (index >= count) throw std::out_of_range("JMP active-entry index.");
    for (unsigned i = index; i + 1 < count; ++i) entries[i] = entries[i + 1];
    --count;
    return index;
}

unsigned jmp::ActiveEntries::size() const { return count; }
jmp::ActiveEntry& jmp::ActiveEntries::at(unsigned index) {
    if (index >= count) throw std::out_of_range("JMP active-entry index.");
    return entries[index];
}
const jmp::ActiveEntry& jmp::ActiveEntries::at(unsigned index) const {
    if (index >= count) throw std::out_of_range("JMP active-entry index.");
    return entries[index];
}
