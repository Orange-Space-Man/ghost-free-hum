#include "game/enemies/enemyfiregeneratorstrategy.h"

#include <stdexcept>

enemystrategy::FireGenerator::FireGenerator(FireGeneratorEnvironment& environment) : environment(environment) {}

enemystrategy::FireGenerator::~FireGenerator() {
    if (candle) candle->release();
    if (glow) environment.glows.release(glow);
}

charactergenerator::Generator& enemystrategy::FireGenerator::generator() const {
    if (!context) throw std::runtime_error("Original fire strategy has no generator owner.");
    return *static_cast<charactergenerator::Generator*>(context);
}

void enemystrategy::FireGenerator::initializeBehavior() {
    auto& parent = generator();
    if (state == 0) {
        position = parent.position;
        const auto room = environment.runtime.resolveRoom(position);
        const float floor = static_cast<float>(environment.rooms.entries().at(room >> 24).bounds[1]);
        const auto range = static_cast<std::uint32_t>(parent.parameter("arg0"));
        const auto height = static_cast<std::uint32_t>(parent.parameter("arg1"));
        upperHeight = floor + static_cast<float>(height + range);
        lowerHeight = floor + static_cast<float>(height - range);
        angle = static_cast<std::uint16_t>(parent.parameter("arg2"));
        radius = static_cast<float>(static_cast<std::uint32_t>(parent.parameter("arg7")));
        requiresVacuum = parent.parameter("arg6") != 0;
        appearTime = static_cast<std::uint32_t>(parent.parameter("appear_time"));
        parent.flags |= 2;
    } else if (state == 1) {
        attractionFrames = 0;
        changed = false;
    }
}

void enemystrategy::FireGenerator::createGlow() {
    glow = environment.glows.create(generator().position, 2.6f, 1, {0xcf, 0x7e, 0x50, 0xff});
}

void enemystrategy::FireGenerator::createCandle() {
    candle = environment.particles.create(environment.common.get(0x4b485f45, "candle_moe.pcr"),
        environment.common.get(0x4b485f45, "ps_ball.bti"));
    if (candle) {
        candle->position = position;
        candle->field220 = {};
        candle->field22C = {};
        candle->fieldC4 = nullptr;
        candle->fieldC8 = nullptr;
    }
}

void enemystrategy::FireGenerator::behavior() {
    if (state == 0) {
        auto& parent = generator();
        if (parent.parameter("arg3") != 0) throw std::runtime_error("Original non-candle generator requires the flame pool.");
        createCandle();
        createGlow();
        if (candle) {
            disabled = parent.parameter("arg4") == 1;
            if (disabled) candle->flags &= 0xfffd;
            setNextState(1);
        }
        return;
    }
    if (state != 1) return;
    if (disabled) ++disabledFrames;
    if (static_cast<std::int32_t>(disabledFrames) > 150) {
        if (candle) {
            if (!glow) createGlow();
            candle->flags |= 2;
        }
        disabled = false;
    }
    const auto& player = environment.player;
    if (environment.runtime.roomVisible(environment.runtime.resolveRoom(position), 0) && player.field89E &&
        (player.field8A0 == 2 || player.field8A0 == 3)) {
        throw std::runtime_error("Original active elemental spray requires generator interaction services.");
    }
    if (!disabled) {
        if (player.field8B4) throw std::runtime_error("Original fire attraction requires the active vacuum query.");
        if (attractionFrames) --attractionFrames;
        if (changed && candle) {
            candle->release();
            createCandle();
            if (glow) glow->scale = 2.6f;
            changed = false;
        }
        if (candle && (candle->flags & 8) && !(candle->flags & 0x40)) {
            if (!glow) createGlow();
        } else if (glow) {
            environment.glows.release(glow);
            glow = nullptr;
        }
    }
    if (!disabled && player.flagsF12[0] == 1) {
        throw std::runtime_error("Original fire-element creation requires its later-area actor strategy.");
    }
}
