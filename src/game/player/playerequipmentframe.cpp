#include "resources/dolresources.h"
#include "game/player/playerequipmentframe.h"
#include "game/startup/startupresources.h"

#include <cstring>
#include <stdexcept>

namespace {
    unsigned word(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }

    bool equipmentAllowed(const playeractions::Actions& actions) {
        for (unsigned state : {10U, 13U, 14U}) if (actions.fields31C[4] == state) return false;
        if (actions.field310 == 2) for (unsigned state : {22U, 4U, 5U, 6U, 7U, 12U, 25U, 26U, 23U, 24U, 28U, 34U})
            if (actions.field314 == state) return false;
        return true;
    }

    bool suctionAllowed(const playeractions::Actions& actions) {
        return actions.field310 == 1 || ((actions.field314 != 4 || actions.fields31C[6] == 3) &&
            actions.field314 != 12 && actions.field314 != 5);
    }

    void changeType(actors::MoveActor* actor, unsigned type) {
        if (actor && actor->actorType != type) {
            actor->initializeType(type);
            actor->selectAnimation(0);
        }
    }
}

playeractor::EquipmentFrame::EquipmentFrame(const std::vector<std::uint8_t>& executable) {
    const auto bits = word(executable, 0x804db57c);
    std::memcpy(&transitionRate, &bits, 4);
    elementalDelay = static_cast<std::int32_t>(word(executable, 0x804d2c38));
}

void playeractor::EquipmentFrame::run(Player& player,
    const std::function<void(unsigned, const std::array<float, 3>&)>& sound) const {
    auto& state = player.state;
    const auto& controls = *player.components->controls;
    const auto& actions = *player.components->actions;
    auto& attached = *player.components->attached;
    if ((state.flags792 & 12) && !(actions.field310 == 2 && (actions.field314 == 32 || actions.field314 == 33)) &&
        !state.flagsFC0[1]) {
        unsigned selection = 0;
        if (player.components->reactions->state != 4 && attached.field2C0 != 8 && attached.field2C0 != 6 &&
            static_cast<std::int16_t>(player.life) > 0 && !state.field1334 && !(state.field1008 > 0) &&
            !(state.field1130 > 0) && equipmentAllowed(actions)) {
            if ((state.flags792 & 8) && !(controls.field20 <= 0) && !controls.flags30[2] && suctionAllowed(actions) &&
                state.field1170 == 0 && !state.flags1140[2]) selection = 1;
            else if ((state.flags792 & 8) && !(controls.field2C <= 0) && state.field1170 == 0 && !state.flags1140[2])
                selection = state.field1184 == 1 ? 6 : state.field1184;
            else if ((state.flags792 & 4) && !state.flags1330[2] && state.field119D) selection = 5;
        }
        if (selection != state.field1180) {
            const bool alternate = player.actorType == 1 || player.actorType == 0xb3;
            if (selection == 5) changeType(state.field7A4, alternate ? 0xb7 : 7);
            else if (selection > 0 && selection < 7) changeType(state.field7A4, alternate ? 0xb6 : 6);
            if (selection && state.field1180) {
                attached.startFirstTransition(1, transitionRate);
                attached.startSecondTransition(1, transitionRate);
                if (selection - 2 < 3) state.field118C = float(elementalDelay);
            }
        }
        state.field1180 = selection;
    }
    if (state.flags10E8[1]) {
        bool contact = false;
        for (const auto* actor : state.fields10EC) if (actor &&
            (((actor->flags & 0x200) && actor->fieldB0 >= 0) || ((actor->flags & 0x400) && actor->field9C >= 0))) {
            contact = true;
            break;
        }
        if (state.field1180 != 1 || contact || state.flags1040[2] || state.field1170 != 0 || attached.field2C0 ||
            actions.field310 == 2) {
            for (auto*& actor : state.fields10EC) if (actor) {
                player.detachChild(*actor);
                actor = nullptr;
            }
            state.flags10E8[1] = 0;
            state.flags10E8[2] = !contact && !state.flags1040[2] && controls.field20 == 0;
            if (state.flags10E8[2]) sound(0x849, player.position);
        } else {
            state.field10FC = player.components->weapon->first->field610;
            state.flags10E8[2] = 0;
        }
    }
    if (state.field1180 == 5 && !state.flags1140[2]) {
        if (!controls.settings.fields[27]) {
            if (controls.flags24[2]) state.field119E = !state.field119E;
        } else state.field119E = !controls.flags24[3];
    }
}
