#include "game/player/playervacuumframe.h"
#include "resources/dolresources.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    template<class T> T constant(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, sizeof(T), bytes, error)) throw std::runtime_error(error);
        std::reverse(bytes.begin(), bytes.end());
        T result;
        std::memcpy(&result, bytes.data(), sizeof(result));
        return result;
    }

    constexpr unsigned idleRow = 0x80355154, modeRows[4] = {0x8035516c, 0x80355178, 0x80355184, 0x80355190};
    constexpr float tintHold = 0.8f, tintTake = 0.2f; // 804DAB68 / 804DAB6C (0.19999999)
}

vacuumcapture::PlayerFrame::PlayerFrame(const std::vector<std::uint8_t>& executable) {
    std::string error;
    if (!trigonometry_.read(executable, error)) throw std::runtime_error(error);
    one_ = constant<float>(executable, 0x804db518);
    half_ = constant<float>(executable, 0x804db528);
    minusOne_ = constant<float>(executable, 0x804db534);
    units_ = constant<float>(executable, 0x804db5e4);
    five_ = constant<float>(executable, 0x804db5e8);
    hundredEighty_ = constant<float>(executable, 0x804db5ec);
    threeSixty_ = constant<float>(executable, 0x804db5f0);
    tintHold_ = constant<float>(executable, 0x804dab68);
    tintTake_ = constant<float>(executable, 0x804dab6c);
    nozzleLift_ = constant<float>(executable, 0x804dab64);
    windowA_ = constant<std::int32_t>(executable, 0x804d2c70);
    windowB_ = constant<std::int32_t>(executable, 0x804d2c64);
    windowC_ = constant<std::int32_t>(executable, 0x804d2c60);
    alpha_ = static_cast<std::uint8_t>(constant<std::uint32_t>(executable, 0x804dab60)); // (char)DAT_804DAB60 = low byte of the word
    const auto row = [&](unsigned address) {
        std::vector<std::uint8_t> bytes;
        if (!dolresources::read(executable, address, 12, bytes, error)) throw std::runtime_error(error);
        Tint tint;
        for (unsigned i = 0; i < 3; ++i) tint.color[i] = bytes[i];
        tint.first = static_cast<std::int32_t>((unsigned(bytes[4]) << 24) | (unsigned(bytes[5]) << 16) | (unsigned(bytes[6]) << 8) | bytes[7]);
        tint.second = static_cast<std::int32_t>((unsigned(bytes[8]) << 24) | (unsigned(bytes[9]) << 16) | (unsigned(bytes[10]) << 8) | bytes[11]);
        return tint;
    };
    tints_[0] = row(idleRow);
    for (unsigned i = 0; i < 4; ++i) tints_[i + 1] = row(modeRows[i]);
}

float vacuumcapture::PlayerFrame::reachScale(const playeractor::State& state) const {
    // 800B99D8.
    double result = state.fieldF60;
    if (0.0f < state.field105C)
        result = static_cast<float>(result * static_cast<double>(one_ - state.field105C * (one_ / static_cast<float>(windowA_))));
    const auto angle = [&](std::int32_t window) {
        const float scaled = (((units_ * five_) * hundredEighty_) * state.field1174 * (one_ / static_cast<float>(window))) / threeSixty_;
        return trigonometry_.cosineAngle(static_cast<std::uint32_t>(static_cast<std::int32_t>(scaled)));
    };
    if (0.0f == state.field1170) {
        if (0.0f < state.field1174) {
            const float cosine = angle(windowB_);
            result = static_cast<float>(result * static_cast<double>(state.field1178 +
                (cosine - minusOne_) * half_ * (one_ - state.field1178)));
        }
    } else if (state.field1174 <= 0.0f) {
        result = static_cast<float>(result * static_cast<double>(state.field1178));
    } else {
        const float cosine = angle(windowC_);
        result = static_cast<float>(result * static_cast<double>(one_ +
            (cosine - minusOne_) * half_ * (state.field1178 - one_)));
    }
    return static_cast<float>(result);
}

