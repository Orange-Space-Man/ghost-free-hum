#pragma once

#include "game/player/playercontrolstate.h"
#include "game/player/playercontrollerstate.h"
#include "game/player/playerorientation.h"

namespace playerinput {
    struct Context {
        const Pad* pad = nullptr;
        actormodel::Matrix view{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        playerorientation::Quaternion orientation{0, 0, 0, 1};
        bool edges = true, suppress = false;
    };

    class Decoder {
    public:
        explicit Decoder(const std::vector<std::uint8_t>& executable);
        void update(playercontrols::State& state, const Context& context) const;
        float magnitude(float x, float y) const;
        unsigned direction(const playercontrols::Motion& motion) const;

    private:
        float threshold, epsilon, buttonMaximum, triggerMaximum, triggerThreshold, diagonalCorrection;
        playercontrols::Motion transform(float x, float y, const Context& context, unsigned mode) const;
        playercontrols::Motion motion(const Context& context, unsigned source, unsigned mode, unsigned inversion) const;
        bool button(const playercontrols::State& state, const Context& context, unsigned selector, bool edge) const;
        float analog(const playercontrols::State& state, const Pad& pad, unsigned selector) const;
    };
}
