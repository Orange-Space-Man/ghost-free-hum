#pragma once

#include "game/events/jmptooldata.h"
#include <array>

namespace jmp {
    class TableList {
    public:
        bool add(const ToolData* table);
        int find(const ToolData* table) const;
        unsigned erase(unsigned index);
        unsigned size() const;
        const ToolData* at(unsigned index) const;

    private:
        std::array<const ToolData*, 10> tables{};
        unsigned count = 0;
    };

    struct ActiveEntry {
        std::uint32_t state = 0;
        ToolDataRef source;
        std::uint32_t roomId = 0;
    };

    class ActiveEntries {
    public:
        bool append(const ActiveEntry& entry);
        int find(const ToolDataRef& entry) const;
        unsigned erase(unsigned index);
        unsigned size() const;
        ActiveEntry& at(unsigned index);
        const ActiveEntry& at(unsigned index) const;

    private:
        std::array<ActiveEntry, 200> entries{};
        unsigned count = 0;
    };
}
