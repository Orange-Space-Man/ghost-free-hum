#pragma once

#include "game/actors/characterowner.h"
#include "game/events/missionresources.h"

namespace charactergenerator {
    struct Event {
        unsigned message;
        std::array<std::uint8_t, 381>* types = nullptr;
    };

    class Observer {
    public:
        virtual ~Observer() = default;
        virtual void receive(Event& event) = 0;
    };

    class Generator {
    public:
        Generator(const enemytypes::Catalog& types, enemies::Runtime& runtime, const mission::Resources& resources);
        void reset();
        std::int32_t parameter(std::string_view name) const;
        jmp::ToolData& path();
        characterowner::Owner* create(unsigned type, int strategy);
        void remove(characterowner::Owner& owner);
        jmp::ToolDataRef source;
        enemies::Manager* manager = nullptr;
        std::unique_ptr<enemystrategy::Strategy> strategy;
        std::array<characterowner::Owner*, 15> children{};
        std::int32_t count = 0;
        std::uint32_t total = 0;
        unsigned type;
        std::array<float, 3> position;
        std::uint16_t flags = 0;

    private:
        const enemytypes::Catalog& types;
        enemies::Runtime& runtime;
        const mission::Resources& resources;
        jmp::ToolData pathData;
    };
}
