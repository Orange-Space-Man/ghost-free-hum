#pragma once

#include "game/actors/actorinitialpose.h"
#include "game/actors/actoranimationsound.h"
#include "game/actors/actormodelresources.h"
#include "game/enemies/enemytypeparameters.h"
#include "game/actors/actorcollisionowner.h"
#include "game/enemies/enemymanager.h"
#include "game/world/roomlocation.h"
#include "game/actors/actorcapturemotion.h"

namespace mapcollision { class State; }
namespace playerresources { class RecordPool44; }
namespace gameflags { class State; }

namespace actors {
    class Registry;
    struct Globals {
        std::uint32_t color = 0xffffffff;
        float fieldA4 = 40, fieldA8 = 200;
    };

    struct Construction {
        const actormodel::Catalog& catalog;
        const actormodel::Resources& resources;
        const enemytypes::Manager& parameters;
        const actormodel::InitialPose& pose;
        Globals& globals;
        const roomlocation::Rooms& rooms;
    };

    class MoveActor : public enemies::Actor {
    public:
        MoveActor(const Construction& construction, unsigned id, unsigned type, unsigned capacity);
        virtual ~MoveActor();
        MoveActor(const MoveActor&) = delete;
        MoveActor& operator=(const MoveActor&) = delete;
        void selectAnimation(unsigned state);
        void advanceAnimation();
        void updateAnimationSound(const actoranimation::SoundServices& services);
        void finishFrame();
        void samplePose(const actormodel::Matrix* root = nullptr);
        void followPath();
        void approachRotation();
        void copyLinkedPose();
        void updateMorph();
        void updateMaterials();
        const s3dmodel::Model& surface() const;
        bool resolveMapContact(const mapcollision::State& collision, const actorcollision::Catalog& catalog,
            std::int16_t& wall);
        void updateMapContact(const mapcollision::State& collision, const actorcollision::Catalog& catalog);
        bool resolveActorContact(MoveActor& other, const actorcollision::Manager& collision);
        void updateActorContacts(const Registry& registry, enemies::Manager& manager,
            const actorcollision::Manager& collision, const playerresources::RecordPool44& pool);
        virtual void contactEvent(const Registry& registry, enemies::Manager& manager, std::array<float, 3>& position);
        void updateContactFrame(const Registry& registry, enemies::Manager& manager, const actorcollision::Manager& collision,
            const playerresources::RecordPool44& pool, const mapcollision::State& map, const actorcollision::Catalog& catalog,
            gameflags::State& flags);
        bool selectAnimation(std::string_view name);
        void selectTexture(std::int16_t texture, unsigned channel);
        std::uint32_t hitDamage() const;
        unsigned id() const override;
        unsigned type() const override;
        unsigned room() const override;
        unsigned resolvePositionRoom() const;
        void setPosition(const std::array<float, 3>& value) override;
        void setRotation(const std::array<std::int16_t, 3>& value) override;
        void setVelocity(const std::array<float, 3>& value);
        void visibility(std::uint8_t primary, std::uint8_t secondary) override;
        void resetOwner(const jmp::ToolDataRef& source);
        void initializeType(unsigned type);
        void retainCapture(const std::array<std::uint8_t, 0x430>& retained);
        actorcapture::State* captureState();
        const actorcapture::State* captureState() const;
        void initializeCapture(actorcapture::Motion& motion, const std::array<float, 3>& target, std::uint16_t type);
        void updateCapture(actorcapture::Motion& motion, const playermodels::VacuumPath* path,
            const actorcapture::Vector* suctionDirection, actormodel::Matrix& retainedTransform,
            const actorcapture::Vector* originalScratch = nullptr, const float* originalRegister = nullptr,
            const bool* emptyCompletion = nullptr);
        bool attachChild(MoveActor& child, unsigned joint, const actormodel::Matrix* transform, std::uint8_t orientation);
        bool updateChildAttachment(MoveActor& child, unsigned joint, const actormodel::Matrix* transform, std::uint8_t orientation);
        bool detachChild(MoveActor& child);
        actormodel::Matrix childMatrix(unsigned index) const;
        void updateChildren();
        unsigned actorId = 0, actorType = 0;
        std::uint32_t flags = 0;
        std::unique_ptr<actormodel::Instance> model;
        actormodel::Matrix matrix;
        std::array<float, 3> position{}, previousPosition{}, field5C{};
        float speed = 0;
        std::array<float, 3> velocity{}, direction{};
        std::array<std::int16_t, 3> rotation{}, previousRotation{};
        std::array<float, 3> scale{1, 1, 1};
        std::int32_t field9C = -1, fieldA0 = -1;
        float fieldA4 = 0, fieldA8 = 0;
        MoveActor* poseLinkedActor = nullptr;
        std::int16_t fieldB0 = -1, fieldB2 = -1;
        std::uint32_t fieldB4 = 0xffffffff;
        std::array<std::uint8_t, 12> fieldsB8;
        unsigned animationState = 255, keyIndex = 0;
        float animationClock = 0, animationFrame = 0, animationFrameStep = 0, animationSpeed = 1;
        std::uint16_t blendFramesRemaining = 0, blendFramesTotal = 0;
        std::uint32_t fieldE0, fieldE4;
        std::uint8_t fieldE8 = 4;
        std::array<std::uint8_t, 3> fieldsE9{64, 64, 64};
        std::uint8_t fieldEC = 255;
        float fieldF0 = 0;
        actormodel::ResourceRef fieldF4;
        std::int16_t fieldF8 = -1, fieldFA = -1;
        std::uint16_t life = 1;
        std::array<float, 3> field100{};
        float field160;
        actoranimation::SoundState sound;
        MoveActor* parent = nullptr;
        struct Child {
            MoveActor* actor = nullptr;
            unsigned joint;
            bool transformed;
            actormodel::Matrix transform;
            std::uint8_t orientation;
        };
        std::array<Child, 7> children;
        const actorcollision::Volume* field788 = nullptr;
        float field78C;

    protected:
        const Construction& construction;

    private:
        std::unique_ptr<actorcapture::State> capture;
    };
}
