#include "game/menu/controllerinstructionservices.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

controllerinstructions::GameplayServices::GameplayServices(gamesave::State& saves,
    playerevents::Dispatcher& players, std::function<void(unsigned)> playSound) :
    saves(saves), players(players), playSound(std::move(playSound)) {
    if (!saves.initialized || !this->playSound)
        throw std::invalid_argument("Controller instructions require initialized options and sound delivery.");
    std::copy_n(saves.working.begin() + 0xb1c, options.size(), options.begin());
}

unsigned controllerinstructions::GameplayServices::option() const { return options[2]; }
void controllerinstructions::GameplayServices::setOption(unsigned value) {
    options[2] = static_cast<std::uint8_t>(value);
}

void controllerinstructions::GameplayServices::playerScheme(unsigned scheme) {
    playerevents::Payload payload;
    payload.word(0x8c, scheme);
    players.send(0, 0x24, &payload);
}

void controllerinstructions::GameplayServices::sound(unsigned id) { playSound(id); }

void controllerinstructions::GameplayServices::saveOptions() {
    std::copy(options.begin(), options.end(), saves.working.begin() + 0xb1c);
}
