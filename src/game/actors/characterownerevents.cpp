#include "game/actors/characterownerevents.h"
#include "game/actors/moveactor.h"
#include "game/enemies/itemcreationroute.h"

#include <stdexcept>

std::uint32_t characterowner::dispatchEvent(Owner& owner, characterevent::Event& event, int group, CaptureServices* capture,
    characteritems::Creation* items) {
    auto& state = owner.state();
    auto& owned = owner.ownership();
    auto* observer = dynamic_cast<characterevent::Observer*>(owned.strategy.get());
    auto notify = [&](std::uint32_t fallback) { return observer ? observer->receive(event) : fallback; };
    auto alpha = [&]() { return dynamic_cast<actors::MoveActor&>(owner.actor()).fieldE0 & 255; };
    switch (event.message) {
    case 0: return 1;
    case 1:
        if (!alpha()) return 0;
        state.fields908[1] = 1;
        return 1;
    case 2:
        state.fields908[0] = 1;
        return notify(1);
    case 3: {
        if (!capture) throw std::runtime_error("Original capture motion and item lifecycle are not connected.");
        auto& actor = dynamic_cast<actors::MoveActor&>(owner.actor());
        const auto captureType = static_cast<std::uint16_t>(event.fields[0]);
        if (actor.selectAnimation("OU")) {
            state.field8cc = 0;
            state.field8d0 = 0;
        }
        capture->initializeMotion(owner, captureType);
        state.flags904[0] = true;
        if (owned.sharedUsers[0]) owned.releaseShared(owned.sharedUsers[0]);
        if (items) items->drop(owner);
        else capture->dropItem(owner);
        if (state.actorUsers[0]) owner.releaseActorReference(state.actorUsers[0]);
        notify(0);
        return 1;
    }
    case 4:
        if (static_cast<std::int32_t>(event.fields[4]) > 1 && static_cast<std::int32_t>(event.fields[4]) < 5) {
            if (!capture) throw std::runtime_error("Original capture completion player event is not connected.");
            capture->notifyPlayer(owner);
        }
        state.field91c = true;
        state.field920 = event.fields[3];
        notify(0);
        return 1;
    case 6:
        owner.setLife(event.fields[5]);
        return 1;
    case 7:
        owner.initializeLife();
        return 1;
    case 8:
        if (!owner.beginReplacement()) return 0;
        notify(0);
        return 1;
    case 9: return notify(1);
    case 10:
        if (state.actorUsers[0]) {
            owner.releaseActorReference(state.actorUsers[0]);
            state.flags904[2] = false;
        }
        return notify(1);
    case 11: return notify(1);
    case 12: return state.field91c ? 0 : notify(0);
    case 13:
    case 16:
        state.fields908[2] = 1;
        return notify(0);
    case 14:
    case 17:
        state.fields908[4] = 1;
        return notify(0);
    case 15:
    case 18:
        state.fields908[3] = 1;
        return notify(0);
    case 19: return notify(0);
    case 20: return state.flags904[2];
    case 21:
    case 22:
    case 23:
    case 24:
    case 25: return static_cast<std::int32_t>(state.fields908[event.message - 21]) > 0;
    case 26:
        if (state.field91c || !owned.strategy) return 0;
        if (group == 2 || group == 3) {
            if (state.actorUsers[1]) return 1;
            if (!state.actorUsers[0]) return 0;
        }
        return owner.kind() == Kind::Ordinary ? owned.strategy->query(owner) : 0;
    case 27: return alpha() != 0;
    case 28: return 1;
    case 29:
        if (!event.types) throw std::runtime_error("Original resource event has no type mask.");
        event.types->at(owner.actor().type()) = 1;
        return notify(1);
    default: return 0;
    }
}
