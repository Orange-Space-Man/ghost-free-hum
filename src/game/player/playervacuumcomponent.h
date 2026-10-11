#pragma once

#include "game/player/playervacuummodel.h"

namespace playercomponents {
    struct VacuumGlobals {
        std::array<playermodels::VacuumParameters, 2> parameters;
        std::uint32_t field8C18 = 0;
        void initialize();
    };

    class Vacuum {
    public:
        Vacuum(const playermodels::SharedModels& shared, VacuumGlobals& globals);
        std::unique_ptr<playermodels::VacuumModel> model;
        float field4 = 0;
        std::array<std::uint8_t, 3> fields8{};
        std::array<std::uint32_t, 16> fieldsC{};
        std::array<float, 5> fields4C{0, 0, 0, 1, 2};
        std::array<std::uint8_t, 0x14> fields60;
        std::uint32_t field74 = 0;
        std::array<std::uint8_t, 8> fields78;
    };
}
