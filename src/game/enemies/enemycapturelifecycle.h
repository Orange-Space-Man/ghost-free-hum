#pragma once

#include "game/enemies/enemycapturemotion.h"
#include "game/enemies/enemystrategy.h"
#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"
#include "game/enemies/enemycreationcatalog.h"
#include <functional>
#include <vector>

namespace ghostcapture {
    void release(characterowner::Owner& owner, enemystrategy::Strategy& strategy);
    bool releaseIfUnlinked(characterowner::Owner& owner, enemystrategy::Strategy& strategy);

    struct DropServices {
        std::function<bool(unsigned, int, const Vector&, const Vector&)> pearl;
        std::function<void(unsigned, int, int, const Vector&, const Vector&)> periodic;
    };

    class Bookkeeping {
    public:
        Bookkeeping(const std::vector<std::uint8_t>& executable, const enemycreation::Catalog& types,
            DropServices services);
        void reset();
        void update(unsigned state, const actors::MoveActor& actor, const characterowner::State& owner,
            int period, int repeatColumn, unsigned itemTable);
        std::uint32_t ticks = 0, pearlIndex = 0;

    private:
        const std::vector<std::uint8_t>& executable;
        const enemycreation::Catalog& types;
        DropServices services;
        std::array<unsigned, 23> portraitTypes{};
        Vector launch{};
    };
}
