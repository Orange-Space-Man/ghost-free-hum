#pragma once

#include "game/actors/actorinitialpose.h"
#include "game/actors/characterowner.h"
#include "game/actors/moveactor.h"
#include "game/enemies/enemymanager.h"
#include "game/enemies/enemystrategy.h"
#include "game/enemies/enemytypeparameters.h"
#include <array>
#include <cstdint>
#include <functional>

namespace ghostparent {
    enum class Parameter : unsigned {
        Speed = 2,
        SpeedUnseen = 3,
        SpeedFight = 4,
        Eyesight = 5,
        AttackPattern1 = 15,
        AttackKarakai = 9,
        AttackOrooro = 10,
        ActAfterAttackSuccess = 16,
        ActAfterAttackFailure = 17,
        TsuriType = 18,
        Element = 19
    };

    struct Overrides {
        std::int32_t place = -1, movement = -1, search = -1, condition = -1, appearance = -1;
    };

    struct Fields {
        Overrides overrides;
        std::int32_t placementId = -1;
        std::uint32_t element = 0;
        bool lightActive = false;
        bool holding = true;
        bool skipElementEffect = false;
        std::uint8_t field5A2 = 0;
        std::int32_t invisibleMode = 0;
        std::uint32_t fadeScaled = 0;
        std::array<float, 3> knockback{}, knockbackBase{};
        std::int32_t knockbackFrames = 0;
    };

    std::int32_t parameterWord(const gameparameters::Parameters& parameters, Parameter parameter);
    float speedForMode(const gameparameters::Parameters& parameters, int mode);
    float randomOffset(float base, const std::function<std::uint32_t()>& random);
    void faceTarget(actors::MoveActor& actor, const actormodel::InitialPose& pose, const std::array<float, 3>& target,
        std::array<float, 3>* displacement = nullptr);
    void afterAttack(enemystrategy::Strategy& strategy, int action);
    void scale(std::array<float, 3>& vector, double factor);
    double reciprocalSquareRoot(double value);
    void updateKnockback(Fields& fields, actors::MoveActor& actor);
    int maximumAlpha(const Fields& fields);
    int minimumAlpha(const Fields& fields);
    void updateFade(const Fields& fields, characterowner::State& state, const actors::MoveActor& actor);
    int sightQuery(enemies::Manager& manager, const actors::MoveActor& actor, std::int32_t eyesight,
        std::array<float, 3>* displacement = nullptr);
}
