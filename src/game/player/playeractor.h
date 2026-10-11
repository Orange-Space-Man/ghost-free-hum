#pragma once

#include "game/player/playeractorview.h"
#include "game/actors/characterowner.h"

namespace playeractor {
    struct Binding {
        std::int32_t index;
        playerrecord::Record* record;
        std::uint16_t flags;
        void* controller;
        void* field798;
        playerinput::ControllerState* field7A0;
        actors::MoveActor* first;
        actors::MoveActor* second;
        actors::MoveActor* third;
    };

    struct Construction {
        const std::vector<std::uint8_t>& executable;
        const actors::Construction& base;
        players::Manager& manager;
        players::Globals& sharedGlobals;
        gameplaycamera::CameraOwner& camera;
        Globals& globals;
        const ViewEnvironment& environment;
    };

    class Player : public actors::MoveActor {
    public:
        Player(const Construction& construction, unsigned id, unsigned type, unsigned capacity);
        ~Player() override;
        void refreshView();
        bool writeRecord();
        bool loadRecord(std::int32_t& audioLife);
        bool playFeedback(unsigned preset);
        void pulseFeedback(double amplitude, std::uint32_t duration, std::uint32_t fade);
        void bind(const Binding& binding, characterowner::Owner& owner, std::int32_t& audioLife);
        void setRotation(const std::array<std::int16_t, 3>& value) override;
        void contactEvent(const actors::Registry& registry, enemies::Manager& manager, std::array<float, 3>& position) override;
        State state;
        std::unique_ptr<Components> components;
        View view;

    private:
        const std::vector<std::uint8_t>& executable;
        const Globals& globals;
        const ViewEnvironment& environment;
    };

    class Alternate final : public Player {
    public:
        Alternate(const Construction& construction, unsigned id, unsigned type, unsigned capacity);
        void contactEvent(const actors::Registry& registry, enemies::Manager& manager, std::array<float, 3>& position) override;
        std::uint32_t field13A4 = 1, field13AC = 0;
        std::array<float, 3> field13B0{}, field13BC{0, 0, 1}, field13C8{0, 0, 1};
        std::array<float, 3> field13D4{};
        actoreffects::State effect;
    };
}
