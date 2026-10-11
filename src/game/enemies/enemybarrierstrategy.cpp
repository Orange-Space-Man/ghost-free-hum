#include "game/enemies/enemybarrierstrategy.h"

#include <stdexcept>

enemystrategy::Barrier::Barrier(BarrierEnvironment& environment) : environment(environment) {}

enemystrategy::Barrier::~Barrier() {
    if (effect) effect->release();
}

void enemystrategy::Barrier::begin() {
    if (!owner || !owner->source.table || owner->source.row < 0 ||
        !owner->source.table->getValue(owner->source.row, "arg0", door)) {
        throw std::runtime_error("Original barrier requires its door placement argument.");
    }
    environment.doors.setEventBlocked(static_cast<std::uint8_t>(door), true);
}

unsigned enemystrategy::Barrier::doorId() const { return door; }

unsigned enemystrategy::Barrier::playerArea() const {
    const auto* bits = environment.player.bits[0];
    if (!bits || !bits->data || bits->bytes <= 0x1b) throw std::runtime_error("Original barrier requires player capture flags.");
    if (bits->data[0x1b] & 0x20) return 4;
    if (bits->data[0x1b] & 0x10) return 3;
    if (bits->data[0x1a] & 0x20) return 2;
    return (bits->data[8] & 1) ? 1 : 0;
}

void enemystrategy::Barrier::initializeBehavior() {
    if (state != 0) throw std::runtime_error("Original barrier unlocking requires the later-area player event route.");
    const auto& actor = dynamic_cast<actors::MoveActor&>(owner->actor());
    const unsigned type = actor.type();
    if (type < 0x10e || type > 0x111) throw std::runtime_error("Original barrier has an unsupported actor type.");
    effect = dynamic_cast<modeleffects::BarrierOwner*>(environment.models.create(type == 0x10e ? 0x24 : 0x25, actor.position));
    const auto texture = environment.common.get(0x4b485f45, "hoso_senko4.bti");
    const char* names[]{"bkey_p_senko.pcr", "bkey_g_senko.pcr", "bkey_b_senko.pcr", "bkey_y_senko.pcr"};
    area = type == 0x10e ? 1 : type == 0x10f ? 2 : 3;
    const auto particle = environment.common.get(0x4b485f45, names[type - 0x10e]);
    if (effect) {
        effect->particle = particle;
        effect->texture = texture;
    }
    if (environment.runtime.doorOpened(0, door) && area == playerArea()) {
        environment.doors.setCollisionActive(static_cast<std::uint8_t>(door), false);
    }
}

void enemystrategy::Barrier::behavior() {
    if (state != 0) throw std::runtime_error("Original barrier unlocking requires the later-area player event route.");
    if (!environment.runtime.doorOpened(0, door)) return;
    throw std::runtime_error("Original barrier key interaction requires the later-area player event route.");
}
