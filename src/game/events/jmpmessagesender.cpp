#include "game/events/jmpmessagesender.h"
#include <stdexcept>

bool jmp::MessageReceiver::create(ToolDataRef&) { return false; }
bool jmp::MessageReceiver::remove(ToolDataRef&) { return false; }
bool jmp::MessageReceiver::message(ToolDataRef&, const char*) { return false; }

bool jmp::MessageSender::addReceiver(MessageReceiver* receiver) {
    if (count >= receivers.size()) return false;
    receivers[count++] = receiver;
    return true;
}

bool jmp::MessageSender::dispatch(EntryHandler handler, ToolDataRef& entry) {
    const auto length = count;
    for (unsigned i = 0; i < length; ++i) {
        if (!receivers[i] || !handler) throw std::runtime_error("Invalid JMP message receiver.");
        if ((receivers[i]->*handler)(entry)) return true;
    }
    return false;
}

bool jmp::MessageSender::dispatch(CommandHandler handler, ToolDataRef& entry, const char* command) {
    const auto length = count;
    for (unsigned i = 0; i < length; ++i) {
        if (!receivers[i] || !handler) throw std::runtime_error("Invalid JMP message receiver.");
        if ((receivers[i]->*handler)(entry, command)) return true;
    }
    return false;
}
