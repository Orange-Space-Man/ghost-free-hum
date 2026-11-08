#pragma once

#include "game/actors/charactergenerator.h"
#include "game/actors/actorinitialpose.h"

namespace enemystrategy {
    class RatSpawn {
    public:
        virtual ~RatSpawn() = default;
        virtual void setRatPath(const std::array<float, 3>& start, const std::array<float, 3>& end) = 0;
    };

    class RatGenerator final : public Strategy, public charactergenerator::Observer {
    public:
        RatGenerator(enemies::Runtime& runtime, const actormodel::InitialPose& pose);
        void initializeBehavior() override;
        void behavior() override;
        void receive(charactergenerator::Event& event) override;

    private:
        struct Holes {
            characterowner::Owner* child = nullptr;
            int start = -1, end = -1;
        };
        charactergenerator::Generator& generator() const;
        void release(characterowner::Owner* child);
        void recycle();
        bool place(characterowner::Owner& child);
        std::array<float, 3> point(int index);
        std::array<Holes, 8> holes{};
        enemies::Runtime& runtime;
        const actormodel::InitialPose& pose;
        int pointCount = 0, pairCount = 0, appearanceFrame = 0;
    };
}
