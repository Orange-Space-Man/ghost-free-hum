#include "game/menu/controllerinstructionowner.h"

#include <stdexcept>

bool controllerinstructions::Owner::open(const std::filesystem::path& package, std::string& error) {
    if (opened) throw std::logic_error("Controller instructions are already open.");
    if (!contents.load(package, error)) return false;
    opened = true;
    closing = false;
    return true;
}

void controllerinstructions::Owner::update(std::uint32_t pressed, std::uint64_t frame, Services& services) {
    if (!opened) return;
    if (closing) {
        opened = closing = false;
        contents.reset();
        return;
    }
    const auto result = contents.update(pressed, services.option(), frame, true);
    if (result.changed) {
        services.sound(0x3801);
        services.playerScheme(result.scheme);
        services.setOption(result.scheme - 1);
    }
    if (result.close) {
        services.sound(0x3802);
        services.saveOptions();
        closing = true;
    }
}

bool controllerinstructions::Owner::active() const { return opened; }
const controllerinstructions::Screen& controllerinstructions::Owner::screen() const { return contents; }
