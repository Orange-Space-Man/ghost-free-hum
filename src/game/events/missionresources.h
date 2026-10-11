#pragma once

#include "game/scene/gamearchive.h"
#include "game/events/jmptooldata.h"
#include <memory>

namespace mission {
    class Resources {
    public:
        bool read(const std::vector<std::uint8_t>& archive, std::string& error);
        const jmp::ToolData* getJmpResource(std::string_view name) const;
        const jmp::ToolData* getPathResource(std::string_view name) const;
        const gamearchive::Archive& archive() const;
        std::size_t tableCount() const;

    private:
        struct Table {
            unsigned offset;
            std::unique_ptr<jmp::ToolData> data;
        };
        gamearchive::Archive source;
        std::vector<Table> tables;
        mutable std::vector<Table> paths;
    };
}
