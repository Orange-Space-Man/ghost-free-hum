#pragma once

#include "game/actors/actoreffectstate.h"
#include "game/actors/actoranimationsound.h"
#include "game/actors/actormodelinstance.h"
#include <functional>

namespace actors {
    class MoveActor;
}

namespace actormodel {
    class Catalog;
    class Resources;
    class InitialPose;
}

namespace playercomponents {
    struct AttachedInput {
        bool enabled = false, aim = false;
        unsigned mode = 0, selection = 0;
        float horizontal = 0, vertical = 0, horizontalScale = 1, verticalScale = 1, frame = 0;
        std::array<float, 3> sourceDirection{}, targetDirection{};
        bool inverted = false;
    };

    struct AttachedOutput {
        std::array<std::int32_t, 2> limits{};
        std::array<std::int32_t, 2> morphs{-1, -1}, textures{-1, -1};
    };

    class AttachedState {
    public:
        AttachedState(const std::vector<std::uint8_t>& executable, int actorType, actormodel::Instance& model,
            actors::MoveActor* actor = nullptr);
        ~AttachedState();
        AttachedState(const AttachedState&) = delete;
        AttachedState& operator=(const AttachedState&) = delete;
        std::array<std::int32_t, 2> setBlend(double first, double second);
        void stopFirstTransition();
        void stopSecondTransition();
        void startFirstTransition(unsigned state, double speed);
        void startSecondTransition(unsigned state, double speed);
        AttachedOutput update(const AttachedInput& value, const actormodel::Catalog& catalog,
            const actormodel::Resources& resources, const actormodel::InitialPose& pose);
        void advance(double rate, const actormodel::Catalog& catalog, const actormodel::Resources& resources,
            const std::function<void(unsigned, const std::array<float, 3>&)>& emit);
        void finish(double rate, const actormodel::Catalog& catalog, const actormodel::Resources& resources,
            const std::function<void(unsigned, const std::array<float, 3>&)>& emit,
            const actoranimation::SoundServices& playback);
        AttachedInput input;
        std::array<std::uint32_t, 53> parameters;
        actors::MoveActor* actor;
        std::array<actoreffects::State, 3> effects;
        float field240, field244;
        std::uint32_t field248 = 1;
        std::array<std::uint32_t, 9> fields24C, fields270, fields294;
        std::array<std::uint8_t, 5> fields2B8{0, 0, 0, 1, 0};
        std::uint32_t field2C0 = 0;
        float field2C4 = 0, field2C8 = 1, field2CC = 0;
        std::uint32_t field2D0 = 0;
        float field2D4 = 0, field2D8 = 1;
        std::int32_t field2DC = -1;
        actoranimation::SoundState sound;
        std::uint8_t field368 = 0, field36C = 0;

    private:
        void updateOverrides();
        actormodel::Instance& model;
    };

    class AttachedSingle {
    public:
        AttachedSingle(const std::vector<std::uint8_t>& executable, int actorType, actormodel::Instance& model,
            actors::MoveActor* actor = nullptr);
        ~AttachedSingle();
        AttachedSingle(const AttachedSingle&) = delete;
        AttachedSingle& operator=(const AttachedSingle&) = delete;
        std::array<std::uint32_t, 10> parameters;
        actors::MoveActor* actor;
        std::uint8_t field28 = 0, field29 = 1;
        float field2C = 1, field30 = 0, field34 = 0, field38 = 0;
        std::uint8_t field3C = 0;
        actoreffects::State effect;

    private:
        actormodel::Instance& model;
    };
}
