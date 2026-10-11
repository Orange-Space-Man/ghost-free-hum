#pragma once

#include "game/effects/particlepoolstate.h"
#include "game/actors/actorcollisionquery.h"

namespace elements {
    struct Particle {
        Particle();
        std::array<float, 3> position;
        std::int16_t age;
        Particle* next;
        Particle* previous;
    };

    struct Emitter {
        Emitter();
        particles::Ring<Particle> particles;
        std::uint8_t field2E = 0;
        std::int32_t type;
        std::array<std::uint8_t, 168> parameters{};
        std::array<float, 3> position{}, direction{}, velocity{}, minimum{}, maximum{};
        std::uint32_t room = 0xffffffff;
        std::uint16_t field2C = 0;
        float field28 = 0;
        std::uint32_t field38 = 0;
        std::uint8_t field3C = 0, field3D = 0, field1B4 = 0;
        std::uint32_t initializer = 0, motion = 0, control = 0, progression = 0;
        Emitter* next;
        Emitter* previous;
    };

    class Pool {
    public:
        Pool(unsigned particles, unsigned emitters);
        bool intersects(const actorcollision::Shape& target, const actorcollision::Vector& position, int type) const;
        bool intersects(const Emitter& emitter, const actorcollision::Shape& target,
            const actorcollision::Vector& position) const;
        particles::Pool<Particle> particles;
        particles::Pool<Emitter> emitters;
        particles::Ring<Emitter> active;
        actorcollision::Box particleBox{{100, 100, 100}, {}};
    };

    struct Projectile {
        std::array<float, 3> position;
        std::uint32_t active;
        std::uint32_t field98 = 0, field9C = 0, fieldA4 = 0, fieldA8 = 0, fieldAC = 0, fieldB0 = 0;
    };

    bool intersects(const Projectile& ice, const Projectile& water, const actorcollision::Shape& target,
        const actorcollision::Vector& position, int type);
}
