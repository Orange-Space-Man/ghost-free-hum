#include "game/actors/actoranimationsound.h"

#include <cstring>
#include <limits>
#include <cmath>

namespace {
    unsigned word(const std::uint8_t* value) {
        return (unsigned(value[0]) << 24) | (unsigned(value[1]) << 16) | (unsigned(value[2]) << 8) | value[3];
    }

    float number(const std::uint8_t* value) {
        const auto bits = word(value);
        float result;
        std::memcpy(&result, &bits, 4);
        return result;
    }

    float pitch(const std::uint8_t* event, double speed) {
        return number(event + 12) + float(static_cast<std::int8_t>(event[21])) * float(speed - 1) * 0.03125f;
    }

    void modify(actoranimation::SoundSlot& slot, double speed, const actoranimation::SoundServices& services) {
        const auto* event = slot.event;
        services.pitch(*slot.sound, event[21] ? pitch(event, speed) : number(event + 12));
        unsigned volume = event[20];
        if (event[21]) {
            const float delta = 2 * float(static_cast<std::int8_t>(event[24])) * float(speed - 1);
            const auto integer = !std::isfinite(delta) || double(delta) < -2147483648.0 || double(delta) >= 2147483648.0 ?
                std::numeric_limits<std::int32_t>::min() : static_cast<std::int32_t>(delta);
            volume += static_cast<std::uint32_t>(integer);
            const auto truncated = static_cast<std::int16_t>(volume);
            if (truncated >= 128) volume = 127;
            else if (truncated < 0) volume = 0;
        }
        services.volume(*slot.sound, float(volume & 255) / 127);
    }
}

void actoranimation::SoundState::update(double currentFrame, double speed, const SoundServices& services) {
    if (!resource) return;
    const auto& bytes = resource->bytes;
    unsigned count = (unsigned(bytes.at(0)) << 8) | bytes.at(1);
    if (!(count & 255)) count = 0;
    const float rate = float(speed);
    std::memcpy(&field80, &rate, 4);
    const bool forward = direction == 1;
    if ((forward && currentFrame < frame) || (!forward && frame < currentFrame)) {
        reverseIndex = forward ? forwardIndex : count - 1;
        frame = float(currentFrame);
        if ((!forward && field78 == -1) || static_cast<std::uint32_t>(field78) < 256) ++field78;
    }
    for (auto& slot : slots) {
        if (!slot.state) continue;
        const auto* event = slot.event;
        const auto flags = word(event + 16);
        if (!(word(event) & 0xc00) && (rate != 0 || !(flags & 0x20))) {
            const double begin = number(event + 4), end = number(event + 8);
            const bool active = begin == end || (forward ?
                ((begin < end && currentFrame < end && begin <= currentFrame) ||
                    (end < begin && (currentFrame < end || begin < currentFrame))) :
                ((end < begin && end < currentFrame && currentFrame < begin) ||
                    (begin < end && (end < currentFrame || currentFrame < begin))));
            if (active) services.start(word(event), slot.sound);
            else slot.state = 0;
        }
        if (slot.sound) {
            modify(slot, speed, services);
            if ((flags & 0x10) && (forward ? number(event + 8) <= currentFrame : currentFrame <= number(event + 8)))
                slot.sound->stop(1);
        }
    }
    while (forward ? reverseIndex < count : reverseIndex != 0xffffffff) {
        const std::size_t offset = std::size_t(reverseIndex) * 32 + 8;
        static_cast<void>(bytes.at(offset + 31));
        const auto* event = bytes.data() + offset;
        const double startFrame = number(event + 4);
        if (!(forward ? startFrame <= currentFrame : currentFrame <= startFrame)) break;
        unsigned index = 0;
        while (index < slots.size() && slots[index].state && word(slots[index].event) != word(event)) ++index;
        const bool duplicate = index < slots.size() && slots[index].state && !(word(event) & 0xc00);
        const auto flags = word(event + 16);
        if (!duplicate && index < slots.size() && (!(flags & 8) || static_cast<unsigned>(field78) == event[22]) &&
            ((direction == 1 && !(flags & 2)) || (direction == -1 && !(flags & 1)))) {
            auto& slot = slots[index];
            services.start(word(event), slot.sound);
            if (slot.sound) {
                slot.resource = resource;
                slot.event = event;
                slot.state = 1;
                services.volume(*slot.sound, float(event[20]) / 127);
                services.pitch(*slot.sound, pitch(event, speed));
                services.pan(*slot.sound, float(event[23]) / 127);
            }
        }
        reverseIndex += static_cast<std::uint32_t>(direction);
    }
    frame = float(currentFrame);
}

void actoranimation::SoundState::stop() {
    for (auto& slot : slots) if (slot.sound) slot.sound->stop(0);
}
