#pragma once

#include "game/player/playerscannermodel.h"
#include "game/actors/actormodelinstance.h"

#include <optional>
#include <vector>

namespace playerscanner {
    using Vector = std::array<float, 3>;

    struct FurnitureTarget {
        actormodel::Matrix inverse;
        Vector extent;
        bool enabled;
    };

    struct ActorTarget {
        Vector position;
        float radius;
        int owner;
        bool active, scannerType, hasOwner, eligible, visible;
    };

    struct Hit {
        int index = -1;
        float distance = 0;
    };

    bool intersect(const FurnitureTarget& target, const Vector& position, const Vector& direction, float& distance);
    Hit furnitureTarget(const std::vector<FurnitureTarget>& targets, const Vector& position, const Vector& direction, float maximum);
    Hit actorTarget(const std::vector<ActorTarget>& targets, const Vector& position, const Vector& direction, float maximum);

    class Services {
    public:
        virtual ~Services() = default;
        virtual std::int16_t delay() const = 0;
        virtual void playScanSound(unsigned sound) = 0;
        virtual Hit furniture(const Vector& position, const Vector& direction) = 0;
        virtual Hit actor(const Vector& position, const Vector& direction) = 0;
        virtual unsigned actorType(int actor) = 0;
        virtual std::optional<unsigned> actorMessage(int actor) = 0;
        virtual void setDiscoveryFlag(unsigned flag) = 0;
        virtual void selectMessage(unsigned player, unsigned message) = 0;
        virtual void openMessage() = 0;
        virtual void markFurniture(int index) = 0;
        virtual bool mirror(int index) = 0;
        virtual bool furnitureMessage(int index) = 0;
        virtual void openFurnitureMessage() = 0;
    };

    struct Input {
        bool pressed = false, cancel = false;
        gameplaycamera::CameraOwner& camera;
    };

    bool update(playermodels::ScannerModels& state, const Input& input, bool& warp, Services& services);
}