void vacuumcapture::PlayerFrame::applyTint(Vacuum& vacuum, unsigned mode, const playermodels::VacuumVolume& volume,
    const PlayerFrameServices& services, const playermodels::VacuumModel* model) const {
    // 8008427C: the nozzle light eases towards the colour row of the current mode.
    if (!vacuum.started) return;
    if (!model) throw std::runtime_error("Original vacuum nozzle light requires a beam model.");
    (void)volume;
    const auto& point = model->points[0].position;
    const Vector position{point[0], point[1] + nozzleLift_, point[2]};
    const unsigned row = mode >= 1 && mode <= 4 ? mode : 0;
    const auto& tint = tints_[row];
    if (row == 0) {
        for (unsigned i = 0; i < 3; ++i) vacuum.tint[i] = static_cast<float>(tint.color[i]);
        vacuum.tint[3] = static_cast<float>(tint.first);
        vacuum.tint[4] = static_cast<float>(tint.second);
    }
    for (unsigned i = 0; i < 3; ++i)
        vacuum.tint[i] = vacuum.tint[i] * tintHold_ + static_cast<float>(tint.color[i]) * tintTake_;
    vacuum.tint[3] = vacuum.tint[3] * tintHold_ + static_cast<float>(tint.first) * tintTake_;
    vacuum.tint[4] = vacuum.tint[4] * tintHold_ + static_cast<float>(tint.second) * tintTake_;
    const std::array<std::uint8_t, 4> color{static_cast<std::uint8_t>(static_cast<std::int32_t>(vacuum.tint[0])),
        static_cast<std::uint8_t>(static_cast<std::int32_t>(vacuum.tint[1])), static_cast<std::uint8_t>(static_cast<std::int32_t>(vacuum.tint[2])), alpha_};
    services.beamLight(position, color, static_cast<std::int32_t>(vacuum.tint[3]), static_cast<std::int32_t>(vacuum.tint[4]));
}

bool vacuumcapture::PlayerFrame::run(playeractor::Player& player, PlayerFrameServices& services) const {
    if (!services.rumble || !services.loopSound || !services.beamLight)
        throw std::runtime_error("Original player vacuum frame requires its rumble, sound and light services.");
    auto& state = player.state;
    auto& components = *player.components;
    auto& controls = *components.controls;
    auto& vacuum = services.vacuum;
    bool updated = false;
    if (state.field1180 == 1) {
        unsigned mode = 0;
        if (state.field11A0) {
            const auto flags = state.field11A0->flags;
            mode = (flags & 2) ? 3 : (flags & 1) ? 2 : (flags & 8) ? 4 : 1;
        }
        if (!state.flags1330[2] && state.field119D) {
            const playermodels::VacuumVolume* volume = state.field11A0 ? state.field11A0->model : nullptr;
            const auto* model = dynamic_cast<const playermodels::VacuumModel*>(volume ? volume : &vacuum.beam());
            applyTint(vacuum, mode, volume ? *volume : vacuum.beam(), services, model);
        }
        if (!state.flags10E8[1]) {
            const auto& weapon = *components.weapon->first;
            const float factor = reachScale(state);
            auto first = services.globals.parameters[0];
            auto second = services.globals.parameters[1];
            first.fields[0] = &weapon.position;
            first.fields[2] = &weapon.field610;
            first.field44 = player.fieldB4;
            first.field40 = static_cast<std::uint16_t>(player.rotation[1]);
            first.field4C = controls.flags24[0];
            first.field50 = factor;
            first.field20 = static_cast<float>(static_cast<double>(first.field20) * factor);
            second.field20 = static_cast<float>(static_cast<double>(second.field20) * factor);
            const bool aiming = components.actions->query74968() && components.attached->field2C0 == 0;
            VacuumInput input;
            input.player = &player;
            input.hand = &weapon.position;
            input.strength = controls.field20;
            input.scan = aiming && !state.flags134C[2];
            input.mode = static_cast<std::int32_t>(mode);
            input.volume = state.field11A0 ? state.field11A0->model : nullptr;
            input.parameters = &first;
            input.target = &second;
            if (vacuum.update(input)) services.rumble(6);
            updated = true;
        } else {
            services.loopSound(controls.field20, player.position, 8);
        }
    }
    if (!updated) vacuum.reset();
    return updated;
}

