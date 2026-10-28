#pragma once

#include "game/world/ladderpaths.h"
#include "game/player/playeractioncontext.h"

namespace playerinteraction {
    struct LadderInput {
        ladders::Vector position{};
        const playercontrols::Motion* motion = nullptr;
        float speed = 0, radius = 0, previousFrame = 0, frame = 0, movementThreshold = 0;
        unsigned actorType = 0, reaction = 0, animation = 0;
        bool eligible = false, pressed = false, cancel = false, contact = false;
    };

    struct LadderOutput {
        ladders::Vector displacement{};
        playerorientation::Quaternion rotation{0, 0, 0, 1};
        float movementScale = 1;
    };

    struct LadderServices {
        const ladders::Paths& paths;
        const actormodel::Catalog& catalog;
        const actormodel::Resources& models;
        const actormodel::Matrix* retainedMatrix = nullptr;
        const ladders::Vector* retainedDisplacement = nullptr;
    };

    class LadderInteraction {
    public:
        explicit LadderInteraction(const std::vector<std::uint8_t>& executable);
        bool select(playercomponents::LadderState& state, const LadderInput& input, const ladders::Paths& paths) const;
        bool run(playercomponents::LadderState& state, playeractions::Actions& actions, const LadderInput& input,
            LadderOutput& output, const LadderServices& services) const;

    private:
        void begin(playercomponents::LadderState& state, playeractions::Actions& actions, const LadderInput& input,
            LadderOutput& output, const LadderServices& services) const;
        int approach(const playercomponents::LadderState& state, const LadderInput& input, LadderOutput& output) const;
        bool climb(playercomponents::LadderState& state, playeractions::Actions& actions, const LadderInput& input,
            LadderOutput& output, const LadderServices& services) const;
        void animate(playercomponents::LadderState& state, const playeractions::Actions& actions, const LadderInput& input,
            LadderOutput& output, const LadderServices& services) const;
        std::array<float, 16> constants;
        playeractions::Actions fallback;
    };

    class LadderFrame {
    public:
        explicit LadderFrame(const std::vector<std::uint8_t>& executable);
        LadderInput input(const playeractor::Player& player) const;
        bool run(playeractor::Player& player, const LadderServices& services) const;

    private:
        LadderInteraction interaction;
        playeractions::Context actions;
        float approachSpeed, movementThreshold, turnBlend;
    };
}
