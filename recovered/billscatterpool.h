#pragma once

#include "game/effects/particlepoolstate.h"
#include "jlib/j3d/j3dmodelinstance.h"

namespace billscatter {
    using Vector = std::array<float, 3>;

    struct Particle {
        Particle() : next(this), previous(this) {}
        std::unique_ptr<j3dmodel::ModelInstance> model;
        Vector position{};
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
        std::int16_t lifetime = 0;
        std::uint8_t delay = 0, suctionCounter = 0;
        bool suctionReady = false;
        Request* next;
        Request* previous;
    };

    class Pool;
    struct Services {
        virtual ~Services() = default;
        virtual std::uint32_t roomAt(const Vector& position, std::uint32_t previous) = 0;
        virtual std::uint16_t visibilityFlags(std::uint32_t room) = 0;
        virtual bool advance(Pool& pool, Request& request, Particle& particle) = 0;
    };

    class Pool {
    public:
        explicit Pool(const std::shared_ptr<j3dmodel::ModelData>& model);
        void update(bool paused, std::uint8_t suctionDelay, std::uint8_t blinkThreshold, Services& services);
        void recycle(Request& request, Particle& particle);
        particles::Pool<Particle> particles{30};
        particles::Pool<Request> requests{15};
        particles::Ring<Request> active;

    private:
        bool updateRequest(Request& request, std::uint8_t blinkThreshold, Services& services);
    };

    class Owners {
    public:
        explicit Owners(const std::array<std::shared_ptr<j3dmodel::ModelData>, 3>& models);
        void update(bool paused, std::uint8_t suctionDelay, std::uint8_t blinkThreshold, Services& services);
        std::array<std::unique_ptr<Pool>, 3> pools;
    };
}
