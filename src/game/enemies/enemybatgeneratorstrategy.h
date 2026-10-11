#pragma once

#include "game/actors/charactergenerator.h"
#include "game/world/roomlocation.h"

namespace enemystrategy {
    class BatSpawn {
    public:
        virtual ~BatSpawn() = default;
        virtual bool initializeBat(bool second, float radius, const std::array<float, 3>& origin, unsigned parameter) = 0;
    };

    class BatGenerator final : public Strategy {
    public:
        BatGenerator(enemies::Runtime& runtime, const roomlocation::Rooms& rooms, bool second);
        void initializeBehavior() override;
        void behavior() override;
        std::array<float, 3> position{}, minimum{}, maximum{};
        float radius = 0;
        std::uint32_t parameter = 0;
        std::int32_t initialDelay = 0;

    private:
        charactergenerator::Generator& generator() const;
        enemies::Runtime& runtime;
        const roomlocation::Rooms& rooms;
        bool second;
    };
}
