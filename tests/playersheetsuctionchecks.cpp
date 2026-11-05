#include "game/player/playersheetsuction.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace {
    void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

    class Volume final : public playermodels::VacuumVolume {
    public:
        bool intersects(double radius, playerscanner::Vector&, const playerscanner::Vector&) const override {
            require(radius == 50, "Original sheet vacuum query radius.");
            ++queries;
            return accepted;
        }
        bool accepted = true;
        mutable unsigned queries = 0;
    };

    class Services final : public playersheets::SheetServices {
    public:
        void sound(unsigned id, const playerscanner::Vector&) override {
            require(id == 0x41, "Original cloth initial-capture sound.");
            events += 's';
        }
        void completion(const playerevents::Payload& payload) override {
            require(payload.word(0x60) == 7 && payload.word(0x70) == 0 && payload.word(0x6c) == 0xffffffff,
                "Cloth completion uses the original initialized event-0x13 payload and kind seven.");
            events += 'c';
        }
        void completionEffect(unsigned channel, const playerscanner::Vector&, const playerscanner::Vector&) override {
            events += channel ? '1' : '0';
        }
        void impact(float radius, const playerscanner::Vector&, unsigned count) override {
            require(radius == 40 && count == 0x80, "Original sheet impact particle parameters.");
            events += 'i';
        }
        void secondaryTexture() override { events += 't'; }
        void rustle(unsigned id, const playerscanner::Vector&, float volume) override {
            lastSound = id;
            lastVolume = volume;
            events += 'r';
        }
        bool requestPull(playersheets::Grid&, const playerscanner::Vector& center, float range, float elasticity) override {
            lastCenter = center;
            lastRange = range;
            lastElasticity = elasticity;
            events += 'p';
            return acceptPull;
        }
        bool elasticReaction(const playerscanner::Vector&, unsigned reaction, unsigned action) override {
            require(reaction == 5 && action == 6, "Original elastic cloth reaction/action payload.");
            events += 'e';
            return acceptReaction;
        }
        void releasePull(playersheets::Grid&) override { events += 'x'; }
        std::string events;
        unsigned lastSound = 0;
        float lastVolume = 0;
        float lastRange = 0, lastElasticity = 0;
        playerscanner::Vector lastCenter{};
        bool acceptPull = true, acceptReaction = true;
    };
}

