#include "game/events/entrancescript.h"

#include <charconv>
#include <stdexcept>
#include <utility>

gameevent::EntranceScript::EntranceScript(std::string source) : source(std::move(source)) {}

std::string_view gameevent::EntranceScript::token(char begin, char end) {
    const auto first = source.find(begin, cursor);
    const auto last = first == std::string::npos ? first : source.find(end, first + 1);
    if (last == std::string::npos) throw std::runtime_error("Truncated entrance event command.");
    cursor = last + 1;
    return std::string_view(source).substr(first + 1, last - first - 1);
}

unsigned gameevent::EntranceScript::number() {
    const auto value = token('(', ')');
    unsigned result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        throw std::runtime_error("Invalid entrance event number.");
    return result;
}

bool gameevent::EntranceScript::update(EntranceServices& services) {
    if (state == 0) {
        if (!completed) services.finish();
        completed = true;
        return true;
    }
    if (state == 9) {
        if (!services.messageClosed()) return false;
        state = 0;
        return false;
    }
    if (state == 4 && --wait != 0) return false;
    state = 1;
    bool grouped = false;
    do {
        const auto command = token('<', '>');
        if (command == "CHECKFLAG") {
            const auto index = number();
            const auto on = token('"', '"');
            const auto off = token('"', '"');
            const auto selected = services.flag(index) ? on : off;
            for (;;) {
                if (token('<', '>') == "CASE" && token('"', '"') == selected) break;
            }
        } else if (command == "MENUOFF") services.menu(false);
        else if (command == "MENUON") services.menu(true);
        else if (command == "LUIGISTOP3") services.stopPlayer();
        else if (command == "LUIGIFREE2") services.releasePlayer();
        else if (command == "BGM") services.music(number() & 255);
        else if (command == "FRAMESTART") grouped = true;
        else if (command == "FRAMEEND") grouped = false;
        else if (command == "GCAMERA") services.camera(token('"', '"'));
        else if (command == "GPATHLIGHT") services.lighting(token('"', '"'));
        else if (command == "ACTOR") {
            const auto actor = token('"', '"');
            if (token('<', '>') != "SHADOWOFF") throw std::runtime_error("Unsupported entrance actor command.");
            services.shadowOff(actor);
        } else if (command == "CAMENDFAST") services.endCamera();
        else if (command == "WAIT") {
            wait = number();
            state = 4;
        } else if (command == "END") {
            services.closeMessage();
            state = 9;
        } else throw std::runtime_error("Unsupported entrance command: " + std::string(command));
    } while (grouped);
    return false;
}

bool gameevent::EntranceScript::finished() const { return completed; }
