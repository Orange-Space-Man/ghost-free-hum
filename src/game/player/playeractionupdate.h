#pragma once

#include "game/player/playeractionstate.h"
#include "game/actors/actorinitialpose.h"
#include "game/events/runtimeflags.h"
#include "game/player/playercontrollerstate.h"
#include "game/actors/moveactor.h"

#include <functional>

namespace playeractions {
    struct Input {
        std::array<std::uint32_t, 4> modes{};
        std::array<std::uint8_t, 7> flags{1, 0, 0, 0, 0, 0, 0};
        std::array<float, 3> direction{}, velocity{};
        float frame = 0;
        std::array<float, 2> thresholds{};
        float maximumSpeed = 0, motion = 0, turn = 0;
        bool useMotion = false;
        std::array<std::uint8_t, 7> animationFlags{};
        std::uint32_t mode = 0;
        actors::MoveActor* actor = nullptr;
        playercomponents::AttachedState* attached = nullptr;
        playerinput::ControllerState* controller = nullptr;
    };

    struct UpdateServices {
        gameflags::State& flags;
        const actormodel::Catalog& catalog;
        std::function<void(std::uint32_t)> sound;
        std::function<std::uint32_t(unsigned)> fallbackDuration;
    };

    class Update {
    public:
        Update(const std::vector<std::uint8_t>& executable, const actormodel::InitialPose& pose);
        unsigned run(Actions& state, const Input& input, const UpdateServices& services) const;
        void advanceReaction(Actions& state, const Input& input, const UpdateServices& services) const;
        unsigned select(Actions& state, const Input& input) const;

    private:
        const std::vector<std::uint8_t>& executable;
        const actormodel::InitialPose& pose;
        std::array<float, 14> constants;
        std::array<std::array<unsigned, 8>, 3> directions;
        unsigned locomotion(Actions& state, const Input& input) const;
        unsigned modeEntry(const Actions& state, const Input& input) const;
    };
}
