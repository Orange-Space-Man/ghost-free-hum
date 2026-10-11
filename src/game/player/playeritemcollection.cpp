#include "game/player/playeritemcollection.h"
#include "game/player/playercaptureflags.h"

#include <stdexcept>

bool playerevents::acceptItem(playeractor::Player& player, const ItemRequest& request, const ItemEnvironment& environment) {
    auto& state = player.state;
    const auto& actions = *player.components->actions;
    const auto action = [&](unsigned value) { return actions.field310 == 2 && actions.field314 == value; };
    if (static_cast<std::int16_t>(player.life) <= 0 || action(0x20) || action(0x21) || state.flags1040[2] ||
        state.field1058 || state.flags1140[2] || ((state.field1170 != 0 || state.field1174 != 0) && request.animated) ||
        state.field1130 != 0 || state.field1338 != 1 || (request.animated && state.flags10E8[1])) return false;
    auto* item = request.actor < 0 ? nullptr : environment.actors.actor(unsigned(request.actor));
    if (!request.animated) {
        if (item) {
            auto* owner = environment.enemies.activeOwner(*item);
            if (!owner) throw std::runtime_error("Original collected item has no active character owner.");
            owner->event(4);
        }
    } else {
        if (state.field1060 || state.field11A0 || action(0x22)) return false;
        state.field1060 = 1;
        state.field1064 = item;
    }
    if (item) {
        unsigned award = 0;
        int counter = -1;
        switch (item->type()) {
        case 0xc2: counter = 9; break;
        case 0xc3: counter = 10; break;
        case 0xc4: counter = 11; break;
        case 0xc5: counter = 3; award = 3; break;
        case 0xc6: counter = 4; award = 4; break;
        case 0xc7: counter = 5; award = 5; break;
        case 0xc8: counter = 6; award = 6; break;
        case 0xc9:
        case 0xcc: counter = 7; award = 7; break;
        case 0xca: counter = 8; award = 8; break;
        case 0xcb: counter = 2; break;
        }
        if (counter >= 0) {
            ++state.fields12F4[unsigned(counter)];
            environment.sound(0x848, player.position);
        }
        if (award && environment.award) {
            environment.award->kind = award;
            environment.award->frames = 100;
        }
        if (item->type() == 0xcc) item->flags &= ~0xc000u;
        if (environment.admission.key(item->type())) environment.sound(0x80f, player.position);
        const auto type = static_cast<std::int32_t>(item->type());
        if (type < 0xbc) {
            if (type >= 0x75 && type < 0x78) environment.sound(0x84e, player.position);
        } else if (type < 0xc1) environment.sound(0x84d, player.position);
    }
    promoteGrade(*player.components->bits[4], request.gradeIndex, request.grade);
    return true;
}
