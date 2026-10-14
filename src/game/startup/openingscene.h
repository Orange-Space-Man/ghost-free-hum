#pragma once

#include "game/startup/openingtimeline.h"
#include "game/models/s3denvironment.h"
#include "game/models/s3dmorph.h"
#include "game/models/s3dshadow.h"

namespace openingscene {
    class Scene {
    public:
        bool load(const std::filesystem::path& path, unsigned cutscene, std::string& error);
        void prepareGraphics();
        void reset();
        void update(bool flag55 = false);
        bool clearsEventFlag() const;
        void draw(bool present);
        void drawFrozen(bool present, unsigned fadeAlpha);
        bool finished() const;

    private:
        struct AnimationState {
            std::vector<s3danimation::Animation> skeletal;
            std::vector<s3denvironment::ModelEffect> effects;
            std::vector<bool> active;
            std::vector<float> effectFrames;
            std::vector<float> effectDrawFrames;
            std::vector<s3dmorph::Animation> morphs;
            std::vector<bool> morphActive;
            std::vector<float> morphFrames;
            std::vector<float> morphDrawFrames;
            unsigned animation = 0, flags = 0;
            float frame = 0;
            float drawFrame = 0;
        };
        Resources resources;
        struct SegmentState {
            std::vector<s3dmodel::Model> models;
            std::vector<AnimationState> animations;
            s3denvironment::CameraAnimation camera;
            s3denvironment::Environment environment;
            s3dshadow::Animation shadow;
        };
        std::vector<SegmentState> prepared;
        Timeline timeline;
        std::vector<s3dmodel::Model> models;
        std::vector<AnimationState> animations;
        std::vector<std::vector<s3dmodel::Batch>> batches;
        std::vector<float> trigonometry;
        s3denvironment::CameraAnimation camera;
        s3denvironment::Environment environment;
        s3dshadow::Animation shadow;
        unsigned loadedSegment = ~0U;
        std::size_t texture = 0;
        bool graphicsReady = false, stepped = false;
        void loadSegment();
        std::string graphicsKey(unsigned segment) const;
    };
}
