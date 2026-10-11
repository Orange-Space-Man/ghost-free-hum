#pragma once

#include "game/enemies/enemystrategy.h"

#include <array>
#include <cstdint>
#include <memory>

namespace actors { class MoveActor; }

namespace characterownership {
    class Resource {
    public:
        virtual ~Resource() = default;
        virtual void release() = 0;
        virtual int attachmentState() = 0;
        virtual void setAttachmentPosition(const std::array<float, 3>& position, const std::array<float, 3>& vector) = 0;
        virtual void setMatrix(const std::array<float, 12>& matrix) = 0;
        virtual void setVisible(bool visible) = 0;
        virtual void setRunning(bool running) = 0;
    };

    class Resources {
    public:
        virtual ~Resources() = default;
        virtual Resource* createPrimary(unsigned type, const std::array<float, 3>& position) = 0;
        virtual Resource* createSecondary(unsigned type, const std::array<float, 3>& position,
            const std::array<float, 3>& vector) = 0;
        virtual std::int32_t playerValue() = 0;
        virtual unsigned gameVariant() = 0;
    };

    struct Acquired {
        Resource* first = nullptr;
        Resource* second = nullptr;
        bool success = false;
    };

    Acquired acquire(unsigned type, Resources& resources);

    using Strategy = enemystrategy::Strategy;

    struct Attachment {
        Resource* resource = nullptr;
        unsigned joint = 0;
        std::array<float, 3> position{};
    };

    class ActorLinks {
    public:
        virtual ~ActorLinks() = default;
        virtual void setInactiveFlags() = 0;
        virtual void* child(unsigned index) = 0;
        virtual void detach(void* child) = 0;
        virtual void deactivate(void* child) = 0;
        virtual void releaseAudio() = 0;
    };

    class Ownership {
    public:
        void reset();
        bool acquire(unsigned type, Resources& resources, Acquired& acquired);
        void attach(const Acquired& acquired);
        void deactivate(ActorLinks& actor);
        void releaseShared(void*& user);
        void updateAttachments(const actors::MoveActor& actor);
        std::unique_ptr<Strategy> strategy;
        std::array<void*, 3> sharedUsers{};
        Resource* sharedResource = nullptr;
        std::int32_t sharedCount = 0;
        void* dependentActor = nullptr;
        std::array<Attachment, 5> attachments{};
        unsigned attachmentCount = 0;

    private:
        void attach(Resource* resource);
    };
}
