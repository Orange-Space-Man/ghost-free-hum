#pragma once

#include "game/enemies/enemystrategy.h"
#include "game/events/missionresources.h"

namespace enemystrategy {
    class PathCondition final : public Strategy {
    public:
        explicit PathCondition(const mission::Resources& resources);
        void initializeBehavior() override;
        void behavior() override;

    private:
        const mission::Resources& resources;
    };
}
