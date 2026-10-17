#pragma once

#include "game/actors/actoranimationsound.h"

#include <vector>
#include <mutex>

namespace jaudio {
    class PositionalSound : public actoranimation::SoundHandle {
    public:
        virtual std::uint32_t metadataFlags() const { return flags(); }
        virtual void clearBinding() { if (binding) *binding = nullptr; binding = nullptr; }
        virtual void retainPosition(const std::array<float, 3>& retained) { position = &retained; ownerPosition = nullptr; }
        const std::array<float, 3>* ownerPosition = nullptr;
        const std::array<float, 3>* position = nullptr;
        PositionalSound* next = nullptr;
        PositionalSound** binding = nullptr;
    };

    struct RetainedPosition {
        RetainedPosition* previous = nullptr;
        RetainedPosition* next = nullptr;
        PositionalSound* sound = nullptr;
        std::uint32_t remaining = 0;
        std::array<float, 3> position{};
    };

    struct PositionOwners {
        void release(const std::array<float, 3>* position);
        void update();
        std::vector<PositionalSound*> categories;
        RetainedPosition* free = nullptr;
        RetainedPosition* active = nullptr;
        std::recursive_mutex mutex;

    private:
        RetainedPosition* allocate();
        void recycle(RetainedPosition& point);
    };
}
