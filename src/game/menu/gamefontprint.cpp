#include "game/menu/gamefontprint.h"
#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace {
    std::array<float, 4> color(std::uint32_t value, bool shadow) {
        return {shadow ? 0 : ((value >> 24) & 255) / 255.0f,
            shadow ? 0 : ((value >> 16) & 255) / 255.0f,
            shadow ? 0 : ((value >> 8) & 255) / 255.0f, (value & 255) / 255.0f};
    }
}

gamefont::PrintLayout gamefont::print(const Font& font, std::string_view text, float origin, float baseline,
    unsigned width, unsigned height, int lineSpacing, bool shadow) {
    PrintLayout result;
    float x = origin, y = baseline, minX = x, maxX = x, minY = y, maxY = y, previousAdvance = 0;
    bool continuation = false, gradient = true;
    std::uint32_t top = 0xffffffff, bottom = 0xffffffff;
    int spacing = 0;
    for (std::size_t i = 0; i < text.size();) {
        unsigned character = static_cast<unsigned char>(text[i++]);
        if (!character) break;
        if (character == 27) {
            if (i + 2 > text.size()) throw std::runtime_error("Truncated original font escape.");
            const auto command = text.substr(i, 2);
            i += 2;
            if (i == text.size() || text[i] != '[') throw std::runtime_error("Font escape requires its recovered argument.");
            const auto end = text.find(']', ++i);
            if (end == std::string_view::npos) throw std::runtime_error("Unterminated original font escape.");
            const bool rgba = command == "CC" || command == "GC";
            std::uint32_t value = 0;
            const auto parsed = std::from_chars(text.data() + i, text.data() + end, value, rgba ? 16 : 10);
            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + end)
                throw std::runtime_error("Invalid original font escape argument.");
            if (rgba) {
                if (end - i == 6) value = (value << 8) | 255;
                else if (end - i != 8) throw std::runtime_error("Invalid original font color length.");
            }
            i = end + 1;
            if (command == "CC") top = value;
            else if (command == "GC") bottom = value;
            else if (command == "FX") width = value;
            else if (command == "FY") height = value;
            else if (command == "SV") lineSpacing = static_cast<int>(value);
            else if (command == "SH") spacing = static_cast<int>(value);
            else if (command == "GM") gradient = value != 0;
            else throw std::runtime_error("Unrecovered font escape: " + std::string(command));
            continue;
        }
        if (character < 32) {
            switch (character) {
            case 8: x -= previousAdvance; previousAdvance = 0; continuation = true; break;
            case 9: {
                const int tab = static_cast<int>(font.width * 4);
                if (tab > 0) { const float next = static_cast<float>(tab + tab * (static_cast<int>(x) / tab)); previousAdvance = next - x; x = next; }
                continuation = true;
                break;
            }
            case 10: x = origin; y += lineSpacing; previousAdvance = 0; continuation = false; break;
            case 13: x = origin; previousAdvance = 0; continuation = false; break;
            case 28: x += 1; break;
            case 29: x -= 1; break;
            case 30: y -= 1; break;
            case 31: y += 1; break;
            }
            continue;
        }
        if (character > 127) {
            if (i == text.size()) throw std::runtime_error("Truncated original font character.");
            character = (character << 8) | static_cast<unsigned char>(text[i++]);
        }
        startupscene::Picture picture;
        previousAdvance = font.draw(character, x, y, static_cast<float>(width), static_cast<float>(height), continuation, picture) + spacing;
        picture.colors = {color(top, shadow), color(top, shadow), color(gradient ? bottom : top, shadow), color(gradient ? bottom : top, shadow)};
        result.pictures.push_back(std::move(picture));
        x += previousAdvance;
        continuation = true;
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }
    result.size = {maxX - minX, maxY - minY + font.leading};
    result.cursor = {x, y};
    return result;
}
