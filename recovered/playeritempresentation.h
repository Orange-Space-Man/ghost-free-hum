#pragma once

#include "game/player/playeractor.h"

namespace playerpresentation {
    class Services {
    public:
        virtual ~Services() = default;
        virtual void effect(unsigned effect, const std::array<float, 3>& position) = 0;
        virtual bool specialItem(unsigned type) const = 0;
        virtual void music(unsigned sequence) = 0;
        virtual void collected(actors::MoveActor& item) = 0;
        virtual void setFlag(unsigned flag) = 0;
        virtual bool equipmentOwned() const = 0;
        virtual void equipmentEvent(unsigned player, unsigned event, unsigned item) = 0;
        virtual void sound(unsigned sound, const std::array<float, 3>& position) = 0;
        virtual void scene(unsigned scene, std::int32_t argument) = 0;
    };

    class Frame {
    public:
        explicit Frame(const std::vector<std::uint8_t>& executable);
        bool run(playeractor::Player& player, Services& services) const;

    private:
        actormodel::Matrix transform(unsigned actorType) const;
        std::array<float, 10> constants;
        std::array<float, 6> placement;
        float turnBlend;
        actormodel::InitialPose poses;
    };
}
