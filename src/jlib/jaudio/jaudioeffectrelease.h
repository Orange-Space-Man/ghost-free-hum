#pragma once

#include "jlib/jaudio/jaudioeffectadmission.h"
#include "jlib/jaudio/jaudioeffectparameterpool.h"
#include "jlib/jaudio/jaudiosequencecontrols.h"

#include <vector>

namespace jaudio {
    class Playback;

    struct EffectReleaseSequence {
        const EffectMetadata* metadata = nullptr;
        SequenceVolume* duckVolume = nullptr;
    };

    struct EffectReleaseContext {
        EffectPool& records;
        EffectParameterPool& parameters;
        std::vector<EffectRecord*>& channels;
        Playback& playback;
        std::uint32_t& duckMask;
        const std::vector<EffectReleaseSequence>& sequences;
        unsigned effectSequence = 0;
    };

    void releaseEffect(EffectRecord& record, EffectReleaseContext& context);
    void stopEffect(EffectRecord** handle, std::uint32_t frames, EffectReleaseContext& context);
}
