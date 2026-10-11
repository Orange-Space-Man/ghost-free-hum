#pragma once

#include "game/events/jmptooldata.h"
#include <array>

namespace jmp {
    class MessageReceiver {
    public:
        virtual ~MessageReceiver() = default;
        virtual bool create(ToolDataRef& entry);
        virtual bool remove(ToolDataRef& entry);
        virtual bool message(ToolDataRef& entry, const char* command);
    };

    class MessageSender {
    public:
        using EntryHandler = bool (MessageReceiver::*)(ToolDataRef&);
        using CommandHandler = bool (MessageReceiver::*)(ToolDataRef&, const char*);
        bool addReceiver(MessageReceiver* receiver);
        bool dispatch(EntryHandler handler, ToolDataRef& entry);
        bool dispatch(CommandHandler handler, ToolDataRef& entry, const char* command);

    private:
        std::array<MessageReceiver*, 12> receivers{};
        unsigned count = 0;
    };
}
