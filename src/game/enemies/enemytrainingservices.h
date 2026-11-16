#pragma once

#include "game/enemies/enemytraininggeneratorstrategy.h"
#include "game/actors/actorinitialpose.h"
#include "game/effects/modeleffectpool.h"

namespace enemystrategy {
    struct TrainingEnvironment {
        enemies::Runtime& runtime;
        enemies::Manager& enemies;
        const enemytypes::Catalog& types;
        const actormodel::InitialPose& pose;
        modeleffects::Factory& models;
        particles::EffectFactory& particles;
        bool allGates = false;
        TrainingServices bind(TrainingGenerator& strategy);
        void openGate(unsigned index);
        void createChild(charactergenerator::Generator& parent, unsigned index);
    };
}
