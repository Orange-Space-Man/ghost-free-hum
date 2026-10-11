#pragma once

#include "game/actors/characteritemselection.h"
#include "game/enemies/enemyitemstrategy.h"

namespace characterowner { class Owner; }

namespace characteritems {
    class Creation {
    public:
        Creation(const mission::Resources& mission, const enemytypes::Manager& parameters, const Admission& admission,
            enemies::Manager& manager, enemies::Runtime& runtime, const actors::Registry& actors,
            const roomlocation::Rooms& rooms, enemystrategy::ItemSounds& sounds);
        unsigned create(unsigned type, const itemsuction::Vector& position, const itemsuction::Vector* velocity = nullptr);
        unsigned create(const jmp::ToolDataRef& source, const itemsuction::Vector& position,
            const itemsuction::Vector* velocity = nullptr);
        unsigned drop(characterowner::Owner& owner);

    private:
        const mission::Resources& mission;
        const enemytypes::Manager& parameters;
        const Admission& admission;
        enemies::Manager& manager;
        enemies::Runtime& runtime;
        const actors::Registry& actors;
        const roomlocation::Rooms& rooms;
        enemystrategy::ItemSounds& sounds;
        const playeractor::Player& player() const;
        itemsuction::Vector center(const itemsuction::Vector& position) const;
    };
}