vacuumcapture::CaptureReaction::CaptureReaction(const std::vector<std::uint8_t>& executable) {
    lookDistance_ = constant<float>(executable, 0x804db58c);
    lookFrames_ = constant<float>(executable, 0x804d2ba4);
    weakStrength_ = constant<float>(executable, 0x804db590);
    strongStrength_ = constant<float>(executable, 0x804db56c);
    trailHeight_ = constant<float>(executable, 0x804db558);
    rangeBase_ = constant<float>(executable, 0x804d2bb8);
    rangeIdle_ = constant<float>(executable, 0x804d2bbc);
    rangeHeld_ = constant<float>(executable, 0x804d2bc0);
    rangeAir_ = constant<float>(executable, 0x804d2bc4);
    rangeBoost_ = constant<float>(executable, 0x804d2bc8);
    rangeCarry_ = constant<float>(executable, 0x804d2bcc);
    rangeSlow_ = constant<float>(executable, 0x804d2bd0);
    rangeSlowAlt_ = constant<float>(executable, 0x804d2bd4);
    rangeFast_ = constant<float>(executable, 0x804d2bd8);
    half_ = constant<float>(executable, 0x804db528);
    drainPeriod_ = constant<std::int32_t>(executable, 0x804d2c5c);
    rumbleLimit_ = static_cast<float>(constant<std::int32_t>(executable, 0x804d2c68));
}

float vacuumcapture::CaptureReaction::nozzleRange(const playeractor::State& state, const playeractions::Reactions& reactions) const {
    float value = rangeBoost_;
    if (!state.flags134C[0]) {
        value = rangeCarry_;
        if (!state.flags134C[1]) {
            const bool equipped = state.field1180 == 1 || state.field1180 == 6;
            value = rangeHeld_;
            if (!equipped) {
                value = rangeAir_;
                if (state.field1114 <= 0.0f) value = rangeBase_ + state.fieldF8C * (rangeIdle_ - rangeBase_);
            }
        }
    }
    if (0.0f < state.field1130) value *= rangeSlow_;
    if (state.field1338 == 2) value *= rangeSlowAlt_;
    if (state.flags10E8[1]) value *= state.field1108;
    if (0.0f != state.field1170) value *= rangeFast_;
    const auto* record = reactions.entries.at(reactions.state);
    if (!record) throw std::runtime_error("Original vacuum range requires the current reaction record.");
    float factor;
    std::memcpy(&factor, &(*record)[5], sizeof factor);
    return value * factor;
}

