#pragma once

namespace playerevents { class Dispatcher; }

#include "game/world/doorresources.h"
#include "game/world/doordescriptors.h"
#include "game/events/runtimeflags.h"

#include <functional>
#include <optional>

namespace doors {
    using SoundEmitter = std::function<void(std::uint32_t, const std::array<float, 3>&)>;
    using SavedFlags = std::array<std::uint32_t, 8>;
    using ModelSubmitter = std::function<void(const ModelResource&, const PoseOwner&, unsigned, std::uint8_t, bool)>;

    struct CollisionBox {
        std::array<float, 3> center{};
        std::array<std::array<float, 3>, 3> axes{};
        std::array<float, 3> extents{};
    };

    struct RoomVisit {
        std::vector<std::uint32_t> doorIds;
        std::uint8_t marker = 0;
    };

    struct UpdateServices {
        SoundEmitter positionalSound;
        std::function<void(std::uint32_t)> globalSound;
        std::function<int(float, const std::array<float, 3>&)> playerInfluence;
        std::function<void(float, std::uint8_t, const std::array<float, 3>&)> influenceSound;
    };

    struct Leaf {
        std::uint8_t savedId = 0, type = 0;
        std::array<std::uint8_t, 8> references{};
        std::uint32_t flags = 0;
        std::array<float, 3> position{};
        std::array<std::int32_t, 3> angles{};
        std::array<std::uint8_t, 2> rooms{};
        dooranimation::Matrix transform{};
        std::uint16_t animation = 0;
        float frame = 0;
        PoseOwner pose;
        std::optional<CollisionBox> collision;
        std::uint8_t collisionActive = 1;
    };

    class State {
    public:
        bool initialize(const doordata::Table& descriptors, std::shared_ptr<const Resources> resources,
            const std::vector<std::uint8_t>& executable, const SavedFlags& savedFlags, std::string& error);
        unsigned size() const;
        const Leaf& leaf(unsigned index) const;
        std::uint32_t action(unsigned index, int direction, std::int8_t command, bool forced);
        void advanceAnimation(unsigned index, const SoundEmitter& emit);
        void refreshPose(unsigned index);
        bool blocked(std::uint32_t descriptorId) const;
        std::array<std::uint8_t, 2> savedPair(std::uint8_t id) const;
        void setSavedFlag(std::uint8_t id, bool enabled, SavedFlags& savedFlags);
        void setEventBlocked(std::uint8_t id, bool enabled);
        std::uint32_t markUsed(unsigned index, std::uint8_t& savedId, std::uint8_t& type);
        void setCollisionActive(std::uint8_t id, bool active);
        void updateVisibility(unsigned buffer, const std::vector<RoomVisit>& visits,
            const std::array<std::uint32_t, 256>& roomIds, std::uint32_t playerRoom);
        void updatePresentation(unsigned buffer, const std::vector<RoomVisit>& visits,
            const std::array<std::uint32_t, 256>& roomIds, std::uint32_t playerRoom,
            const std::array<float, 3>& playerPosition);
        void submitOpaque(unsigned buffer, bool overrideOpacity, const ModelSubmitter& submit) const;
        void submitTranslucent(unsigned buffer, bool overrideOpacity, const ModelSubmitter& submit) const;
        void update(unsigned map, std::uint32_t frameCounter, gameflags::State& flags,
            SavedFlags& savedFlags, const UpdateServices& services);
        int activeDoor() const;
        void setGlobalBarrier(bool enabled, const std::function<void(unsigned, unsigned, unsigned)>& notify);
        void setGlobalBarrier(bool enabled, const playerevents::Dispatcher& events);
        void resetGlobalBarrier();
        std::uint32_t eventBlocked(std::uint32_t index) const;
        bool globalBarrier() const;
        std::uint8_t barrierValue() const;

    private:
        std::shared_ptr<const Resources> resources;
        std::vector<Leaf> leaves;
        std::array<std::array<std::uint8_t, 2>, 256> descriptorPairs{}, savedPairs{};
        unsigned shakeFrame = 10;
        int active = -1;
        bool barrier = false;
        std::uint8_t barrierLevel = 0;
    };
}
