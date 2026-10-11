#pragma once

#include "game/effects/elementparticlepool.h"
#include "game/effects/particlescenecontext.h"
#include "game/world/roomlocation.h"

namespace elements {
    class Scene {
    public:
        explicit Scene(const std::vector<std::uint8_t>& executable);
        void updateProjectiles(const playeractor::View& player, std::uint32_t actorFlags);
        void update(const playeractor::View& player, std::uint32_t actorFlags,
            particles::SceneContext& context, const roomtraversal::State& visibility);
        Pool pool;
        Projectile ice{}, water{};
        std::array<float, 12> firstTextureMatrix{}, secondTextureMatrix{};
        float particleScale;
        bool paused = false;

    private:
        void initialize(unsigned type, const std::array<float, 3>& position, const std::array<float, 3>& direction);
        void control(Emitter& emitter, const playeractor::View& player, std::uint32_t actorFlags,
            particles::SceneContext& context, const roomtraversal::State& visibility);
        const std::vector<std::uint8_t>& executable;
        roomlocation::Rooms rooms;
        float contextScale;
    };
}
