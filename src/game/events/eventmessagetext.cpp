#include "game/events/eventmessagetext.h"
#include "resources/dolresources.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <utility>

gameevent::MessageText::MessageText(const std::vector<std::uint8_t>& executable, std::vector<std::uint8_t> source) :
    source(std::move(source)) {
    constexpr unsigned messageColorTable = 0x80339c48;
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, messageColorTable, 32, bytes, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < colors.size(); ++i) colors[i] = (unsigned(bytes[i * 4]) << 24) |
        (unsigned(bytes[i * 4 + 1]) << 16) | (unsigned(bytes[i * 4 + 2]) << 8) | bytes[i * 4 + 3];
}

void gameevent::MessageText::color(std::string& text, std::uint32_t rgba) {
    char commands[27];
    std::snprintf(commands, sizeof(commands), "\x1b" "CC[%08x]\x1b" "GC[%08x]", rgba, rgba);
    text += commands;
}

std::string gameevent::MessageText::line(unsigned index) {
    std::size_t cursor = 0;
    std::string result;
    for (unsigned row = 0; row <= index; ++row) {
        result.clear();
        color(result, 0xffffffff);
        for (;;) {
            if (cursor >= source.size()) throw std::runtime_error("Truncated original event message line.");
            const auto value = source[cursor];
            if (value == '\r') break;
            if ((value >= 0x80 && value <= 0xbf) || (value >= 0xe0 && value <= 0xfb)) {
                if (cursor + 1 >= source.size()) throw std::runtime_error("Truncated event message character.");
                result.push_back(static_cast<char>(source[cursor++]));
                result.push_back(static_cast<char>(source[cursor++]));
            } else if (value == '<') {
                while (cursor < source.size() && source[cursor++] != '>') {}
                if (cursor >= source.size() || source[cursor++] != '(')
                    throw std::runtime_error("Invalid event message color argument.");
                std::string number;
                while (cursor < source.size() && source[cursor] != ')') number.push_back(static_cast<char>(source[cursor++]));
                if (cursor == source.size()) throw std::runtime_error("Truncated event message color argument.");
                ++cursor;
                int selected = std::atoi(number.c_str());
                if (selected > 7) selected = 0;
                if (selected < 0) throw std::runtime_error("Event message color reads before the original table.");
                color(result, colors[selected]);
            } else if (value == 12) {
                source[cursor] += 2;
                result.push_back('\n');
            } else result.push_back(static_cast<char>(source[cursor++]));
        }
        if (cursor + 1 >= source.size() || source[cursor + 1] != '\n')
            throw std::runtime_error("Original event messages require CRLF line endings.");
        cursor += 2;
    }
    return result;
}
