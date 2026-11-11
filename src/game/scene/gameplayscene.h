#pragma once

#include "game/scene/gameplayresourceowner.h"
#include "platform/d3d11/modelbatchrenderer.h"
#include "game/startup/keypresentationowner.h"
#include "platform/d3d11/j3dpacketrenderer.h"
#include "game/effects/modeleffectlighting.h"
#include "platform/d3d11/particlebillboardrenderer.h"
#include "platform/d3d11/playersheetrenderer.h"
#include "platform/d3d11/roommirrorrenderer.h"

namespace gameplay {
    class Scene {
    public:
        std::string debugInfo();
        Scene(const std::filesystem::path& package, gameentry::State& entry);
        void prepareGraphics();
        bool updateRoomResources();
        void prepareModelGraphics(ID3D11Device* device);
        void update(const playerinput::PadStatus& pad = {}, std::int16_t port = -1, std::uint32_t audioFrame = 0);
        void updatePresentation(bool gameplayTick);
        void updateAudio();
        std::shared_ptr<jaudio::PositionalPlayback> playback();
        void draw(bool present);
        unsigned updates() const;
        const playeractor::View& playerView(unsigned index = 0);
        bool presentKey(bool missingKey, bool front, unsigned doorType);
        bool keyPresentationActive() const;
        bool gameplaySuspended() const;

    private:
        ResourceOwner resources;
        keypresentation::SceneControl sceneControl;
        keypresentation::Owner keyPresentation;
        unsigned presentationButtons = 0;
        originalmath::FloatTrig trig;
        dooranimation::PoseTables poseTables;
        std::vector<float> textureTrigonometry;
        dooranimation::Matrix view{};
        roomportal::Matrix projection{}, visibilityProjection{};
        std::vector<roomtraversal::Submission> rooms;
        std::map<const doors::ModelResource*, modelrenderer::Batches> models;
        std::vector<actors::MoveActor*> actors;
        std::vector<s3dmodel::Model> actorModels;
        std::vector<actormodel::ResourceRef> actorModelResources;
        std::vector<std::size_t> textures;
        modelrenderer::PipelineState pipeline;
        std::array<modelrenderer::StageRegisters, 16> retained{};
        unsigned ticks = 0;
        std::uint8_t flaggedOpacity = 0, limitedOpacity = 0;
        bool overrideRoomOpacity = false;
        float opacityDistance = 0;
        std::array<float, 3> lightOrigin{}, lightTarget{};
        bool useRoomLights = true;
        float equipmentAmbientScale = 0;
        std::array<std::uint8_t, 4> ambient{};
        s3denvironment::Light actorLight, actorHighlight;
        std::unique_ptr<j3dmodel::TransformTables> modelTransforms;
        j3drenderer::PacketRenderer lightRenderer;
        j3dmodel::MaterialState lightMaterialState;
        std::unique_ptr<modeleffects::Lighting> effectLighting;
        j3dmodel::MaterialState effectMaterialState;
        std::map<const j3dmodel::ModelData*, j3drenderer::PacketRenderer> effectRenderers;
        unsigned visibleModelEffects = 0;
        std::unique_ptr<particles::Billboards> particleBillboards;
        particlerenderer::Renderer particleRenderer;
        playersheets::Renderer sheetRenderer;
        roommirror::Renderer mirrorRenderer;
        D3D11_VIEWPORT drawViewport{0, 0, 640, 480, 0, 1};
        void prepareModel(ID3D11Device* device, const doors::ModelResource& model);
        void prepareRoomGraphics(ID3D11Device* device, const roomresidency::Resident& resident);
        void drawSheets(unsigned pass);
        float lightProjectionDepth = 0;
        void drawPlayerLight();
        void drawModelEffects();
        void updateOpacity();
        void publishCamera();
        void drawGameplay(bool present);
        void drawShadows();
        void drawWorld(bool reflection, const s3denvironment::Camera& camera);
        void refreshActorModels();
        void drawActors(unsigned stage, const s3denvironment::Camera& camera, unsigned visibilityBuffer);
        void drawModel(const doors::ModelResource& model, const std::function<const dooranimation::Matrix&(unsigned)>& matrix,
            unsigned pass, std::uint8_t opacity, unsigned room, const roomtraversal::Bounds& bounds,
            bool lowOpacity = false);
    };
}
