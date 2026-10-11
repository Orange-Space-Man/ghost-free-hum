#include "game/enemies/enemyitemlifetime.h"
#include "game/enemies/enemymanager.h"

enemystrategy::ItemLifetime::ItemLifetime() {
    parameters.add("mIgnoreTime", 4, 90);
    parameters.add("mLife", 4, 480);
    parameters.add("mBlinkFeq", 4, 3);
    parameters.add("mBlinkLife", 4, 50);
}

void enemystrategy::ItemLifetime::initialize() {
    remaining = parameters.entries()[1].bits;
    blink = 0;
}

void enemystrategy::ItemLifetime::update(enemies::Owner& owner, bool infinite, bool collected,
    std::uint32_t& flags, bool& expired) {
    if (infinite || collected) return;
    --remaining;
    if (static_cast<std::int32_t>(remaining) < static_cast<std::int32_t>(parameters.entries()[3].bits)) {
        ++blink;
        const bool hide = blink == parameters.entries()[2].bits;
        if (hide) blink = 0;
        owner.actor().visibility(hide ? 0 : 255, hide ? 0 : 255);
    }
    if (static_cast<std::int32_t>(remaining) <= 0) {
        flags &= ~1u;
        expired = true;
        owner.event(4);
    }
}
