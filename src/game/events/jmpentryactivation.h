#pragma once

#include "game/events/jmpeventstate.h"
#include "game/events/jmpmessagesender.h"
#include "game/events/runtimeflags.h"
#include "game/events/eventconditions.h"

namespace jmp {
    class ActivationRuntime {
    public:
        virtual ~ActivationRuntime() = default;
        virtual unsigned resolveRoom(const std::array<float, 3>& position) = 0;
        virtual bool roomVisible(unsigned room) = 0;
    };

    class EntryActivation {
    public:
        EntryActivation(MessageSender& sender, ActivationRuntime& runtime, const gameflags::State& flags,
            const gameevent::ActivationState& events);
        bool add(const ToolData* table);
        bool remove(const ToolData* table);
        void update();
        void scan(const ToolData* table, bool create, bool force = false);
        const ActiveEntries& entries() const;

    private:
        MessageSender& sender;
        ActivationRuntime& runtime;
        const gameflags::State& flags;
        const gameevent::ActivationState& events;
        TableList tables;
        ActiveEntries active;
    };
}
