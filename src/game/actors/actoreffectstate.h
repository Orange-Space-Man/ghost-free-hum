#pragma once

#include <array>
#include <cstdint>
#include "game/actors/actormodelarchive.h"

namespace actoreffects {
    struct State {
        State();
        std::uint16_t flags;
        actormodel::ResourceRef field4;
        float field8;
        std::uint16_t fieldC, fieldE;
        std::array<float, 3> field10, field1C, field28;
        float field34, field38, field3C, field40;
        std::array<float, 12> fields44;
        std::uint16_t field74, field76;
    };
}
