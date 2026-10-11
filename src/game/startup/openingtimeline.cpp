#include "game/startup/openingtimeline.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    unsigned word(const std::uint8_t* data) {
        return (unsigned(data[0]) << 24) | (unsigned(data[1]) << 16) | (unsigned(data[2]) << 8) | data[3];
    }

    float number(const std::uint8_t* data) {
        const auto bits = word(data);
        float result;
        std::memcpy(&result, &bits, 4);
        if (!std::isfinite(result)) throw std::runtime_error("Invalid opening timeline value.");
        return result;
    }

    float step(const openingscene::Segment& segment) {
        if (segment.settings.data.size() < 4) throw std::runtime_error("Missing opening playback speed.");
        const auto result = number(segment.settings.data.data());
        if (result <= 0) throw std::runtime_error("Unsupported opening playback speed.");
        return result;
    }

    unsigned duration(const openingscene::Segment& segment) {
        if (segment.scene.data.size() < 2) throw std::runtime_error("Missing opening duration.");
        return (segment.scene.data[0] << 8) | segment.scene.data[1];
    }
}

void openingscene::Timeline::reset(const Resources& resources) {
    if (resources.segments.empty()) throw std::runtime_error("Empty opening timeline.");
    *this = Timeline{};
    resetAnimations = clearFlag55 = true;
}

void openingscene::Timeline::update(const Resources& resources, bool flag55) {
    commands.clear();
    resetAnimations = clearFlag55 = false;
    if (finished || requestedBranch >= 0) return;
    if (nextSegment) {
        nextSegment = false;
        frame = drawFrame = 0;
        flags = segmentEvents = 0;
        resetAnimations = clearFlag55 = true;
        flag55 = false;
        if (segment == 0) event = 0;
    }
    const auto& current = resources.segments.at(segment);
    const auto increment = step(current);
    flags = (flags & 8) ? (flags | 16) : (flags & ~16U);
    for (unsigned index = event; index < resources.events.size(); ++index) {
        const auto& record = resources.events[index];
        if (word(record.data() + 4) != current.id) break;
        const auto time = number(record.data());
        if (frame < time) break;
        if (frame - time >= increment) continue;
        ++event;
        ++segmentEvents;
        const unsigned opcode = (record[10] << 8) | record[11];
        if (opcode < 4 || opcode == 8 || opcode == 9) {
            if (record[8] >= current.models.size()) throw std::runtime_error("Invalid opening event model.");
            commands.push_back({opcode, record[8], record[9], frame - time});
        } else if (opcode == 4 || opcode == 5) {
            const unsigned mask = opcode == 4 ? 1 : 2;
            if (!flag55) { --event; --segmentEvents; flags |= mask; }
            else flags &= ~mask;
        } else if (opcode == 6 || opcode == 7) {
            if (!flag55) flags |= 4;
            else {
                flags &= ~4U;
                if (opcode == 7) requestedBranch = 2;
            }
        } else if (opcode == 10) flags |= 8;
        else if (opcode == 11) flags &= ~8U;
    }
    animationStep = (flags & 2) ? 0 : increment;
    finished = requestedBranch < 0 && !(flags & 6) && frame + increment >= duration(current) && segment + 1 == resources.segments.size();
    if (!(flags & 3)) {
        if (frame + increment < duration(current) || !(flags & 4)) {
            drawFrame = frame;
            frame += increment;
        } else {
            event -= segmentEvents;
            segmentEvents = 0;
            frame = drawFrame = 0;
            resetAnimations = clearFlag55 = true;
        }
    }
}

void openingscene::Timeline::afterDraw(const Resources& resources) {
    if (finished || requestedBranch >= 0 || frame < duration(resources.segments.at(segment))) return;
    if (segment + 1 < resources.segments.size()) { ++segment; nextSegment = true; }
    else if (resources.initialValue & 2) { segment = 0; nextSegment = true; }
}
