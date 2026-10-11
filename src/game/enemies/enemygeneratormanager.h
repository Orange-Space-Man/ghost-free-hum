#pragma once

#include "game/actors/charactergenerator.h"
#include "game/enemies/enemystrategyfactory.h"

namespace charactergenerator {
    class Manager final : public jmp::MessageReceiver {
    public:
        Manager(const enemytypes::Catalog& types, const enemycreation::Catalog& actors,
            const enemystrategy::Catalog& strategies, const enemystrategy::Factory& factory,
            enemies::Manager& enemies, enemies::Runtime& runtime, const mission::Resources& resources);
        ~Manager();
        bool create(jmp::ToolDataRef& entry) override;
        bool remove(jmp::ToolDataRef& entry) override;
        bool message(jmp::ToolDataRef& entry, const char* command) override;
        void update();
        std::array<std::uint8_t, 262> requiredModels();
        std::array<std::unique_ptr<Generator>, 20> generators;
        bool enabled = true;

    private:
        const enemytypes::Catalog& types;
        const enemycreation::Catalog& actors;
        const enemystrategy::Catalog& strategies;
        const enemystrategy::Factory& factory;
        enemies::Manager& enemies;
        enemies::Runtime& runtime;
        Generator* find(const jmp::ToolDataRef& entry);
    };
}
