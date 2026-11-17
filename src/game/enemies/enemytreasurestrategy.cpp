#include "game/enemies/enemytreasurestrategy.h"
#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"

#include <stdexcept>

enemystrategy::Treasure::Treasure(const mission::Resources& mission, enemies::Runtime& runtime,
    const std::vector<std::uint32_t>& roomFlags,
    std::function<void(unsigned, const std::array<float, 3>&)> sound) :
    mission(mission), runtime(runtime), roomFlags(roomFlags), sound(std::move(sound)) {}

void enemystrategy::Treasure::initializeBehavior() {
    if (!owner) throw std::runtime_error("Btakara1 requires its original character owner.");
    auto& actor = dynamic_cast<actors::MoveActor&>(owner->actor());
    if (state == 0) {
        actor.flags |= 0xc400;
        actor.selectAnimation("NORMAL");
        room = runtime.resolveRoom(actor.position);
        const auto* table = mission.getJmpResource("treasuretable");
        if (!table) throw std::runtime_error("Btakara1 requires the original treasuretable.");
        int selected = -1;
        for (unsigned row = 0; row < table->count(); ++row) {
            std::uint32_t number = 0;
            if (table->getValue(row, "room", number) && number == (room & 255)) { selected = static_cast<int>(row); break; }
        }
        if (selected < 0) throw std::runtime_error("Btakara1 room has no original treasuretable row.");
        for (const char* name : {"coin", "bill", "gold", "SPearl", "mPearl", "LPearl", "sapphire", "emerald",
            "ruby", "diamond", "cdiamond", "rdiamond", "effect", "other"}) {
            std::uint32_t value = 0;
            if (table->getValue(selected, name, value) && value)
                throw std::runtime_error("Btakara1 treasure delivery requires its original item path.");
        }
        std::uint32_t mode = 0;
        table->getValue(selected, "camera", mode);
        camera = mode != 0;
        setNextState((roomFlags.at(room >> 24) & 0x20) ? 6 : 2);
    } else if (state == 2) {
        actor.selectAnimation("NORMAL");
        actor.flags &= ~0xc000u;
        if (!sound) throw std::runtime_error("Btakara1 requires its positional sound owner.");
        sound(0x18bb, actor.position);
    } else if (state != 4) {
        throw std::runtime_error("Btakara1 state is outside the ported original first-area path.");
    }
}

void enemystrategy::Treasure::behavior() {
    if (state == 2 && frames > 30) {
        dynamic_cast<actors::MoveActor&>(owner->actor()).flags &= ~0x400u;
        setNextState(4);
    }
}
