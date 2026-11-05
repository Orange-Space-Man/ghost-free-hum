#include "game/player/playersheetframe.h"
#include "game/player/playersheetplayerbinding.h"
#include "game/scene/gameplayresourceowner.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace {
    void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

    class Services final : public playersheets::FrameServices {
    public:
        Services() : binding{nullptr, position, direction} {}
        const playersheets::SuctionOwner* owner(int player) override {
            require(player == 0, "Original furniture cloth scheduler uses player zero.");
            events += 'o';
            return &binding;
        }
        bool holding(int) override { return held; }
        bool pulling(int) override { return pullingState; }
        std::uint32_t roomColor(const playerscanner::Vector&) override { events += 'l'; return 255; }
        void sound(unsigned id, const playerscanner::Vector&) override {
            require(id == 0x41 || id == 0x20 || id == 0x21, "Original scheduled cloth sound.");
            events += id == 0x41 ? 's' : 'd';
        }
        void completion(const playerevents::Payload& payload) override {
            require(payload.word(0x60) == 7, "Scheduled cloth completion preserves kind seven.");
            events += 'c';
        }
        void completionEffect(unsigned channel, const playerscanner::Vector&, const playerscanner::Vector&) override {
            events += channel ? '1' : '0';
        }
        void impact(float, const playerscanner::Vector&, unsigned) override { events += 'i'; }
        void secondaryTexture() override { throw std::runtime_error("Frame texture switching must use the actual owned resource."); }
        void rustle(unsigned, const playerscanner::Vector&, float) override { events += 'r'; }
        bool requestPull(playersheets::Grid&, const playerscanner::Vector&, float, float) override {
            pullingState = true;
            events += 'p';
            return true;
        }
        bool elasticReaction(const playerscanner::Vector&, unsigned, unsigned) override { events += 'e'; return true; }
        void releasePull(playersheets::Grid&) override { events += 'x'; }
        playerscanner::Vector position{}, direction{0, 0, 1};
        playersheets::SuctionOwner binding;
        bool held = false, pullingState = false;
        std::string events;
    };
}

