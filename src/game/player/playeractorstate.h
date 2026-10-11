#pragma once

#include "game/actors/moveactor.h"
#include "game/player/playerorientation.h"
#include "game/player/playerrecordwriteback.h"
#include "game/player/playercontrolstate.h"

namespace playermodels { class VacuumVolume; }
namespace playerinput { class ControllerState; }

namespace playeractor {
    struct Target {
        playermodels::VacuumVolume* model;
        std::uint32_t flags;
        actors::MoveActor** actor;
        float horizontal = 0, vertical = 0;
    };

    struct Globals {
        std::int32_t field2C0C = 300, field2C14 = 30;
        float field2BE4 = 70, field2BF4 = 250;
    };

    struct State {
        State(const actors::MoveActor& actor, const Globals& globals, const actormodel::InitialPose& pose);
        std::uint8_t field790 = 0;
        std::uint16_t flags792 = 0;
        void* field794 = nullptr;
        void* field798 = nullptr;
        playerrecord::Record* record = nullptr;
        playerinput::ControllerState* field7A0 = nullptr;
        actors::MoveActor* field7A4 = nullptr;
        actors::MoveActor* field7A8 = nullptr;
        actors::MoveActor* field7AC = nullptr;
        std::uint8_t field7B0 = 0;
        enemies::Owner* field7B4 = nullptr;
        unsigned fieldF30;
        std::int32_t fieldF34 = -1;
        playerorientation::Quaternion quaternion;
        std::array<float, 3> direction;
        float fieldF54 = 100, fieldF58 = 200;
        std::uint32_t fieldF5C = 255;
        float fieldF60 = 1, fieldF64 = 1, fieldF68 = 1, fieldF6C = 1;
        std::uint8_t fieldF70 = 0;
        std::array<float, 3> fieldF74{};
        std::array<float, 3> fieldF80{0, 0, 1};
        float fieldF8C = 0, fieldF90 = 0;
        std::int32_t fieldF94 = -1;
        std::uint8_t fieldF98 = 0;
        playercontrols::Motion fieldF9C;
        const playercontrols::Motion* fieldFB8 = nullptr;
        const playercontrols::Motion* fieldFBC = nullptr;
        std::array<std::uint8_t, 4> flagsFC0{};
        std::array<float, 3> fieldFC4{}, fieldFD0{0, 0, 1};
        float fieldFDC = 0, fieldFE0 = 0;
        std::array<float, 3> fieldFE4, fieldFF0;
        std::uint16_t maximumLife = 100;
        float field1000 = 50, field1004, field1008 = 0;
        std::uint8_t field100C = 0;
        actormodel::Matrix matrix1010{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        std::array<std::uint8_t, 3> flags1040{};
        std::uint32_t field1044 = 0;
        std::array<float, 3> field1048;
        std::int32_t field1054 = -1;
        std::uint8_t field1058 = 0;
        float field105C = 0;
        std::uint8_t field1060 = 0, field1061 = 0;
        actors::MoveActor* field1064 = nullptr;
        std::int32_t field1068 = -1;
        float field106C = 0;
        std::uint32_t field1070 = 0;
        std::array<float, 3> field1074{}, field1080{};
        float field108C = 1;
        std::array<float, 3> field1090{0, 0, 1};
        float field109C = 0;
        float field10A0 = 0;
        std::array<float, 3> field10A4;
        std::uint8_t field10B0 = 0;
        std::array<float, 3> field10B4{};
        std::uint32_t field10C0 = 0;
        float field10C4 = 0, field10C8 = 0, field10CC = 0;
        std::uint8_t field10D0 = 0, field10D1 = 0;
        std::array<float, 3> field10D4{};
        std::int16_t field10E0 = -1;
        std::uint32_t field10E4 = 0;
        std::array<std::uint8_t, 3> flags10E8{};
        std::array<actors::MoveActor*, 4> fields10EC{};
        std::array<float, 3> field10FC{0, 0, 1};
        float field1108 = 1, field110C = 0, field1110 = 0, field1114 = 0;
        float field1118 = 0, field111C = 0, field1120 = 0, field1124 = 0;
        std::uint32_t field1128 = 0, field112C = 0;
        float field1130 = 0, field1138 = 0, field113C = 0;
        std::uint32_t field1134 = 0;
        std::array<std::uint8_t, 4> flags1140{};
        std::array<float, 3> field1144{0, 0, 1};
        float field1150 = 0;
        std::array<float, 3> field1154{}, field1160{0, 0, 1};
        float field116C = 0, field1170 = 0, field1174 = 0, field1178 = 1, field117C = 0;
        std::uint32_t field1180 = 0, field1184 = 1;
        float field1188 = 0, field118C = 0;
        std::uint32_t field1190 = 0, field1194 = 0, field1198 = 0;
        std::uint8_t field119D = 1, field119E = 1;
        Target* field11A0 = nullptr;
        std::array<float, 3> field11A4{};
        std::array<std::uint32_t, 7> fields11B0{};
        float field11CC = 0, field11D0 = 0;
        std::uint8_t field11D4 = 0;
        std::array<std::uint32_t, 3> fields11D8{};
        std::uint8_t field11E4 = 0;
        std::int32_t field11E8 = -1;
        std::uint32_t entryCount = 0;
        std::array<std::uint32_t, 64> entries{};
        std::uint32_t field12F0 = 0;
        std::array<std::uint32_t, 12> fields12F4{};
        std::uint16_t field1324 = 20;
        std::uint32_t field1328 = 0, field132C;
        std::array<std::uint8_t, 4> flags1330{};
        std::uint8_t field1334 = 0;
        std::uint32_t field1338 = 1;
        std::uint32_t field133C = 0;
        float field1340 = 0, field1344 = 0, field1348 = 0;
        std::array<std::uint8_t, 3> flags134C{};
        std::array<float, 3> field1350{};
        std::uint32_t field135C = 0;
        float field1360 = 0, field1364 = 0;
        std::array<std::uint8_t, 3> flags1388{};
        std::array<std::uint32_t, 6> fields138C{};
    };
}
