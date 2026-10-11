#include "game/actors/actoranimationsound.h"

#include <cstring>

namespace {
    unsigned word(const std::uint8_t* value) {
        return (unsigned(value[0]) << 24) | (unsigned(value[1]) << 16) | (unsigned(value[2]) << 8) | value[3];
    }
}

actoranimation::SoundState::SoundState() {}

void actoranimation::SoundState::bind(actormodel::ResourceRef value, int playbackDirection, double startFrame) {
    resource = std::move(value);
    if (resource) {
        reverseIndex = 0;
        direction = playbackDirection;
        const auto& bytes = resource->bytes;
        unsigned count = (unsigned(bytes.at(0)) << 8) | bytes.at(1);
        if (!count) {
            resource.reset();
            return;
        }
        if (!(count & 0xff)) count = 0;
        unsigned index = 0;
        while (index < count) {
            const std::size_t offset = std::size_t(index) * 32 + 12;
            static_cast<void>(bytes.at(offset + 3));
            const auto bits = word(bytes.data() + offset);
            float eventFrame;
            std::memcpy(&eventFrame, &bits, 4);
            if (!(eventFrame < startFrame)) break;
            ++index;
        }
        if (direction == 1) {
            reverseIndex = 0;
            forwardIndex = index;
            frame = 0;
            field78 = 0;
        } else {
            reverseIndex = index;
            forwardIndex = 0;
            frame = 0;
            field78 = -1;
        }
    }
    for (auto& slot : slots) {
        auto* sound = slot.sound;
        if (sound && (word(slot.event + 16) & 4)) {
            sound->stop(1);
            slot.state = 0;
        }
        if (!sound || !(sound->flags() & 0xc00)) slot.state = 0;
    }
}
