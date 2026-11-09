#pragma once

#include "game/startup/openingresources.h"
#include "game/actors/actoranimationsound.h"

namespace openingscene {
    class AnimationSound {
    public:
        explicit AnimationSound(actoranimation::SoundServices services);
        ~AnimationSound();
        AnimationSound(const AnimationSound&) = delete;
        AnimationSound& operator=(const AnimationSound&) = delete;
        void load(const Segment& segment);
        void select(unsigned model, unsigned animation, float frame);
        void update(unsigned model, float frame, float step);

    private:
        struct Model {
            std::vector<actormodel::ResourceRef> animations;
            actoranimation::SoundState sound;
            actormodel::ResourceRef selected;
            ~Model();
        };
        actoranimation::SoundServices services;
        std::vector<std::unique_ptr<Model>> models;
    };
}
