#pragma once

#include "game/effects/elementparticlepool.h"
#include "game/effects/particlescenecontext.h"
#include "game/world/roomlocation.h"
#include "game/world/mapcollisionstate.h"
#include "game/effects/particlebillboards.h"
#include "game/math/originalfloattrig.h"

namespace elements {
    class Scene {
    public:
        explicit Scene(const std::vector<std::uint8_t>& executable);
        void updateProjectiles(const playeractor::View& player, std::uint32_t actorFlags);
        void refreshForceRanges(particles::SceneContext& context) const;
        void update(const playeractor::View& player, std::uint32_t actorFlags,
            particles::SceneContext& context, const roomtraversal::State& visibility);
        void updateBlow(const playeractor::View& player, std::uint32_t actorFlags,
            particles::SceneContext& context, const mapcollision::State& collision, std::uint32_t& randomState);
        void updateBreath(const playeractor::View& player, particles::SceneContext& context,
            const mapcollision::State& collision, std::uint32_t& randomState);
        particles::Billboard blowMaterial() const;
        particles::Billboard breathMaterial() const;
        std::vector<particles::Billboard> prepareBlow(const std::array<float, 12>& view) const;
        std::vector<particles::Billboard> prepareBreath(const std::array<float, 12>& view) const;
        Pool pool;
        Projectile ice{}, water{};
        std::array<float, 12> firstTextureMatrix{}, secondTextureMatrix{};
        float particleScale;
        bool paused = false;

    private:
        void initialize(unsigned type, const std::array<float, 3>& position, const std::array<float, 3>& direction);
        void control(Emitter& emitter, const playeractor::View& player, std::uint32_t actorFlags,
            particles::SceneContext& context, const roomtraversal::State& visibility);
        std::vector<particles::Billboard> prepareElement(const Emitter& emitter, particles::Billboard material,
            const std::array<float, 12>& view) const;
        const std::vector<std::uint8_t>& executable;
        roomlocation::Rooms rooms;
        float contextScale;
        Emitter* blow = nullptr;
        Emitter* breath = nullptr;
        particles::Resource blowImage;
        particles::Resource breathImage;
        std::array<std::uint8_t, 168> breathDefaults{};
        std::array<std::uint8_t, 48> breathModes{};
        std::array<float, 4096> sine{};
        std::array<std::array<float, 3>, 4> corners{};
        std::array<std::array<float, 2>, 4> coordinates{};
        originalmath::FloatTrig trig;
        float blowThreshold = 0, timerScale = 0, timerBase = 0, timerControl = 0;
        float collisionThreshold = 0, collisionScale = 0, maximumParticleSpeed = 0, radiansPerDegree = 0;
        float breathSpecialScale = 0, breathSpecialRadius = 0;
    };
}

