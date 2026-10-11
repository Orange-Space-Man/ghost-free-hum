#include "game/player/playeractorview.h"
#include "game/world/dooranimationpose.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
    std::int32_t integer(double value) {
        if (!std::isfinite(value) || value < -2147483648.0 || value >= 2147483648.0)
            return std::numeric_limits<std::int32_t>::min();
        return static_cast<std::int32_t>(value);
    }

    std::uint32_t unsignedInteger(double value) {
        if (!(0 <= value)) return 0;
        if (value >= 4294967296.0) return 0xffffffff;
        return static_cast<std::uint32_t>(value);
    }

    float magnitude(const std::array<float, 3>& value) {
        return std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
    }

    bool bit(const playercomponents::BitBuffer& buffer, unsigned index) {
        return buffer.data && (buffer.data[index >> 3] & (1U << (index & 7))) != 0;
    }

    const std::array<float, 3>* reactionPosition(const playeractions::Reactions& reaction,
        const std::array<actors::MoveActor*, 128>& actors) {
        if (reaction.state == 1) return nullptr;
        if (reaction.field18) return &reaction.fieldC;
        if (static_cast<std::int32_t>(reaction.field8) < 0) return nullptr;
        const auto* actor = actors.at(reaction.field8);
        if (!actor) throw std::runtime_error("Player reaction references an unconstructed actor.");
        return actor->flags & 0xc000 ? nullptr : &actor->position;
    }
}

void playeractor::View::refresh(actors::MoveActor& actor, State& state, Components& components,
    const Globals& globals, const ViewEnvironment& environment) {
    position = state.flags1040[2] ? &state.field1048 : &actor.position;
    velocity = &actor.velocity;
    magnitude = ::magnitude(actor.velocity);
    quaternion = &state.quaternion;
    direction = &state.direction;
    angle = actor.rotation[1];
    id = actor.actorId;
    type = actor.actorType;
    room = previousRoom = actor.fieldB4;
    volume = actor.field788 ? actor.field788 : &environment.collisions.type(type);
    field83C = state.field7B4;
    field834 = state.flags1330[1];
    controls = components.controls.get();
    field840 = &state.fieldFC4;
    field844 = &state.fieldFD0;
    field848 = state.fieldFDC;
    field84C = state.fieldFE0;
    life = actor.life;
    maximumLife = state.maximumLife;
    field858 = static_cast<std::int16_t>(integer(state.field1000));
    field85A = 0;
    if (state.field1184 == 1) {
        const float maximum = static_cast<float>(globals.field2C0C);
        field85E = state.field1008 <= 0 ? static_cast<std::int16_t>(integer(10.0f *
            ((state.field1004 - maximum) / (0.0f - maximum)))) : 10;
    } else field85E = static_cast<std::int16_t>(integer(0.9999f + 0.1f * state.field1188));
    field8C0 = 0;
    field860 = globals.field2C14 == integer(state.field1008);
    if (state.field7A8) {
        const auto& held = *state.field7A8;
        state.matrix1010 = held.model ? dooranimation::concatenate(held.matrix,
            held.model->matrix(0, components.weapon->second->parameters[18])) : held.matrix;
    } else state.matrix1010 = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    field8C4 = &state.matrix1010;
    field85C = 0;
    field861 = (state.flags792 & 8) != 0;
    field862 = state.flags1040[2];
    field863 = state.field1058;
    field864 = state.field1184;
    flags = components.flags.get();
    for (unsigned i = 0; i < bits.size(); ++i) bits[i] = components.bits[i].get();
    field880 = 0;
    for (unsigned i = 0; i < 60; ++i) field880 += bit(*bits[2], i);
    const auto& actions = *components.actions;
    field884 = actions.field310 == 2 && (actions.field314 == 0x22 ||
        (actions.field314 == 0xc && (actions.field35C == 2 || actions.field35C == 4)));
    field886 = state.flagsFC0[3];
    field885 = state.flags1040[1];
    const auto& reactions = *components.reactions;
    field888 = reactions.state >= 2 && reactions.state < 5 ? reactions.state - 1 : 0;
    field88C = unsignedInteger(reactions.field4);
    field890 = reactionPosition(reactions, environment.actors);
    field894 = 0;
    field895 = state.flags1140[2];
    field898 = &state.field1144;
    field89C = 0 < state.field110C;
    field89D = 0 < state.field1114;
    field89E = actions.query74968() && !state.flags10E8[1] && !(state.field1180 == 5 && !state.field119E);
    field8A0 = field89E ? state.field1180 : 0;
    field8A4 = &components.weapon->first->position;
    field8A8 = &components.weapon->first->field610;
    field8AC = actions.query74588() ? components.attached->field240 : 0.5f;
    field8B0 = components.controls->field20;
    field8B4 = state.field11A0 ? state.field11A0->model :
        (state.field1180 == 1 && actions.query74544() ? components.vacuum->model.get() : nullptr);
    field8B8 = nullptr;
    if (state.field1180 == 5) {
        const bool suppressed = !(environment.mapMode == 2 && environment.flag3D) &&
            (environment.roomFlags.at(actor.fieldB4 >> 24) & 0x8000) != 0;
        if (!suppressed) field8B8 = components.light.get();
    }
    field8C8 = actions.query74968() && !state.field11A0;
    field8C9 = state.field11A0 != nullptr;
    field8CA = state.field11A0 && (state.field11A0->flags & 0x10) != 0;
    field8CC = state.field11A0 ? &state.field11A4 : nullptr;
    field8D0 = state.field11A0 ? (*state.field11A0->actor)->life : 0xffff;
    field8D4 = state.field11A0 ? &state.fields11B0 : nullptr;
    fieldF15 = state.field106C != 0;
    fieldF16 = state.field1070 > 3;
    fieldF17 = state.field1170 != 0;
    fieldF18 = &state.fieldFE4;
    fieldF1C = &state.fieldFF0;
    fieldF20 = globals.field2BE4;
    fieldF24 = state.field1118;
    fieldF28 = state.field1118 * globals.field2BF4;
    counts.fill(0);
    for (unsigned i = 0; i < state.entryCount; ++i) ++counts.at(state.entries.at(i));
    count = state.entryCount;
    fieldED0 = 0;
    fieldsED4 = state.fields12F4;
    constexpr unsigned flagIndices[]{188, 192, 189, 190, 191};
    for (unsigned i = 0; i < flagsF04.size(); ++i) flagsF04[i] = bit(*bits[0], flagIndices[i]);
    fieldF09 = state.flags10E8[1];
    fieldF0A = state.flags10E8[2];
    fieldF0C = &state.field10FC;
    fieldF10 = state.flags134C[2];
    fieldF11 = 0;
    if (state.flags134C[2]) {
        std::array<float, 3> difference;
        for (unsigned i = 0; i < difference.size(); ++i) difference[i] = actor.position[i] - state.field1350[i];
        fieldF11 = 0.8f * state.field1360 + 0.85f * state.field1364 < ::magnitude(difference);
    }
    constexpr unsigned otherIndices[]{117, 119, 118};
    for (unsigned i = 0; i < flagsF12.size(); ++i) flagsF12[i] = bit(*bits[0], otherIndices[i]);
    fieldF2C = state.flags1330[0];
    fieldF2D = state.field1130 > 0;
    fieldF2E = state.flags1140[0];
}
