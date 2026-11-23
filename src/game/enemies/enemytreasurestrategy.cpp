#include "game/enemies/enemytreasurestrategy.h"
#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"

#include <stdexcept>
#include <string_view>

enemystrategy::Treasure::Treasure(const mission::Resources& mission, enemies::Runtime& runtime,
    const std::vector<std::uint32_t>& roomFlags,
    std::function<void(unsigned, const std::array<float, 3>&)> sound,
    std::function<unsigned(std::string_view, const std::array<float, 3>&)> createItem,
    std::function<void(const std::array<float, 3>&)> revealCamera,
    std::function<bool()> automaticallyOpenKeyChests,
    std::function<void(unsigned)> markOpened) :
    mission(mission), runtime(runtime), roomFlags(roomFlags), sound(std::move(sound)), createItem(std::move(createItem)),
    revealCamera(std::move(revealCamera)), automaticallyOpenKeyChests(std::move(automaticallyOpenKeyChests)),
    markOpened(std::move(markOpened)) {}

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
            "ruby", "diamond", "cdiamond", "rdiamond", "effect"}) {
            std::uint32_t value = 0;
            if (table->getValue(selected, name, value) && value)
                throw std::runtime_error("Btakara1 money and jewel delivery is not yet ported.");
        }
        std::string_view item;
        if (table->getValue(selected, "other", item) && item != "(null)") other = item;
        std::uint32_t mode = 0;
        table->getValue(selected, "camera", mode);
        camera = mode != 0;
        setNextState((roomFlags.at(room >> 24) & 0x20) ? 6 : 2);
    } else if (state == 2) {
        actor.selectAnimation("NORMAL");
        // 80153DDC fades the chest in by setting its target alpha to FF and
        // advancing eight alpha units per update (FF / 1E).
        actor.fieldE4 = (actor.fieldE4 & 0xffffff00u) | 0xffu;
        actor.fieldE8 = 0xff / 0x1e;
        actor.flags &= ~0xc000u;
        if (!sound) throw std::runtime_error("Btakara1 requires its positional sound owner.");
        sound(0x18bb, actor.position);
        if (camera && runtime.playerRoom(0) == room) {
            if (!revealCamera) throw std::runtime_error("Btakara1 requires the original reveal camera owner.");
            revealCamera(actor.position);
        }
    } else if (state == 3) {
        // 80153F48: OPEN begins only after the player's tap event enters state 3.
        if (actor.selectAnimation("OPEN")) {
            auto& character = dynamic_cast<characterowner::Owner&>(*owner);
            character.state().field8cc = 0;
            character.state().field8d0 = 0;
        }
    } else if (state == 4) {
        // 80153EC4: the appeared chest remains closed until Luigi interacts.
    } else if (state == 5) {
        // 80153FC4 / 801548B4: room 35's imported row has no money or jewels;
        // its 'other' entry is key_38, launched 50 units above the chest.
        if (!other.empty() && !delivered) {
            if (!createItem) throw std::runtime_error("Btakara1 requires its original item creator.");
            auto position = actor.position;
            position[1] += 50.0f;
            if (createItem(other, position) == 0xffffffffu)
                throw std::runtime_error("Btakara1 could not create its listed item.");
            if (!markOpened) throw std::runtime_error("Btakara1 requires its original room chest-open flag.");
            markOpened(room);
            delivered = true;
        }
    } else if (state == 6) {
        actor.selectAnimation("WAIT");
    } else {
        throw std::runtime_error("Btakara1 state is outside the ported original first-area path.");
    }
}

void enemystrategy::Treasure::behavior() {
    if (state == 2 && frames > 30) {
        dynamic_cast<actors::MoveActor&>(owner->actor()).flags &= ~0x400u;
        setNextState(4);
    } else if (state == 3) {
        auto& character = dynamic_cast<characterowner::Owner&>(*owner);
        if (character.state().field8d0 & 1) setNextState(5);
    } else if (state == 4 && std::string_view(other).substr(0, 4) == "key_" &&
        (roomFlags.at(room >> 24) & 0x8000) && automaticallyOpenKeyChests && automaticallyOpenKeyChests()) {
        setNextState(3);
    } else if (state == 5 && frames > 30) {
        setNextState(6);
    }
}

std::uint32_t enemystrategy::Treasure::playerTapped(characterevent::Event&) {
    if (state != 4 || nextState != 0xffff) return 0;
    setNextState(3);
    return 1;
}

