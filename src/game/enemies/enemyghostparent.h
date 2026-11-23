#pragma once

#include "game/enemies/enemyghostparentselection.h"
#include "game/enemies/enemyghostcaptureparent.h"
#include "game/enemies/enemyghostdisappearance.h"
#include <memory>
#include <optional>

namespace ghostparent {
    struct Children {
        enemystrategy::Strategy* movement = nullptr;
        enemystrategy::Strategy* search = nullptr;
        enemystrategy::Strategy* appearance = nullptr;
        enemystrategy::Strategy* condition = nullptr;
        enemystrategy::Strategy* attack = nullptr;
    };

    struct ParentServices {
        actors::MoveActor& actor;
        characterowner::State& character;
        enemies::Manager& manager;
        const gameparameters::Parameters& parameters;
        const jmp::ToolDataRef& source;
        const actormodel::InitialPose& pose;
        enemystrategy::GhostDisappearance& disappearance;
        std::function<std::uint32_t()> random;
        std::function<void(int, std::int32_t&)> place;
        std::function<Children(const Selection&)> assemble;
        std::function<bool(unsigned)> roomCleared;
        std::function<void(std::uint32_t)> elementEffect;
        std::function<std::unique_ptr<enemystrategy::Strategy>()> createCapture;
        std::function<void(characterevent::Event&)> notifyAttackFishing;
        std::function<void()> finishSecondary;
        std::function<void()> clearElement;
        std::function<void()> lightResult101;
        std::function<void()> secondResult101;
        std::function<void(unsigned)> damagePlayer;
        std::function<enemystrategy::Strategy*()> light;
    };

    class Parent : public enemystrategy::GhostCaptureParent {
    public:
        explicit Parent(ParentServices services);
        bool hitAdmissible() const;
        std::uint32_t enteredFlashlight(characterevent::Event& event) override;
        std::uint32_t collideWithPlayer(characterevent::Event& event) override;
        std::uint32_t playerLeftRoom(characterevent::Event& event) override;
        std::uint32_t playerEnteredRoom(characterevent::Event& event) override;
        std::uint32_t replacement(characterevent::Event& event) override;
        std::uint32_t sprayedWithFire(characterevent::Event& event) override;
        std::uint32_t sprayedWithIce(characterevent::Event& event) override;
        std::uint32_t sprayedWithWater(characterevent::Event& event) override;
        void setLightDelegate(enemystrategy::Strategy* delegate);

        Fields fields;
        Children children;
        Selection selection;
        enemystrategy::Strategy* lightDelegate = nullptr;
        enemystrategy::Strategy* secondDelegate = nullptr;

    protected:
        Parent();
        void bind(ParentServices services);
        std::unique_ptr<enemystrategy::Strategy> createCapture() override;
        void notifyAttackFishing(characterevent::Event& event) override;
        void initializeGhostState() override;
        void updateGhostState() override;
        void updateKnockback() override;
        enemystrategy::GhostDisappearance& disappearance() override;
        void finishSecondary() override;
        void clearElement() override;

    private:
        std::optional<ParentServices> bound;
        ParentServices& services() const;
        void startLight();
        void pushAway();
        bool pumpLight();
        bool pumpSecond();
        enemystrategy::Strategy& child(enemystrategy::Strategy* value, const char* name) const;
        void faceFirstPlayer();
        void enterInitialize();
        void enterWait();
        void enterMovement();
        void updateMovement();
        void enterSearch();
        void updateSearch();
        void enterAppearance();
        void updateAppearance();
        void enterAttack();
        void updateAttack();
        void enterKnockback();
        void updateKnockbackState();
        void enterAfterAttack(Parameter count, Parameter action, const char* animation);
        void updateAfterAttack(Parameter action);
    };
}
