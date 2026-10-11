#pragma once

#include "game/actors/characterowner.h"

namespace characteritems { class Creation; }

namespace characterowner {
    class CaptureServices {
    public:
        virtual ~CaptureServices() = default;
        virtual void initializeMotion(Owner& owner, std::uint16_t type) = 0;
        virtual void dropItem(Owner& owner) = 0;
        virtual void notifyPlayer(Owner& owner) = 0;
    };

    std::uint32_t dispatchEvent(Owner& owner, characterevent::Event& event, int group, CaptureServices* capture,
        characteritems::Creation* items = nullptr);
}
