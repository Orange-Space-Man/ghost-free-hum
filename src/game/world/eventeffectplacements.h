#pragma once

#include "game/events/jmpmessagesender.h"
#include "game/effects/particleeffectfactory.h"

namespace particles {
    class EventEffectPlacements final : public jmp::MessageReceiver {
    public:
        EventEffectPlacements(ArchiveResources& resources, EmitterPool& emitters);
        bool message(jmp::ToolDataRef& entry, const char* command) override;

    private:
        ArchiveResources& resources;
        EmitterPool& emitters;
    };
}
