#pragma once

#include "game/scene/gameplayresourceowner.h"
#include "platform/d3d11/modelbatchrenderer.h"
#include "game/startup/keypresentationowner.h"

namespace gameplay {
    class Scene {
    public:
        std::string debugInfo();
        Scene(const std::filesystem::path& package, gameentry::State& entry);
        void prepareGraphics();
        void update(const playerinput::PadStatus& pad = {}, std::int16_t port = -1);
        void draw(bool present);
        unsigned updates() const;
        bool presentKey(bool missingKey, bool front, unsigned doorType);
        bool keyPresentationActive() const;
        bool gameplaySuspended() const;

    private:
        ResourceOwner resources;
        keypresentation::SceneControl sceneControl;
        keypresentation::Owner keyPresentation;
        unsigned presentationButtons = 0;
        bool presentationTick = false;
        originalmath::FloatTrig trig;
        dooranimation::PoseTables poseTables;
        dooranimation::Matrix view{};
        roomportal::Matrix projection{}, visibilityProjection{};
        std::vector<roomtraversal::Submission> rooms;
        std::map<const doors::ModelResource*, modelrenderer::Batches> models;
        std::vector<actors::MoveActor*> actors;
        std::vector<s3dmodel::Model> actorModels;
        std::vector<std::size_t> textures;
        modelrenderer::PipelineState pipeline;
        std::array<modelrenderer::StageRegisters, 16> retained{};
        unsigned ticks = 0;
        std::uint8_t flaggedOpacity = 0, limitedOpacity = 0;
        float opacityDistance = 0;
        std::array<float, 3> lightOrigin{}, lightTarget{};
        s3denvironment::Light actorLight;
        void updateOpacity();
        void drawGameplay(bool present);
        void drawActors(unsigned stage);
        void drawModel(const doors::ModelResource& model, const std::function<const dooranimation::Matrix&(unsigned)>& matrix,
            unsigned pass, std::uint8_t opacity, unsigned room, const roomtraversal::Bounds& bounds);
    };
}
