#pragma once

#include "game/player/playeractionreaction.h"

#include <array>
#include <cstdint>
#include <vector>

namespace playercomponents { class AttachedState; }
namespace actors { class MoveActor; }

namespace playeractions {
    class Actions {
    public:
        Actions(const std::vector<std::uint8_t>& executable, int actorType);
        void reset();
        bool query74544() const;
        bool query74588() const;
        bool query74968() const;
        bool suctionReaction(std::uint32_t reaction, std::uint32_t kind);
        void beginReaction(std::uint32_t reaction, const ReactionPayload& payload);
        bool request(unsigned message, const std::array<std::uint32_t, 12>* payload = nullptr);
        std::uint32_t reactionEntry() const;
        void updateNormalState();
        playercomponents::AttachedState* attached = nullptr;
        actors::MoveActor* actor = nullptr;
        std::array<std::uint32_t, 4> fields0;
        std::uint8_t field10, field11, field14;
        std::uint32_t field18, field1C;
        float field20;
        std::array<float, 2> fields24;
        float field2C;
        std::array<std::uint32_t, 2> fields30;
        std::uint8_t field38;
        std::array<std::uint8_t, 6> fields39;
        std::uint8_t field3F = 0;
        std::array<std::uint32_t, 4> fields40;
        float field50;
        std::array<std::uint32_t, 3> parameters;
        std::uint32_t field60, field64;
        std::array<std::uint32_t, 165> entries;
        std::uint8_t field2FC;
        std::uint32_t field300;
        std::array<std::uint8_t, 3> fields304;
        float field308;
        std::uint8_t field30C, field30D;
        std::uint32_t field310, field314;
        float field318;
        std::array<std::uint32_t, 14> fields31C;
        std::uint32_t field354, field358, field35C, field360;
    };

    class Reactions {
    public:
        Reactions(const std::vector<std::uint8_t>& executable, int actorType);
        Reactions(const Reactions&) = delete;
        Reactions& operator=(const Reactions&) = delete;
        std::uint32_t state = 1;
        float field4 = 0;
        std::uint32_t field8 = 0;
        std::array<float, 3> fieldC{};
        std::uint8_t field18 = 0;
        std::vector<std::array<std::uint32_t, 13>> records;
        std::array<const std::array<std::uint32_t, 13>*, 5> entries{};
    };
}
