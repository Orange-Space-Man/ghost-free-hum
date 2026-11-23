#pragma once

#include "moneyscatterowner.h"
#include "game/events/jmpmessagesender.h"

namespace moneyscatter {
    class Placements final : public jmp::MessageReceiver {
    public:
        Placements(Owner& owner, const roomlocation::Rooms& rooms,
            const std::array<std::int16_t, 256>& roomIndices, const std::vector<std::uint32_t>& roomFlags,
            std::function<Vector(int)> retainedDirection);
        bool create(jmp::ToolDataRef& entry) override;
        bool remove(jmp::ToolDataRef& entry) override;
        bool message(jmp::ToolDataRef& entry, const char* command) override;

    private:
        Owner& owner;
        const roomlocation::Rooms& rooms;
        const std::array<std::int16_t, 256>& roomIndices;
        const std::vector<std::uint32_t>& roomFlags;
        std::function<Vector(int)> retainedDirection;
    };
}
