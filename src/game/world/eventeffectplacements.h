#pragma once

#include "game/events/jmpmessagesender.h"
#include "game/effects/trainingfireworks.h"

namespace particles {
    class EventEffectPlacements final : public jmp::MessageReceiver {
    public:
        EventEffectPlacements(ArchiveResources& resources, EmitterPool& emitters, TrainingFireworks& fireworks);
        bool message(jmp::ToolDataRef& entry, const char* command) override;

    private:
        TrainingFireworks& fireworks;
        ArchiveResources& resources;
        EmitterPool& emitters;
    };
}
