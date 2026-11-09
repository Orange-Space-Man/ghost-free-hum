#include "game/startup/cutsceneanimationsound.h"

openingscene::AnimationSound::AnimationSound(actoranimation::SoundServices services) : services(std::move(services)) {}
openingscene::AnimationSound::~AnimationSound() = default;
openingscene::AnimationSound::Model::~Model() { sound.stop(); }

void openingscene::AnimationSound::load(const Segment& segment) {
    models.clear();
    for (const auto& source : segment.models) {
        auto model = std::make_unique<Model>();
        for (const auto& resource : source.soundAnimations) {
            actormodel::ResourceRef sound;
            if (!resource.data.empty()) sound = std::make_shared<actormodel::Resource>(actormodel::Resource{resource.name, 0, resource.data});
            model->animations.push_back(std::move(sound));
        }
        models.push_back(std::move(model));
        select(static_cast<unsigned>(models.size() - 1), 0, 0);
    }
}

void openingscene::AnimationSound::select(unsigned index, unsigned animation, float frame) {
    auto& model = *models.at(index);
    model.selected = animation < model.animations.size() ? model.animations[animation] : nullptr;
    model.sound.bind(model.selected, 1, static_cast<unsigned>(frame));
}

void openingscene::AnimationSound::update(unsigned index, float frame, float step) {
    auto& model = *models.at(index);
    if (!model.selected) return;
    if (frame < step) model.sound.bind(model.selected, 1, static_cast<unsigned>(step));
    model.sound.update(frame, step, services);
}
