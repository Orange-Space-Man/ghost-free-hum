#include "jlib/jaudio/jaudioeffectrelease.h"
#include "jlib/jaudio/jaudioplayback.h"

#include <stdexcept>

void jaudio::releaseEffect(EffectRecord& record, EffectReleaseContext& context) {
    if (!record.state || !record.parameters) throw std::runtime_error("Effect release requires an active admitted sound.");
    if (record.state != 1) context.playback.stopEffectTrack(record.channel);
    if (context.duckMask && (record.metadataFlags() & 8)) {
        for (unsigned index = 0; index < context.sequences.size(); ++index) {
            const auto& sequence = context.sequences[index];
            if (index == context.effectSequence || !sequence.metadata || (sequence.metadata->flags & 8)) continue;
            const unsigned shift = record.channel & 63;
            context.duckMask &= ~(shift < 32 ? 1u << shift : 0);
            if (!context.duckMask) {
                if (!sequence.duckVolume) throw std::runtime_error("Active ducked sequence requires its retained volume slot.");
                sequence.duckVolume->set(127, 3);
            }
        }
    }
    for (auto& channel : context.channels) {
        if (channel != &record) continue;
        channel = nullptr;
        break;
    }
    if (record.binding) *record.binding = nullptr;
    record.state = 0;
    context.parameters.release(*record.parameters);
    context.records.recycle(record);
}

void jaudio::stopEffect(EffectRecord** handle, std::uint32_t frames, EffectReleaseContext& context) {
    if (!handle || !*handle) return;
    auto& record = **handle;
    if (record.soundId & 0xc0000000) throw std::runtime_error("Effect stop received a non-effect sound.");
    if (record.state) {
        if (!frames || record.state == 1) releaseEffect(record, context);
        else {
            if (!record.parameters) throw std::runtime_error("Effect fade requires its admitted parameter bank.");
            record.stopFrames = frames;
            record.parameters->controls.set(EffectControl::volume, 6, 0, frames);
        }
    }
    *handle = nullptr;
}
