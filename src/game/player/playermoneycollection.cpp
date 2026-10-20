#include "game/player/playermoneycollection.h"
#include "game/player/playeractorcomponents.h"

#include <stdexcept>

namespace {
    constexpr unsigned coinSound = 0x810;
    constexpr unsigned billSound = 0x811;
    constexpr unsigned suctionEvent = 0x13;
    constexpr unsigned coinContactEvent = 0x26;
    constexpr unsigned billContactEvent = 0x27;
}

std::int32_t playerevents::collectSuctionMoney(playeractor::Player& player, unsigned kind, unsigned amount,
    const CollectionSound& sound) {
    if (kind != 5 && kind != 6) throw std::invalid_argument("Money suction requires a coin or bill payload.");
    if (!sound) throw std::runtime_error("Money collection requires positional playback.");
    const bool coin = kind == 5;
    player.state.fields12F4[coin ? 0 : 1] += amount;
    const auto delay = coin ? player.components->weapon->pipe->pulse(-1, 1.7f) : 10;
    sound(coin ? coinSound : billSound, player.position);
    player.pulseFeedback(coin ? 0.3f : 0.2f, 10, 0);
    return delay;
}

unsigned playerevents::collectMoney(playeractor::Player& player, unsigned message, const Payload& payload,
    const CollectionSound& sound) {
    if (message == suctionEvent) {
        const auto delay = collectSuctionMoney(player, payload.word(0x60), payload.word(0x64), sound);
        if (payload.word(0x70) == 1 && player.state.field1124 <= 0) {
            player.state.field1124 = static_cast<float>(delay);
            player.state.field1128 = payload.word(0x74);
        }
        return 1;
    }
    if (message != coinContactEvent && message != billContactEvent)
        throw std::invalid_argument("Unrecognized money collection event.");
    const bool coin = message == coinContactEvent;
    const auto amount = payload.word(coin ? 0x94 : 0x98);
    if (static_cast<std::int32_t>(amount) <= 0) return 0;
    if (!sound) throw std::runtime_error("Money collection requires positional playback.");
    player.state.fields12F4[coin ? 0 : 1] += amount;
    sound(coin ? coinSound : billSound, player.position);
    return 1;
}
