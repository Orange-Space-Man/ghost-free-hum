#pragma once

#include "game/enemies/enemystrategy.h"
#include "game/actors/characterevent.h"
#include "game/events/missionresources.h"
#include "game/enemies/enemymanager.h"

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace enemystrategy {
    class Treasure final : public Strategy, public characterevent::Observer {
    public:
        Treasure(const mission::Resources& mission, enemies::Runtime& runtime,
            const std::vector<std::uint32_t>& roomFlags,
            std::function<void(unsigned, const std::array<float, 3>&)> sound,
            std::function<unsigned(std::string_view, const std::array<float, 3>&)> createItem,
            std::function<void(const std::array<float, 3>&)> revealCamera,
            std::function<bool()> automaticallyOpenKeyChests,
            std::function<void(unsigned)> markOpened);
        void initializeBehavior() override;
        void behavior() override;

        std::uint32_t playerTapped(characterevent::Event& event) override;

    private:
        const mission::Resources& mission;
        enemies::Runtime& runtime;
        const std::vector<std::uint32_t>& roomFlags;
        std::function<void(unsigned, const std::array<float, 3>&)> sound;
        std::function<unsigned(std::string_view, const std::array<float, 3>&)> createItem;
        std::function<void(const std::array<float, 3>&)> revealCamera;
        std::function<bool()> automaticallyOpenKeyChests;
        std::function<void(unsigned)> markOpened;
        std::uint32_t room = 0;
        std::string other;
        bool camera = false;
        bool delivered = false;
    };
}
