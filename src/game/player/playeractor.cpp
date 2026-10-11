#include "game/player/playeractor.h"
#include "game/player/playercontrollerstate.h"
#include "game/player/playeractorstate.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

bool playeractor::Player::playFeedback(unsigned preset) {
    return state.field7A0 ? state.field7A0->play(executable, preset) : false;
}

void playeractor::Player::pulseFeedback(double amplitude, std::uint32_t duration, std::uint32_t fade) {
    if (state.field7A0) state.field7A0->feedback.pulses.play(amplitude, duration, fade);
}

namespace {
    float real(std::uint32_t value) {
        float result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    std::uint16_t readHalf(const playerrecord::Record& record, unsigned offset) {
        return static_cast<std::uint16_t>((record[offset] << 8) | record[offset + 1]);
    }

    std::uint32_t readWord(const playerrecord::Record& record, unsigned offset) {
        return (static_cast<std::uint32_t>(readHalf(record, offset)) << 16) | readHalf(record, offset + 2);
    }

    void bindBits(playercomponents::BitBuffer& buffer, playerrecord::Record& record, unsigned offset,
        unsigned count, unsigned bytes, unsigned width) {
        buffer.data = record.data() + offset;
        buffer.count = count;
        buffer.bytes = bytes;
        buffer.width = width;
    }
}

playeractor::Player::Player(const Construction& construction, unsigned id, unsigned type, unsigned capacity) :
    MoveActor(construction.base, id, type, capacity), state(*this, construction.globals, construction.base.pose),
    executable(construction.executable), globals(construction.globals), environment(construction.environment) {
    fieldE8 = 0;
    life = state.maximumLife;
    components = std::make_unique<Components>(construction.executable, *this, state, construction.manager,
        construction.sharedGlobals, construction.camera);
    refreshView();
}

playeractor::Player::~Player() {
    writeRecord();
    components.reset();
}

void playeractor::Player::refreshView() {
    view.refresh(*this, state, *components, globals, environment);
}

void playeractor::Player::setRotation(const std::array<std::int16_t, 3>& value) {
    MoveActor::setRotation(value);
    state.quaternion = playerorientation::fromMatrix(construction.pose.rotation(value));
    state.direction = playerorientation::direction(state.quaternion);
    components->quaternion->quaternion = state.quaternion;
}

bool playeractor::Player::writeRecord() {
    if (!state.record) return false;
    playerrecord::Values values;
    values.life = life;
    values.maximumLife = state.maximumLife;
    values.flags = state.flags792;
    values.entries = state.entries;
    values.count = state.entryCount;
    values.field12F4 = state.fields12F4[0];
    values.field12F8 = state.fields12F4[1];
    std::copy_n(state.fields12F4.begin() + 2, values.fields12FC.size(), values.fields12FC.begin());
    std::copy_n(state.fields12F4.begin() + 9, values.fields1318.size(), values.fields1318.begin());
    values.controlWord = static_cast<std::uint32_t>(components->controls->mode);
    return playerrecord::write(state.record, values);
}

bool playeractor::Player::loadRecord(std::int32_t& audioLife) {
    if (!state.record) return false;
    auto& record = *state.record;
    state.maximumLife = readHalf(record, 2);
    life = static_cast<std::uint16_t>(std::max(1, static_cast<int>(static_cast<std::int16_t>(readHalf(record, 0)))));
    audioLife = static_cast<std::int16_t>(life);
    state.flags792 = static_cast<std::uint16_t>((state.flags792 & ~0xc) | ((record[0x16a] & 3) << 2));
    for (unsigned i = 0; i < state.entries.size(); ++i) state.entries[i] = readHalf(record, 0x32 + i * 2);
    state.entryCount = readWord(record, 0xb4);
    state.fields12F4[0] = readWord(record, 0xb8);
    state.fields12F4[1] = readWord(record, 0xbc);
    for (unsigned i = 0; i < 7; ++i) state.fields12F4[i + 2] = record[0xc0 + i];
    for (unsigned i = 0; i < 3; ++i) state.fields12F4[i + 9] = readHalf(record, 0xc8 + i * 2);
    bindBits(*components->flags, record, 0x144, 0x100, 0x20, 1);
    bindBits(*components->bits[0], record, 0xd4, 0x180, 0x30, 1);
    bindBits(*components->bits[1], record, 0x104, 0x180, 0x30, 1);
    bindBits(*components->bits[2], record, 0x134, 0x40, 8, 1);
    bindBits(*components->bits[3], record, 0x13c, 0x40, 8, 1);
    bindBits(*components->bits[4], record, 0x164, 0x18, 6, 2);
    components->controls->setMode(static_cast<std::int32_t>(readWord(record, 0xd0)));
    return true;
}

void playeractor::Player::bind(const Binding& binding, characterowner::Owner& owner, std::int32_t& audioLife) {
    if (&owner.actor() != this) throw std::runtime_error("Player binding references a different character owner.");
    const auto& descriptor = environment.collisions.type(actorType);
    const auto& volume = field788 ? *field788 : descriptor;
    state.fieldF34 = binding.index;
    state.fieldF54 = real(descriptor.records.at(0)[1]);
    if (volume.field8 == 2) state.fieldF58 = real(volume.extended.value()[1]);
    else if (volume.field8 == 1) state.fieldF58 = real(volume.secondary.value()[1]);
    else state.fieldF58 = 200;
    state.flags792 = binding.flags;
    state.field794 = binding.controller;
    state.field798 = binding.field798;
    state.record = binding.record;
    state.field7A0 = binding.field7A0;
    state.field7A4 = binding.first;
    state.field7A8 = binding.second;
    state.field7AC = binding.third;
    if (binding.first) {
        const auto parameters = playerweapons::readParameters(executable, actorType, binding.first->actorType);
        attachChild(*binding.first, parameters[7], nullptr, 1);
    }
    if (binding.second) {
        const auto parameters = playerweapons::readParameters(executable, actorType, binding.second->actorType);
        attachChild(*binding.second, parameters[7], nullptr, 1);
    }
    if (binding.third) attachChild(*binding.third, 0x24, nullptr, 1);
    loadRecord(audioLife);
    state.field790 = 1;
    refreshView();
    owner.setLife(static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(life))));
}

playeractor::Alternate::Alternate(const Construction& construction, unsigned id, unsigned type, unsigned capacity) :
    Player(construction, id, type, capacity) {
    state.fieldF60 = 1;
    state.fieldF6C = 1.0f / state.fieldF60;
    state.field100C = 1;
    auto settings = components->controls->settings;
    settings.flags |= 4;
    components->controls->configure(&settings);
    if (type == 0xb3) {
        model->attachment(2).object = &effect;
        effect.flags |= 0x20;
        effect.field1C = {0, 0, 0};
        effect.field28 = {0.7f, 1, 1};
    }
}


playeractor::State::State(const actors::MoveActor& actor, const Globals& globals, const actormodel::InitialPose& pose) :
    fieldF30(actor.actorType), fieldFE4(actor.position), field1004(static_cast<float>(globals.field2C0C)),
    field132C(actor.fieldB4 >> 24) {
    quaternion = playerorientation::fromMatrix(pose.rotation(actor.rotation));
    direction = playerorientation::direction(quaternion);
    fieldFF0 = direction;
}
