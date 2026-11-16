#pragma once

#include "game/enemies/enemyghostparentselection.h"
#include "game/enemies/enemyghostcaptureparent.h"
#include "game/enemies/enemyghostdisappearance.h"
#include <memory>

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
        std::function<Children(const Selection&)> assemble;
        std::function<bool(unsigned)> roomCleared;
        std::function<void(std::uint32_t)> elementEffect;
        std::function<std::unique_ptr<enemystrategy::Strategy>()> createCapture;
        std::function<void(characterevent::Event&)> notifyAttackFishing;
        std::function<void()> finishSecondary;
        std::function<void()> clearElement;
        std::function<void()> lightResult101;
        std::function<void()> secondResult101;
    };

    class Parent final : public enemystrategy::GhostCaptureParent {
    public:
        explicit Parent(ParentServices services);
        bool hitAdmissible() const;
        void setLightDelegate(enemystrategy::Strategy* delegate);

        Fields fields;
        Children children;
        Selection selection;
        enemystrategy::Strategy* lightDelegate = nullptr;
        enemystrategy::Strategy* secondDelegate = nullptr;

    protected:
        std::unique_ptr<enemystrategy::Strategy> createCapture() override;
        void notifyAttackFishing(characterevent::Event& event) override;
        void initializeGhostState() override;
        void updateGhostState() override;
        void updateKnockback() override;
        enemystrategy::GhostDisappearance& disappearance() override;
        void finishSecondary() override;
        void clearElement() override;

    private:
        ParentServices services;
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
