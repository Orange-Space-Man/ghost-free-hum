#pragma once

#include "roomfurnitureresponse.h"
#include "game/actors/actorinitialpose.h"

namespace roomfurniture {
    struct SwingState {
        std::array<float, 2> target{}, previousVelocity{};
        float collected = 0;
    };

    struct SwingServices {
        std::function<float(float, const Vector&, Vector&)> suction;
        std::function<void(unsigned, const Vector&, const std::array<float, 2>&)> sound;
    };

    struct SwingSound {
        bool stop = false, changePitch = false;
        float pitch = 0, volume = 0;
    };

    struct AnimatedSwingState {
        SwingState swing;
        float mode = 0, delay = 0, proximity = 0, hit = 0;
    };

    struct AnimatedSwingServices {
        Vector playerPosition;
        bool enabled = false, scanner = false;
        std::function<bool()> messageBusy;
        std::function<float()> random;
        std::function<void(bool)> camera;
        std::function<void(unsigned)> playerAction;
        std::function<void(const Vector&, unsigned, unsigned)> playerDamage;
        std::function<Vector(const Object&, const Vector&)> jointPoint;
        std::function<void(const Vector&, const std::array<float, 2>&, const Vector&)> actorReaction;
        std::function<unsigned(const Object&)> animationEnd;
        std::function<void(unsigned)> globalSound;
    };

    class Swing {
    public:
        explicit Swing(const std::vector<std::uint8_t>& executable);
        SwingSound sound(const std::array<float, 2>& velocity, float currentPitch) const;
        void update(Object& object, RowState& row, SwingState& state, bool visible, bool vacuumActive,
            const Response& response, const ResponseServices& responses, const SwingServices& services) const;
        void updateAnimated(Object& object, RowState& row, AnimatedSwingState& state, bool visible, bool vacuumActive,
            const Response& response, const ResponseServices& responses, const SwingServices& services,
            const AnimatedSwingServices& animation) const;

    private:
        actormodel::InitialPose pose;
        std::array<float, 4> spring;
        std::array<unsigned, 26> sounds;
        float quantization, inverseQuantization, strengthScale, directionScale, radius, zero, one;
        float rootThreshold;
        double half, three;
        float soundMaximum, soundMinimum, pitchScale, pitchThreshold, volumeScale;
        float activeMode, delayLimit, reactionFrame, cameraReleaseFrame, effectOffset, nearDistance, farDistance;
        double randomThreshold;
        Vector jointOffset, reactionDirection;
        std::array<float, 2> reactionParameters;
        void animate(Object& object, AnimatedSwingState& state, const ResponseServices& responses,
            const AnimatedSwingServices& services) const;
        std::array<float, 2> angles(const Vector& direction) const;
        void approach(float target, float stiffness, float damping, float& position, float& velocity) const;
    };
}
