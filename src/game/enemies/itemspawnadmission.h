#pragma once

#include "game/events/jmptooldata.h"
#include "game/enemies/enemytypecatalog.h"
#include "game/player/playeractorview.h"

namespace characteritems {
    class Admission {
    public:
        Admission(const std::vector<std::uint8_t>& executable, const enemytypes::Catalog& types);
        bool nothing(const jmp::ToolDataRef& item) const;
        bool money(const jmp::ToolDataRef& item) const;
        unsigned type(const jmp::ToolDataRef& item) const;
        bool key(unsigned type) const;
        bool key(const jmp::ToolDataRef& item) const;
        bool redirected(const jmp::ToolDataRef& item) const;
        bool blocked(const jmp::ToolDataRef& item, const playeractor::View& player) const;

    private:
        const enemytypes::Catalog& types;
        std::array<unsigned, 8> redirectTypes;
        std::array<unsigned, 5> keyTypes;
    };
}
