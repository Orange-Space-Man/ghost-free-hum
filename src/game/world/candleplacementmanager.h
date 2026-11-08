#pragma once

#include "game/events/jmpmessagesender.h"
#include "game/effects/particleeffectfactory.h"
#include "game/effects/particleglowpool.h"
#include "game/player/playeractorview.h"
#include "game/world/roomlocation.h"
#include "game/world/roomtraversalstate.h"

#include <list>
#include <functional>

namespace particles {
    class CandlePlacements final : public jmp::MessageReceiver {
    public:
        CandlePlacements(ArchiveResources& resources, EmitterPool& emitters, GlowPool& glows,
            const playeractor::View& player, const roomlocation::Rooms& rooms, const roomtraversal::State& visibility,
            std::uint32_t& randomState, std::function<bool(unsigned)> cleared,
            std::function<void(unsigned, const std::array<float, 3>&)> sound);
        ~CandlePlacements();
        bool create(jmp::ToolDataRef& entry) override;
        bool remove(jmp::ToolDataRef& entry) override;
        bool message(jmp::ToolDataRef& entry, const char* command) override;
        unsigned count() const;

    private:
        struct Candle final : EmitterCallback {
            void advance(Emitter& emitter) override;
            CandlePlacements* manager = nullptr;
            Resource parameters, texture;
            std::array<float, 3> position{}, disturbance{};
            std::array<std::uint8_t, 4> color{};
            Emitter* emitter = nullptr;
            Glow* glow = nullptr;
            unsigned room = 0, flags = 0, state = 0;
            int row = 0, height = 1;
        };
        ArchiveResources& resources;
        EmitterPool& emitters;
        GlowPool& glows;
        const playeractor::View& player;
        const roomlocation::Rooms& rooms;
        const roomtraversal::State& visibility;
        std::uint32_t& randomState;
        std::function<bool(unsigned)> cleared;
        std::function<void(unsigned, const std::array<float, 3>&)> sound;
        std::array<Candle, 40> candles;
        std::list<Candle*> active, free;
        void update(Candle& candle);
        void disturb(Candle& candle);
        void retire(Candle& candle);
        float random();
    };
}