void vacuumcapture::CaptureReaction::run(playeractor::Player& player, std::uint8_t previous, ReactionServices& services) const {
    auto& state = player.state;
    auto& components = *player.components;
    auto& controls = *components.controls;
    // The capture target is published by the environment frame at the start of the update; on the frame that hooks a
    // target it is still empty and the original reads null + 0x24 for the flags (taken as zero here).
    const auto* target = state.field11A0;
    std::uint8_t reactionFlag = 0, heldFlag = 0; // uVar15 / uVar13
    std::array<std::uint32_t, 12> block{};
    const std::uint32_t flags = target ? target->flags : 0;
    actors::MoveActor* first = target && target->actor ? *target->actor : nullptr;
    if (first) {
        auto* record = services.entries.sourceRecord(&player);
        if (!record) throw std::runtime_error("Original vacuum reaction requires its aggregated source record.");
        const Vector look{first->position[0], player.position[1], first->position[2]};
        const double distance = std::sqrt(static_cast<double>((look[0] - player.position[0]) * (look[0] - player.position[0]) +
            (look[1] - player.position[1]) * (look[1] - player.position[1]) + (look[2] - player.position[2]) * (look[2] - player.position[2])));
        if (!(flags & 0x4000) && static_cast<double>(lookDistance_) < distance) {
            state.field1070 = 5;
            state.field1074 = look;
            state.field109C = lookFrames_;
        }
        state.field11A4 = services.jointPosition(*first, services.attachmentJoint(*first));
        const auto recordLife = static_cast<std::int16_t>(record[0][0] >> 16);
        if (recordLife < static_cast<std::int16_t>(player.life)) playerevents::setLife(player, recordLife, services.life);
        float ratio;
        std::memcpy(&ratio, &(*record)[7], sizeof ratio);
        state.fieldF68 = static_cast<float>(static_cast<double>(state.fieldF68) * static_cast<double>(ratio));
        unsigned count = 0;
        for (auto** p = target->actor; p && *p; ++p) {
            const auto handle = services.slotHandle(**p);
            if (handle && count < state.fields11B0.size()) state.fields11B0[count++] = handle;
        }
        if (!(flags & 0x20)) {
            if (!(flags & 0xc00)) {
                if (flags & 0x80) {
                    heldFlag = 1;
                } else if (flags & 0x1000) {
                    block[5] = 2;
                    components.actions->request(0x12, &block);
                } else if (flags & 0x2000) {
                    block[5] = 1;
                    components.actions->request(0x12, &block);
                }
            } else {
                block[1] = 5;
                block[3] = 1;
                block[4] = 1;
                services.sound(0x2809, &player.position);
                components.actions->request(3, &block);
            }
        } else {
            bool heavy = false;
            for (unsigned i = 0; i < 6; ++i) {
                const auto& slot = *services.entries.entries[i];
                if (!slot.state || slot.source != &player) continue;
                if (!(services.controller.entry(i).flags & 0x20)) continue;
                playerevents::Payload payload;
                payload.word(0x60, 3);
                payload.word(0x68, slot.target->type());
                if (3 < services.typeGroup(slot.target->type())) heavy = true;
                services.events.send(static_cast<std::int32_t>(services.playerIndex), 0x13, &payload);
            }
            block[6] = components.reactions->state;
            block[7] = heavy ? 3 : 0;
            components.actions->request(9, &block);
            services.sound(0x2821, &player.position);
            reactionFlag = 1;
        }
        if (flags & 1) {
            state.field11D4 = 1;
            if (previous == 0) {
                services.sound(0x2877, &player.position);
                float strength;
                std::memcpy(&strength, &(*record)[8], sizeof strength);
                services.rumble(weakStrength_ <= strength ? (strongStrength_ <= strength ? 10 : 9) : 8);
            }
        }
        if (!(flags & 8) || components.actions->fields31C[4] != 2) {
            state.field11CC = 0.0f;
        } else {
            const auto tick = static_cast<std::uint32_t>(static_cast<std::int32_t>(state.field11CC));
            if (tick == (tick / static_cast<std::uint32_t>(drainPeriod_)) * static_cast<std::uint32_t>(drainPeriod_)) {
                const int loss = services.doubledDamage() ? 2 : 1;
                playerevents::setLife(player, static_cast<std::int16_t>(static_cast<std::int16_t>(player.life) - loss), services.life);
            }
            state.field11CC += 1.0f;
        }
        state.field11D0 += 1.0f;
        if (state.field11D0 < rumbleLimit_) services.rumble(7);
    }
    if ((state.field1180 == 1 || components.actions->field310 == 2) && components.attached->field2C0 == 0) {
        if ((state.field1180 != 1 || 0.0f != state.field1130 || 0.0f != state.field1348) && !(flags & 0x40)) return;
        const auto& weapon = *components.weapon->first;
        playermodels::VacuumTracking tracking;
        tracking.field0 = weapon.position;
        tracking.fieldC = weapon.field610;
        tracking.field18 = state.fieldF9C.second;
        tracking.field24 = state.fieldF9C.value;
        tracking.field28 = controls.field20;
        tracking.field2C = static_cast<float>(static_cast<double>(half_) * static_cast<double>(nozzleRange(state, *components.reactions)));
        tracking.field30 = 0.0f < state.field1114 ? 1 : 0;
        tracking.field31 = controls.flags30[3];
        tracking.field38 = reactionFlag;
        tracking.field39 = heldFlag;
        services.entries.setTracking(&player, tracking);
        return;
    }
    services.sound(0x1810, &player.position);
    services.releaseTrail(components.weapon->first->position, 0x40, trailHeight_);
    services.rumble(0xb);
}