#pragma once

#include "game/enemies/enemyghostparenthelpers.h"
#include "game/events/jmptooldata.h"

namespace ghostparent {
    enum class MovementChild { Default, Random, SearchFar, SearchNear };
    enum class AppearanceChild { None, Frighten, Appear2, Appear6 };

    struct Configuration {
        std::int32_t place = 0, movement = 1, search = 5, appearance = 2, condition = 7, invisible = 0;
    };

    struct Selection {
        Configuration configuration;
        MovementChild movement = MovementChild::Default, search = MovementChild::Default;
        AppearanceChild appearance = AppearanceChild::None;
        unsigned condition = 0, attack = 0;
    };

    struct InitializeServices {
        actors::MoveActor& actor;
        characterowner::State& character;
        const gameparameters::Parameters& parameters;
        const jmp::ToolDataRef& source;
        std::function<std::uint32_t()> random;
    };

    MovementChild movementChild(int type);
    AppearanceChild appearanceChild(int type);
    unsigned conditionStrategy(int type);
    unsigned attackStrategy(std::int32_t pattern);
    unsigned captureStrategy(std::int32_t tsuriType);
    Configuration resolveConfiguration(const jmp::ToolDataRef& source, Overrides& overrides);
    Selection initialize(Fields& fields, const InitializeServices& services);
}
