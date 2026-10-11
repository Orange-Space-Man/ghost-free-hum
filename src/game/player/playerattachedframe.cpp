#include "resources/dolresources.h"
#include "game/player/playerattachedframe.h"
#include "game/startup/startupresources.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    void read(const std::vector<std::uint8_t>& executable, unsigned address, float* destination, unsigned count) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, count * 4, bytes, error)) throw std::runtime_error(error);
        for (unsigned i = 0; i < count; ++i) {
            const auto offset = i * 4;
            const std::uint32_t bits = (unsigned(bytes[offset]) << 24) | (unsigned(bytes[offset + 1]) << 16) |
                (unsigned(bytes[offset + 2]) << 8) | bytes[offset + 3];
            std::memcpy(destination + i, &bits, 4);
        }
    }
}

playercomponents::AttachedFrame::AttachedFrame(const std::vector<std::uint8_t>& executable) {
    read(executable, 0x804d2bac, constants.data(), 3);
    read(executable, 0x804db544, &scannerHorizontal, 1);
}

playercomponents::AttachedInput playercomponents::AttachedFrame::build(playeractor::Player& player) const {
    auto& state = player.state;
    const auto& actions = *player.components->actions;
    const auto& controls = *player.components->controls;
    const unsigned flags = controls.settings.flags;
    AttachedInput input;
    input.aim = actions.query74588() && (state.flags792 & 12);
    input.enabled = input.aim || (state.flags1140[2] && actions.field310 == 1);
    if (state.field10A0 > 0) {
        state.field10A0 -= 1;
        if (state.field10A0 <= 0) {
            state.field10A0 = 0;
            if ((flags & 0x40) && state.field1180 == 5) {
                state.field1070 = 5;
                state.field1074 = state.field10A4;
                state.field1074[1] = player.position[1];
                state.field109C = constants[0];
            }
        }
    }
    if (state.fieldFBC && !(state.field10A0 > 0)) {
        input.horizontal = state.fieldFBC->second[0] * state.fieldFBC->value;
        input.vertical = state.fieldFBC->second[2] * state.fieldFBC->value;
    }
    if (state.flags1140[2]) input.selection = state.field1180 == 0 ? 6 : 7;
    else if (state.flags134C[0]) input.selection = 5;
    else if (state.flags1040[0]) input.selection = 4;
    else input.selection = state.field1180 == 5 ? 8 : 0;

    if (state.field11A0 && !(state.field11A0->flags & 0x4000)) {
        input.mode = 7;
        input.horizontal = state.field11A0->horizontal;
        input.vertical = state.field11A0->vertical;
        if (state.field11A0->flags & 1) input.selection = 1;
        else if (state.field11A0->flags & 4) input.selection = 2;
    } else if (!actions.query74968() && !state.flags1140[2]) input.mode = 1;
    else {
        if (!controls.settings.fields[53]) input.mode = 2;
        else if (!controls.fieldC5) {
            if (!state.fieldFBC || state.fieldFBC->value == 0) {
                input.horizontal = input.vertical = 0;
                input.mode = 7;
            } else input.mode = 2;
        } else {
            input.horizontal = 0;
            input.vertical = 1;
            input.mode = 7;
        }
        if (state.flags10E8[1] && input.mode == 2) input.mode = !(flags & 0x80) || state.field1108 < 1 ? 4 : 5;
        else if (state.flags134C[0] || state.flags1140[2] || state.flags134C[2] || ((flags & 0x40) && state.field1180 == 5)) {
            if (input.mode == 2) input.mode = 3;
        }
        if (flags & 0x10) input.mode = 7;
        if ((flags & 8) && (!state.fieldFBC || state.fieldFBC->value == 0)) {
            input.horizontal = input.vertical = 0;
            input.mode = 8;
        }
        if (actions.field310 == 2 && (actions.field314 == 0x20 || actions.field314 == 4 || actions.field314 == 0xe)) input.mode = 1;
        if (state.field10A0 > 0) {
            if (!player.components->weapon->first) throw std::runtime_error("Attached aim requires its original weapon owner.");
            const auto& weapon = *player.components->weapon->first;
            input.mode = 10;
            input.sourceDirection = weapon.field610;
            for (unsigned i = 0; i < 3; ++i) input.targetDirection[i] = state.field10A4[i] - weapon.position[i];
            const auto& target = input.targetDirection;
            const float length = std::sqrt((target[0] * target[0] + target[1] * target[1]) + target[2] * target[2]);
            if (length >= 0.0001f) for (auto& component : input.targetDirection) component *= 1.0f / length;
            else input.targetDirection.fill(0);
        }
        if (state.field11A0 && (state.field11A0->flags & 0x4000)) input.verticalScale *= constants[1];
        if (state.flags1140[2]) {
            input.horizontalScale *= scannerHorizontal * constants[2];
            input.verticalScale *= constants[2];
        }
    }
    if (!input.selection && player.components->reactions->state == 3) input.selection = 3;
    input.frame = player.animationFrame;
    input.inverted = state.flags1330[1] != 0;
    return input;
}

playercomponents::AttachedOutput playercomponents::AttachedFrame::run(playeractor::Player& player,
    const actormodel::Catalog& catalog, const actormodel::Resources& resources, const actormodel::InitialPose& pose) const {
    const auto input = build(player);
    const auto output = player.components->attached->update(input, catalog, resources, pose);
    const auto morph = static_cast<std::int16_t>(output.morphs[0] == -1 ? output.morphs[1] : output.morphs[0]);
    player.fieldFA = morph;
    player.fieldF8 = morph == -1 ? static_cast<std::int16_t>(catalog.model(catalog.type(player.actorType).model).keyMetadata.at(player.keyIndex)[0]) : morph;
    player.selectTexture(static_cast<std::int16_t>(output.textures[0] == -1 ? output.textures[1] : output.textures[0]), 15);
    return output;
}