void checkSheetFrame(gameplay::ResourceOwner& resources) {
    {
        auto& dispatcher = resources.managers().playerEvents();
        auto& state = dispatcher.player(0)->state;
        const auto retained = state;
        int firstOwner, secondOwner;
        const playerscanner::Vector center{10, 20, 30};
        playerevents::Payload request;
        request.pull(&center, &firstOwner, 140, 80);
        state.flags134C[2] = 0;
        state.pullOwner = nullptr;
        require(dispatcher.send(0, 0x34, &request) == 1 && state.pullOwner == &firstOwner &&
            state.field1350 == center && state.field1360 == 140 && state.field1364 == 80,
            "Cloth pull enters the real manager-owned player's event dispatcher and retained motion state.");
        require(dispatcher.send(0, 0x34, &request) == 1, "The current cloth owner can refresh its pull center.");
        request.pull(&center, &secondOwner, 999, 999);
        require(dispatcher.send(0, 0x34, &request) == 0 && state.field1360 == 140 &&
            dispatcher.send(0, 0x35, &request) == 0 && state.flags134C[2],
            "Another cloth cannot overwrite or release the retained pull owner.");
        request.pull(nullptr, &firstOwner, 0, 0);
        require(dispatcher.send(0, 0x34, &request) == 0 && dispatcher.send(0, 0x35, &request) == 1 &&
            !state.flags134C[2] && !state.pullOwner && state.field1350 == center && state.field1360 == 140,
            "Release clears ownership and the active flag while preserving the original retained geometry.");
        require(dispatcher.send(0, 0x35, &request) == 0, "Repeated pull release returns the original inactive result.");
        request.pull(&center, &firstOwner, 100, 0);
        require(dispatcher.send(0, 0x34, &request) == 1, "Pull can be reacquired after release.");
        request.pull(&center, nullptr, 200, 50);
        require(dispatcher.send(0, 0x34, &request) == 1 && !state.pullOwner && state.field1360 == 200,
            "Original anonymous pull preempts a non-null owner.");
        require(dispatcher.send(0, 0x34, &request) == 0, "Original active anonymous pull cannot refresh itself.");
        request.pull(nullptr, &secondOwner, 0, 0);
        require(dispatcher.send(0, 0x35, &request) == 1, "Any release can clear an anonymous pull.");
        request.reset();
        require(!request.pullCenter && !request.pullOwner, "Reset clears native payload references with the original pointer fields.");
        state = retained;
    }
    auto& models = *resources.managers().playerEvents().player(0)->components->grids;
    const auto& shared = *resources.managers().players().models;
    playersheets::Owner owner(models, shared, resources.furniture(), resources.executable());
    playerscanner::FurnitureCache cache(resources.executable(), resources.furniture(), resources.managers().flags());
    unsigned slot = 4, row = 0;
    cache.refresh(2, [&](const auto& input) {
        owner.initialize(input, false);
        slot = input.slot;
        row = input.row;
    });
    require(slot < 4 && row == 208, "Actual foyer cloth placement is initialized through its scanner and manager slot.");
    auto& frame = owner.sheet(slot);
    auto& model = *models.grids[slot];
    {
        auto& player = *resources.managers().playerEvents().player(0);
        const auto retained = player.state;
        playersheets::PlayerBinding binding(resources.managers().playerEvents(), 0);
        const auto& suction = binding.owner();
        require(&suction.position == player.view.field8A4 && &suction.direction == player.view.field8A8 &&
            suction.volume == player.view.field8B4, "Cloth uses the real player's published weapon and vacuum references.");
        player.state.flags134C[2] = 0;
        player.state.pullOwner = nullptr;
        const auto holding = binding.holding(), pulling = binding.pulling();
        require(binding.request(frame.grid, {1, 2, 3}, 200, 80) && player.state.pullOwner == &frame.grid,
            "The recovered cloth service connects pull ownership to its native grid identity.");
        require(binding.holding() == holding && binding.pulling() == pulling,
            "Pull dispatch does not invent an immediate player-view refresh absent from the original event.");
        binding.release(frame.grid);
        require(!player.state.flags134C[2] && !player.state.pullOwner, "Cloth release reaches the same original owner route.");
        player.state = retained;
    }
    require(models.fields16BC[slot] && model.count == frame.grid.points().size() &&
        model.points[0].linkCount == frame.grid.points()[0].linkCount && model.points[0].field2C == frame.grid.points()[0].position,
        "Owned sheet initialization publishes point constraints into the existing player allocation.");
    require(model.texture.secondary == shared.texture(13) && model.field148 == -1, "Sheet ownership begins without a player suction binding.");
    Services services;
    for (unsigned tick = 0; tick < 60; ++tick) owner.update(services);
    require(model.suctionVolume == nullptr && model.suctionPosition == &services.position && model.field148 == 0,
        "Original scheduler retains native player position references rather than truncated addresses.");
    for (unsigned i = 0; i < model.count; ++i) {
        require(model.points[i].field2C == frame.grid.points()[i].position && model.points[i].field38 == frame.grid.points()[i].velocity,
            "Each complete update publishes simulated point positions and velocities to the original owner.");
        for (float value : model.points[i].field2C) require(std::isfinite(value), "Scheduled foyer cloth retains finite positions.");
    }
    playersheets::MotionParameters parameters(resources.executable());
    parameters.iterations = 0;
    const auto before = frame.grid.points()[0].position;
    frame.update(parameters, -1, false, services);
    require(frame.grid.points()[0].position == before && model.suctionPosition == nullptr && model.field148 == -1,
        "Zero physics iterations preserve point positions while rebinding the original no-player state.");
    frame.grid.points()[0].touched = true;
    frame.suction.pullFrames = 0;
    frame.update(parameters, 0, false, services);
    require(services.pullingState && frame.suction.pullFrames == 1,
        "Scheduled feedback observes player state changes made by the pull request before evaluating its timer.");

    frame.motion.detached = true;
    for (auto& point : frame.grid.points()) { point.position = {}; point.velocity = {}; point.touched = false; point.active = true; }
    services.held = true;
    services.events.clear();
    const unsigned flags = resources.furniture().rowState(row).flags;
    owner.update(services);
    require(services.events == "osc01xd", "Scheduled completion retains effects, release and final detached sound order.");
    require(frame.suction.completed && model.field161 && models.fields16BC[slot] && resources.furniture().rowState(row).flags == flags,
        "Completion publishes state without retiring the owner slot in the same update.");
    services.events.clear();
    owner.update(services);
    require(services.events.empty() && !models.fields16BC[slot] && resources.furniture().rowState(row).flags == ((flags & ~2U) | 4U),
        "The following owner update retires the slot and replaces only the original sheet furniture bits.");
    owner.update(services);
    require(services.events.empty(), "Retired cloth slots no longer query player services.");
    playersheets::PlayerBinding playerBinding(resources.managers().playerEvents(), 0);
    playerevents::Payload completion;
    completion.word(0x60, 7);
    playerBinding.complete(completion);
}
