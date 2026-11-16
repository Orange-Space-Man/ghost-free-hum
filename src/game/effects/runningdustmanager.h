#pragma once

#include "game/effects/particlebillboards.h"
#include "game/effects/dustinteraction.h"
#include "game/effects/particlescenecontext.h"
#include "game/events/jmpmessagesender.h"
#include "game/events/missionresources.h"
#include <functional>

namespace runningdust {
    struct EmitterInteraction {
        unsigned level;
        std::int16_t captureLifetime, releasedLifetime;
        SuctionParameters motion;
    };
    using ParticleInteraction = std::function<ForceResult(InteractionParticle&, const EmitterInteraction&)>;
    class Manager final : public jmp::MessageReceiver {
    public:
        Manager(const std::vector<std::uint8_t>& executable, const mission::Resources& mission,
            const roomlocation::Rooms& rooms, const roomtraversal::State& visibility, std::uint32_t& randomState);
        ~Manager();
        bool create(jmp::ToolDataRef& entry) override;
        bool remove(jmp::ToolDataRef& entry) override;
        void update(particles::SceneContexts& contexts, const ParticleInteraction& interaction = {});
        bool impact(float speed, float radius, const std::array<float, 3>& position,
            const std::array<float, 3>& velocity, const std::array<float, 3>& normal, unsigned level,
            particles::SceneContext& player, const std::function<std::array<float, 3>()>& direction);
        bool release(const std::array<float, 3>& position, unsigned count, particles::SceneContext& player);
        std::vector<particles::Billboard> prepare(const std::array<float, 12>& view) const;
        particles::Billboard material() const;
        unsigned particleCount() const;
        unsigned emitterCount() const;
        unsigned level(unsigned room) const;

    private:
        struct State;
        std::unique_ptr<State> state;
    };
}
