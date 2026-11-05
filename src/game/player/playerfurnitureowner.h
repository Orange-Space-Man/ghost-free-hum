#pragma once

#include "game/player/playerfurnitureframe.h"
#include "game/player/playersheetplayerbinding.h"
#include "game/audio/jaudiopositionalplayback.h"
#include "game/effects/particleeffectfactory.h"

namespace playerinteraction {
    class FurnitureOwner final : private playersheets::FrameServices {
    public:
        FurnitureOwner(playeractor::Player& player, playerevents::Dispatcher& events,
            playermodels::SharedModels& shared, roomfurniture::Owner& furniture, const gameflags::State& flags,
            jaudio::PositionalPlayback& playback, particles::EffectFactory& effects, const std::vector<std::uint8_t>& executable);
        void update(const roomlocation::Rooms& rooms, enemies::Manager& enemies, const actormodel::Catalog& catalog,
            const actorcollision::Manager& collision, const mapcollision::State& map, unsigned mapMode,
            const std::function<std::uint32_t()>& random);
        void updateSheets();

    private:
        const playersheets::SuctionOwner* owner(int index) override;
        bool holding(int index) override;
        bool pulling(int index) override;
        std::uint32_t roomColor(const playerscanner::Vector& position) override;
        void sound(unsigned id, const playerscanner::Vector& position) override;
        void completion(const playerevents::Payload& payload) override;
        void completionEffect(unsigned channel, const playerscanner::Vector& position,
            const playerscanner::Vector& direction) override;
        void impact(float radius, const playerscanner::Vector& position, unsigned count) override;
        void secondaryTexture() override;
        void rustle(unsigned id, const playerscanner::Vector& position, float volume) override;
        bool requestPull(playersheets::Grid& sheet, const playerscanner::Vector& center, float range, float elasticity) override;
        bool elasticReaction(const playerscanner::Vector& center, unsigned reaction, unsigned action) override;
        void releasePull(playersheets::Grid& sheet) override;
        playeractor::Player& player;
        roomfurniture::Owner& furniture;
        jaudio::PositionalPlayback& playback;
        particles::EffectFactory& effects;
        playersheets::PlayerBinding binding;
        playerscanner::FurnitureCache cache;
        playersheets::Owner sheets;
        FurnitureFrame frame;
    };
}
