#include "game/input/controllerfeedbackscript.h"

#include <stdexcept>

int controllerfeedback::combineCommands(int first, int second) {
    if (first == '.') return '.';
    if (first > '-') return first > '/' ? first : second;
    if (first < '-') return first;
    return second == '/' ? first : second;
}

bool controllerfeedback::Scripts::play(const Script& script) {
    const auto length = script.commands.find('\0');
    const auto size = length == std::string::npos ? script.commands.size() : length;
    unsigned starts = 0;
    unsigned ends = 0;
    std::uint32_t start = 0;
    std::uint32_t end = 0;
    for (std::size_t i = 0; i < size; ++i) {
        if (script.commands[i] == '0') {
            ++starts;
            start = static_cast<std::uint32_t>(i);
        }
        if (script.commands[i] == '1') {
            ++ends;
            end = static_cast<std::uint32_t>(i);
        }
    }
    if (starts > 1 || ends > 1 || starts != ends) return false;
    const bool looping = starts != 0;
    if (looping) {
        for (auto& slot : slots) {
            if (slot.active && slot.script.identity == script.identity && !slot.retrigger) {
                slot.retrigger = true;
                return false;
            }
        }
    }
    ScriptSlot* selected = nullptr;
    std::uint32_t remaining = 0xffff;
    for (auto& slot : slots) {
        if (!slot.active) {
            selected = &slot;
            break;
        }
        if (slot.looping && slot.length - slot.cursor < remaining) {
            remaining = slot.length - slot.cursor;
            selected = &slot;
        }
    }
    if (!selected) return false;
    selected->active = true;
    selected->looping = looping;
    selected->retrigger = looping;
    selected->script = script;
    selected->cursor = 0;
    selected->length = static_cast<std::uint32_t>(size);
    selected->loopStart = start;
    selected->loopEnd = end;
    selected->repeats = 0;
    return true;
}

int controllerfeedback::Scripts::update() {
    int command = '/';
    for (auto& slot : slots) {
        if (!slot.active) continue;
        if (slot.looping && !slot.retrigger) {
            if (slot.loopEnd < slot.length) {
                slot.looping = false;
                slot.retrigger = false;
                slot.cursor = slot.loopEnd + 1;
            }
            else slot.active = false;
        }
        bool control;
        std::size_t steps = 0;
        do {
            if (++steps > slot.script.commands.size() + 2) {
                throw std::runtime_error("Controller feedback script has a non-advancing control loop.");
            }
            control = false;
            if (slot.cursor > slot.script.commands.size()) {
                throw std::runtime_error("Controller feedback script cursor exceeds its storage.");
            }
            const char current = slot.script.commands[slot.cursor];
            if (current == '*' || current == '-' || current == '/') command = combineCommands(command, current);
            else if (!current) {
                slot.active = false;
                command = combineCommands(command, '/');
            }
            else if (current == '0') control = true;
            else if (current == '1') {
                control = true;
                slot.cursor = slot.loopStart;
                ++slot.repeats;
            }
            ++slot.cursor;
        } while (control);
        if (slot.looping) slot.retrigger = false;
    }
    return command;
}

void controllerfeedback::Scripts::reset() {
    for (auto& slot : slots) {
        slot.active = false;
        slot.script.identity = 0;
        slot.cursor = 0;
    }
}
