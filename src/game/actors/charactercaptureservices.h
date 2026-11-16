#pragma once

#include "game/actors/characterownerevents.h"
#include "game/actors/actorcapturemotion.h"
#include "game/player/playereventdispatch.h"

namespace characterowner {
    class NativeCaptureServices final : public CaptureServices {
    public:
        NativeCaptureServices(actorcapture::Motion& motion, playerevents::Dispatcher& players,
            characteritems::Creation& items);
        void initializeMotion(Owner& owner, std::uint16_t type) override;
        void dropItem(Owner& owner) override;
        void notifyPlayer(Owner& owner) override;

    private:
        actorcapture::Motion& motion;
        playerevents::Dispatcher& players;
        characteritems::Creation& items;
    };
}
