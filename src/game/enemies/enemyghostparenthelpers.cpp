#include "game/enemies/enemyghostparenthelpers.h"
#include <cmath>
#include <limits>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
    constexpr float knockbackDecay = 0.62f;
    constexpr double knockbackBaseDecay = 0.25;
    constexpr float fadeStep = 0.033333335f;

    const char* parameterName(ghostparent::Parameter parameter) {
        switch (parameter) {
        case ghostparent::Parameter::Speed: return "mSpeed";
        case ghostparent::Parameter::SpeedUnseen: return "mSpeedUnseen";
        case ghostparent::Parameter::SpeedFight: return "mSpeedFight";
        case ghostparent::Parameter::Eyesight: return "mEyesight";
        case ghostparent::Parameter::AttackPattern1: return "mAttackPattern1";
        case ghostparent::Parameter::AttackKarakai: return "mNumAtkKarakai";
        case ghostparent::Parameter::AttackOrooro: return "mNumAtkOrooro";
        case ghostparent::Parameter::ActAfterAttackSuccess: return "mActAfterAttackSu";
        case ghostparent::Parameter::ActAfterAttackFailure: return "mActAfterAttackFa";
        case ghostparent::Parameter::TsuriType: return "mTsuriType";
        case ghostparent::Parameter::Element: return "mElement";
        }
        throw std::invalid_argument("Unknown ghost parameter.");
    }
}

std::int32_t ghostparent::parameterWord(const gameparameters::Parameters& parameters, Parameter parameter) {
    const auto& entry = parameters.entries().at(static_cast<unsigned>(parameter));
    if (entry.name != parameterName(parameter))
        throw std::runtime_error("Original ghost parameter index does not match the native parameter list.");
    return static_cast<std::int32_t>(entry.bits);
}

float ghostparent::speedForMode(const gameparameters::Parameters& parameters, int mode) {
    Parameter parameter;
    if (mode == 0) parameter = Parameter::Speed;
    else if (mode == 1) parameter = Parameter::SpeedUnseen;
    else if (mode == 2) parameter = Parameter::SpeedFight;
    else throw std::invalid_argument("Original ghost speed mode must be 0, 1 or 2.");
    const float value = static_cast<float>(static_cast<double>(parameterWord(parameters, parameter)));
    return (30.0f * value) / 15.0f;
}

float ghostparent::randomOffset(float base, const std::function<std::uint32_t()>& random) {
    const float sample = static_cast<float>(static_cast<double>(static_cast<std::int32_t>(random() % 10000))) / 9999.0f;
    const float offset = 2.0f * static_cast<float>(static_cast<double>(sample) - static_cast<double>(0.5f));
    return static_cast<float>(static_cast<double>(base) + static_cast<double>(offset));
}

void ghostparent::faceTarget(actors::MoveActor& actor, const actormodel::InitialPose& pose,
    const std::array<float, 3>& target, std::array<float, 3>* displacement) {
    const std::array<float, 3> delta{target[0] - actor.position[0], target[1] - actor.position[1], target[2] - actor.position[2]};
    actor.previousRotation[1] = static_cast<std::int16_t>(pose.angle(delta[2], delta[0]));
    if (displacement) *displacement = delta;
}

void ghostparent::afterAttack(enemystrategy::Strategy& strategy, int action) {
    if (action == 1) strategy.setNextState(10);
    else if (action == 0) strategy.setNextState(5);
}

void ghostparent::scale(std::array<float, 3>& vector, double factor) {
    for (auto& component : vector) component = static_cast<float>(static_cast<double>(component) * factor);
}

void ghostparent::updateKnockback(Fields& fields, actors::MoveActor& actor) {
    if (fields.knockbackFrames <= 0) return;
    auto base = fields.knockbackBase;
    const auto previous = fields.knockback;
    for (auto& component : fields.knockback) component *= knockbackDecay;
    std::array<float, 3> result = base;
    scale(base, knockbackBaseDecay);
    for (unsigned i = 0; i < 3; ++i) result[i] += previous[i] - base[i];
    actor.setVelocity(result);
    fields.knockbackBase = result;
    --fields.knockbackFrames;
}

int ghostparent::maximumAlpha(const Fields& fields) {
    return fields.fadeScaled == 0 ? 0xff : static_cast<int>(255.0f * 0.75f);
}

int ghostparent::minimumAlpha(const Fields& fields) {
    return fields.fadeScaled == 0 ? 0x96 : static_cast<int>(150.0f * 0.75f);
}

void ghostparent::updateFade(const Fields& fields, characterowner::State& state, const actors::MoveActor& actor) {
    if (fields.invisibleMode == 1 && (actor.flags & 0xc000) != 0) return;
    auto* controlled = dynamic_cast<actors::MoveActor*>(state.controlActor);
    if (!controlled) throw std::runtime_error("Original alpha fade requires its controlled actor.");
    state.maximum = maximumAlpha(fields) & 0xff;
    state.minimum = minimumAlpha(fields) & 0xff;
    // The original fade object stores its goal and step at +0x8B4 and +0x8B8, which the native State names speed and target.
    float& goal = state.speed;
    float& step = state.target;
    bool retarget = true;
    if (state.value < 1.0f && 0.0f < state.value) retarget = false;
    if (retarget) {
        goal = 1.0f - state.value;
        step = fadeStep;
    }
    const float difference = goal - state.value;
    if (difference < -step || step < difference) {
        float direction = 1.0f;
        if (difference <= 0.0f) {
            direction = 0.0f;
            if (difference < 0.0f) direction = -1.0f;
        }
        state.value += step * direction;
    } else {
        state.value = goal;
    }
    const float minimum = static_cast<float>(static_cast<double>(state.minimum));
    const float span = static_cast<float>(static_cast<double>(state.maximum - state.minimum));
    const auto alpha = static_cast<std::uint8_t>(static_cast<std::int32_t>(minimum + state.value * span));
    controlled->visibility(alpha, alpha);
}

int ghostparent::sightQuery(enemies::Manager& manager, const actors::MoveActor& actor, std::int32_t eyesight,
    std::array<float, 3>* displacement) {
    const float range = 100.0f + (2000.0f * static_cast<float>(static_cast<double>(eyesight))) / 20.0f;
    if (displacement) manager.nearestPlayer(actor.position, displacement);
    int result = manager.nearPlayer(actor.position, range);
    if (!manager.roomVisible(actor.resolvePositionRoom())) result = -1;
    return result;
}

double ghostparent::reciprocalSquareRoot(double value) {
    // 80087C74: three Newton steps from the hardware estimate (modelled here with the exact root; Gekko estimate bits are deferred).
    if (0.0 < value) {
        float estimate = static_cast<float>(1.0 / std::sqrt(value));
        for (int step = 0; step < 3; ++step)
            estimate = 0.5f * estimate * (3.0f - static_cast<float>(value * static_cast<double>(estimate * estimate)));
        return static_cast<double>(estimate);
    }
    if (value != 0.0) return static_cast<double>(std::numeric_limits<float>::quiet_NaN());
    return static_cast<double>(std::numeric_limits<float>::infinity());
}
