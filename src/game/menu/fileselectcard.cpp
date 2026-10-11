#include "game/menu/fileselectcard.h"

#include <algorithm>

bool fileselect::writeCardAction(const Request& request, gamesave::State& saves, const gamesave::State& defaults,
    gamecard::Storage& card, const gamecard::Block& banner, unsigned tick, unsigned modified, std::string& error) {
    error.clear();
    if (!saves.initialized || !defaults.initialized || request.source >= 3 ||
        (request.action != Action::copy && request.action != Action::erase)) {
        error = "Unsupported memory-card operation.";
        return false;
    }
    auto next = saves;
    const auto source = 12 + request.source * 0x3b0;
    if (request.action == Action::erase) {
        std::copy_n(defaults.working.begin() + source, 0x3b0, next.working.begin() + source);
    } else {
        if (request.destination >= 3 || request.destination == request.source || !gamesave::slotUsed(saves, request.source) ||
            gamesave::slotUsed(saves, request.destination)) {
            error = "Invalid memory-card copy destination.";
            return false;
        }
        next.working = saves.committed;
        const auto destination = 12 + request.destination * 0x3b0;
        std::copy_n(saves.working.begin() + source + 2, 10, next.working.begin() + destination + 2);
        next.working[destination + 12] = (next.working[destination + 12] & 0xe0) | (saves.working[source + 12] & 0x1f);
        std::copy_n(saves.working.begin() + source + 0x14, 0x39c, next.working.begin() + destination + 0x14);
        next.working[destination + 1] |= 1;
        if (next.working[destination + 0x3a7]) next.working[destination + 1] |= 2;
        std::copy_n(saves.working.begin() + 0xb1c, 16, next.working.begin() + 0xb1c);
    }
    if (!card.save(next, banner, tick, modified, error)) return false;
    saves = next;
    return true;
}
