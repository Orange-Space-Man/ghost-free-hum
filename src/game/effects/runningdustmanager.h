#pragma once

#include "game/effects/particlebillboards.h"
#include "game/effects/particlescenecontext.h"
#include "game/events/jmpmessagesender.h"
#include "game/events/missionresources.h"

namespace runningdust {
    class Manager final : public jmp::MessageReceiver {
    public:
        Manager(const std::vector<std::uint8_t>& executable, const mission::Resources& mission,
            const roomlocation::Rooms& rooms, const roomtraversal::State& visibility, std::uint32_t& randomState);
        ~Manager();
        bool create(jmp::ToolDataRef& entry) override;
        bool remove(jmp::ToolDataRef& entry) override;
        void update(particles::SceneContexts& contexts);
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
