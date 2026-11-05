#pragma once

#include "game/player/playerenvironmentframe.h"
#include "game/player/playerdoorframe.h"
#include "game/player/playerladderinteraction.h"
#include "game/player/playeritempresentation.h"
#include "game/player/playerscannerinteraction.h"
#include "game/player/playerenergyframe.h"
#include "game/player/playerequipmentframe.h"
#include "game/player/playercontrolframe.h"
#include "game/player/playertimerframe.h"
#include "game/player/playercontactreactions.h"
#include "game/player/playercaptureframe.h"

namespace playeractor {
    struct MainFrameServices {
        playerenvironment::Services environment;
        playerinteraction::DoorFrameServices door;
        playerinteraction::LadderServices ladder;
        playerpresentation::Services& presentation;
        playerscanner::Services& scanner;
        playerevents::Dispatcher& events;
        TimerServices timers;
        std::uint8_t& specialActive;
        std::function<void()> owner, input, furniture, motion;
        std::function<void(Player&)> beginWarp, warp, elemental, elementalShot;
        std::function<bool(Player&)> vacuum;
        std::function<void(Player&, std::uint8_t)> captureReaction;
        std::function<void(unsigned, const std::array<float, 3>&)> sound;
    };

    class MainFrame {
    public:
        explicit MainFrame(const std::vector<std::uint8_t>& executable);
        void run(Player& player, MainFrameServices& services) const;

    private:
        playerenvironment::Frame environment;
        CaptureFrame capture;
        playerinteraction::DoorFrame door;
        playerinteraction::LadderFrame ladder;
        playerpresentation::Frame presentation;
        EquipmentFrame equipment;
        EnergyFrame energy;
        TimerFrame timers;
        playercontrols::Frame controls;
        playeractions::ContactFrame contact;
    };
}