void checkSheetSuction(const std::vector<std::uint8_t>& executable) {
    playersheets::Builder builder(executable);
    playerscanner::SheetInput input{0, 0, 0, 1, 0, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}, {100, 50, 100}, false, false};
    const auto layout = builder.build(input, 256, 0, 13, false);
    playersheets::Grid grid(256);
    grid.initialize(layout);
    playersheets::Motion motion(grid, executable);
    playersheets::Suction suction(grid, motion, executable);
    Services services;
    Volume volume;
    const playerscanner::Vector position{}, direction{0, 0, 1};
    playersheets::SuctionOwner owner{&volume, position, direction};
    for (auto& point : grid.points()) point.attracted = point.touched = true;
    suction.attract(nullptr, services);
    for (const auto& point : grid.points()) require(!point.attracted && !point.touched, "Absent vacuum owner clears both point capture flags.");
    require(services.events.empty(), "Absent attached owner produces no completion or sound.");

    const unsigned target = 7 * 16;
    for (auto& point : grid.points()) point.position = {1000, 1000, 1000};
    grid.points()[target].position = {15, 0, 0};
    suction.attract(&owner, services);
    require(!grid.points()[target].touched && grid.points()[target].attracted,
        "Exact attached capture radius attracts but does not capture.");
    grid.points()[target].position = {14, 0, 0};
    suction.attract(&owner, services);
    require(grid.points()[target].touched && services.events == "s", "Crossing attached capture radius plays the first-capture sound.");
    for (unsigned x = 0; x < 4; ++x)
        require(grid.points()[7 * 16 + x].attracted, "Cloth overhang capture marks the entire strip to its anchored border.");
    suction.requestRetraction(true);
    require(motion.pulling && suction.releaseStarted && motion.pullDirection == std::array<float, 2>{-1, 0},
        "Left overhang capture requests the original signed directional retraction.");

    grid.initialize(layout);
    playersheets::Motion uniformMotion(grid, executable);
    playersheets::Suction uniform(grid, uniformMotion, executable);
    grid.points()[7 * 16 + 7].touched = true;
    uniform.requestRetraction(true);
    require(uniformMotion.retracting && !uniformMotion.pulling, "Interior capture requests uniform retraction.");

    grid.initialize(layout);
    playersheets::Motion detachedMotion(grid, executable);
    detachedMotion.detached = true;
    playersheets::Suction detached(grid, detachedMotion, executable);
    services.events.clear();
    for (auto& point : grid.points()) { point.position = {1000, 0, 0}; point.velocity = {}; }
    grid.points()[0].position = {50, 0, 0};
    grid.points()[0].velocity = {30, 2, 3};
    const unsigned queries = volume.queries;
    detached.attract(&owner, services);
    require(grid.points()[0].velocity == playerscanner::Vector{-50, 0, 0} && !grid.points()[0].touched && volume.queries == queries,
        "Detached near-range suction replaces velocity by displacement and bypasses the vacuum volume query.");
    grid.points()[0].position = {100, 0, 0};
    grid.points()[0].velocity = {30, 2, 3};
    detached.attract(&owner, services);
    require(grid.points()[0].velocity == playerscanner::Vector{-20, 2, 3},
        "At twice detached force, suction adds force instead of replacing velocity.");
    for (unsigned i = 0; i < 191; ++i) grid.points()[i].position = {};
    detached.attract(&owner, services);
    require(!detached.completed, "One point below the original three-quarter completion threshold remains active.");
    grid.points()[191].position = {};
    detached.attract(&owner, services);
    require(detached.completed && services.events == "sc01", "Completion dispatch precedes both original effect channels.");
    for (const auto& point : grid.points()) require(!point.active, "Completion disables every cloth point after dispatching effects.");

    grid.initialize(layout);
    playersheets::Motion impactMotion(grid, executable);
    playersheets::Suction impact(grid, impactMotion, executable);
    services.events.clear();
    impact.impactReady = true;
    auto& center = grid.points()[7 * 16 + 7];
    center.position = {0, 49, 0};
    center.velocity = {0, -30, 0};
    center.touched = true;
    impact.collide(services, true);
    require(!center.touched && impact.impactReady && services.events.empty(),
        "Collision clears point capture at the exact impact threshold without emitting effects.");
    center.position[1] = 49;
    center.velocity[1] = -31;
    impact.collide(services, true);
    require(!impact.impactReady && services.events == "it", "Strong collision emits particles before switching the sheet texture.");
    impact.collide(services, true);
    require(services.events == "it", "Disarmed collision effects do not repeat.");

    grid.initialize(layout);
    playersheets::Motion soundMotion(grid, executable);
    playersheets::Suction sound(grid, soundMotion, executable);
    services.events.clear();
    sound.previousSpeed = 0;
    sound.soundDelay = 1;
    for (auto& point : grid.points()) point.attracted = false;
    grid.points()[0].velocity = {6, 0, 0};
    sound.updateSound(services);
    require(sound.soundDelay == 0 && services.events.empty(), "Cloth sound delay decrements after the sound decision.");
    grid.points()[0].velocity = {7, 0, 0};
    sound.updateSound(services);
    require(services.events == "r" && services.lastSound == 0x1812 && std::abs(services.lastVolume - 0.4f) < 0.0001f,
        "Bordered cloth rustle uses speed increase and original gain scaling.");
    grid.points()[0].active = false;
    grid.points()[0].attracted = true;
    sound.updateSound(services);
    require(sound.previousSpeed == 14 && services.lastVolume == 1,
        "Original cloth sound includes inactive points and doubles attracted-point contributions.");

    grid.initialize(layout);
    playersheets::Motion feedbackMotion(grid, executable);
    playersheets::Suction feedback(grid, feedbackMotion, executable);
    services.events.clear();
    grid.points()[target].touched = true;
    feedback.pullFrames = 44;
    feedback.updateFeedback(true, true, false, true, services);
    require(feedback.pullFrames == 45 && services.events == "p" && services.lastRange == 320 &&
        services.lastCenter == grid.points()[7 * 16 + 4].position,
        "Feedback clamps overhang samples to the border and retains the exact 45-frame threshold.");
    feedback.updateFeedback(true, true, false, true, services);
    require(feedback.pullFrames == 46 && services.events == "ppx" && feedbackMotion.pulling,
        "The next pull frame begins directional retraction before releasing player attachment.");
    services.events.clear();
    services.acceptPull = false;
    feedback.updateFeedback(true, true, false, true, services);
    require(services.events == "p" && !grid.points()[target].touched && feedback.pullFrames == 46,
        "Rejected pull clears captured flags without changing the retained timer or sending release.");
    services.acceptPull = true;
    feedback.updateFeedback(true, true, true, false, services);
    require(services.events == "px" && feedback.pullFrames == 0 && !feedback.releaseStarted,
        "An empty capture clears feedback state and releases a player still holding the cloth.");

    input.elastic = true;
    grid.initialize(builder.build(input, 256, 0, 13, false));
    playersheets::Motion elasticMotion(grid, executable);
    playersheets::Suction elastic(grid, elasticMotion, executable);
    grid.points()[target].touched = true;
    elastic.pullFrames = 45;
    services.events.clear();
    elastic.updateFeedback(true, true, false, true, services);
    require(services.events == "pex" && elastic.impactReady && services.lastElasticity == 450 && !elasticMotion.pulling,
        "Elastic feedback requests the original reaction and arms impact effects without retracting the sheet.");
}
