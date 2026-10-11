#pragma once

#include "game/player/playeractorcomponents.h"

namespace playeractor {
    struct ViewEnvironment {
        const actorcollision::Catalog& collisions;
        const std::array<actors::MoveActor*, 128>& actors;
        const std::vector<std::uint32_t>& roomFlags;
        unsigned mapMode;
        bool flag3D;
    };

    struct View {
        void refresh(actors::MoveActor& actor, State& state, Components& components,
            const Globals& globals, const ViewEnvironment& environment);
        const std::array<float, 3>* position;
        const std::array<float, 3>* velocity;
        float magnitude;
        const playerorientation::Quaternion* quaternion;
        const std::array<float, 3>* direction;
        std::int16_t angle;
        unsigned id, type, room, previousRoom;
        std::uint8_t field834;
        const actorcollision::Volume* volume;
        enemies::Owner* field83C;
        const std::array<float, 3>* field840;
        const std::array<float, 3>* field844;
        float field848, field84C;
        playercontrols::State* controls;
        std::uint16_t life, maximumLife;
        std::int16_t field858, field85A, field85C, field85E;
        std::uint8_t field860, field861, field862, field863;
        std::uint32_t field864;
        playercomponents::FlagBuffer* flags;
        std::array<playercomponents::BitBuffer*, 5> bits;
        unsigned field880;
        std::uint8_t field884, field885, field886;
        std::uint32_t field888, field88C;
        const std::array<float, 3>* field890;
        std::uint8_t field894, field895;
        const std::array<float, 3>* field898;
        std::uint8_t field89C, field89D, field89E;
        std::uint32_t field8A0;
        const std::array<float, 3>* field8A4;
        const std::array<float, 3>* field8A8;
        float field8AC, field8B0;
        playermodels::VacuumVolume* field8B4;
        playermodels::LightModel* field8B8;
        float field8C0;
        const actormodel::Matrix* field8C4;
        std::uint8_t field8C8, field8C9, field8CA;
        const std::array<float, 3>* field8CC;
        std::uint16_t field8D0;
        const std::array<std::uint32_t, 7>* field8D4;
        std::array<std::uint32_t, 381> counts;
        std::uint32_t count, fieldED0;
        std::array<std::uint32_t, 12> fieldsED4;
        std::array<std::uint8_t, 5> flagsF04;
        std::uint8_t fieldF09, fieldF0A;
        const std::array<float, 3>* fieldF0C;
        std::uint8_t fieldF10, fieldF11;
        std::array<std::uint8_t, 3> flagsF12;
        std::uint8_t fieldF15, fieldF16, fieldF17;
        const std::array<float, 3>* fieldF18;
        const std::array<float, 3>* fieldF1C;
        float fieldF20, fieldF24, fieldF28;
        std::uint8_t fieldF2C, fieldF2D, fieldF2E;
    };
}
