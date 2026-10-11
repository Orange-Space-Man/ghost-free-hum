#pragma once

#include "game/effects/cobwebeffectpool.h"
#include "game/events/jmpmessagesender.h"
#include "game/actors/actorinitialpose.h"

#include <list>

namespace modeleffects {
    class CobwebPlacements final : public jmp::MessageReceiver {
    public:
        CobwebPlacements(Factory& factory, const actormodel::InitialPose& pose);
        void initialize();
        void clear();
        bool create(jmp::ToolDataRef& entry) override;
        bool remove(jmp::ToolDataRef& entry) override;
        bool message(jmp::ToolDataRef& entry, const char* command) override;

    private:
        struct Record {
            CobwebOwner* owner = nullptr;
            int row = -1;
        };
        Factory& factory;
        const actormodel::InitialPose& pose;
        std::array<Record, 10> records;
        std::list<Record*> active, free;
    };
}
