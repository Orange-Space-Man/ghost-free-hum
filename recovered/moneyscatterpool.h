#pragma once

#include "game/effects/particlepoolstate.h"
#include "jlib/j3d/j3dmodelinstance.h"

namespace moneyscatter {
    using Vector = std::array<float, 3>;

    struct Particle {
        Particle() : next(this), previous(this) {}
        Vector position{};
        std::unique_ptr<j3dmodel::ModelInstance> model;
        Vector acceleration{}, velocity{}, forward{}, up{};
        std::array<std::int16_t, 3> angles{};
        float spin = 0, scale = 0;
        std::int16_t rotationPhase = 0, angularVelocity = 0;
        std::uint8_t sparkleCounter = 0;
        bool placedMotion = false;
        std::int32_t motion = 0;
        std::uint32_t room = 0xffffffff, currentRoom = 0xffffffff;
        std::int16_t lifetime = 0;
        bool visible = false, blinking = false;
        std::uint8_t blinkCounter = 0;
        Particle* next;
        Particle* previous;
    };

    struct Request {
        Request() : next(this), previous(this) {}
        particles::Ring<Particle> particles;
        Vector position{}, direction{};
        float speed = 0;
        std::uint8_t count = 0, positionMode = 0;
        std::int16_t lifetime = 0;
        std::uint8_t suctionCounter = 0, collectionCounter = 0;
        bool suctionReady = false, collectionReady = false, placedMoney = false;
        Request* next;
        Request* previous;
    };

    struct Timing {
        std::uint8_t suctionDelay, collectionDelay, blinkThreshold;
    };

    class Pool;

    struct Services {
        virtual ~Services() = default;
        virtual void initializeParticle(Request& request, Particle& particle) = 0;
        virtual std::uint32_t roomAt(const Vector& position, std::uint32_t previousRoom) = 0;
        virtual std::uint16_t roomFlags(std::uint32_t room) = 0;
        virtual bool advanceParticle(Pool& pool, Request& request, Particle& particle) = 0;
        virtual void updateAuxiliaryModel() = 0;
    };

    class Pool {
    public:
        explicit Pool(Timing timing, bool reservePersistent = true);
        void initializePersistent();
        Request* scatter(const Vector& position, const Vector& direction, std::uint8_t count,
            std::uint16_t lifetime, double speed, std::uint8_t positionMode, Services& services);
        void update(Services& services);
        void recycle(Request& request, Particle& particle);
        Request& persistent();
        bool paused = false;
        particles::Pool<Particle> particles{200};
        particles::Pool<Request> requests{35};
        particles::Ring<Request> active;

    private:
        bool updateRequest(Request& request, Services& services);
        Timing timing;
        Request* retained;
    };
}
