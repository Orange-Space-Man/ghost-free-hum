#include "../recovered/playerscannerinteraction.h"
#include "../recovered/playerfurniturescancache.h"
#include "../recovered/playeractorscanbindings.h"
#include "game/scene/gameplayresourceowner.h"

#include <stdexcept>
#include <string>
#include <cmath>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    class Services final : public playerscanner::Services {
    public:
        std::int16_t delay() const override { return frames; }
        void playScanSound(unsigned sound) override { require(sound == 0x3813, "Original scan sound."); events += 's'; }
        playerscanner::Hit furniture(const playerscanner::Vector&, const playerscanner::Vector& direction) override {
            require(direction == playerscanner::Vector{0, 0, 1}, "Scanner uses camera-to-target direction.");
            events += 'f'; return furnitureHit;
        }
        playerscanner::Hit actor(const playerscanner::Vector&, const playerscanner::Vector&) override { events += 'a'; return actorHit; }
        unsigned actorType(int) override { events += 't'; return type; }
        std::optional<unsigned> actorMessage(int) override { events += 'r'; return message; }
        void setDiscoveryFlag(unsigned flag) override { require(flag == 0xad, "Original discovery flag."); events += 'd'; }
        void selectMessage(unsigned player, unsigned value) override {
            require(player == 0 && value == expectedMessage, "Original message player and low-byte payload."); events += 'm';
        }
        void openMessage() override { events += 'o'; }
        void markFurniture(int) override { events += 'k'; }
        bool mirror(int) override { events += 'w'; return isMirror; }
        bool furnitureMessage(int) override { events += 'q'; return hasMessage; }
        void openFurnitureMessage() override { events += 'v'; }
        std::int16_t frames = 2;
        playerscanner::Hit furnitureHit{}, actorHit{};
        unsigned type = 0x31, expectedMessage = 0x1a;
        std::optional<unsigned> message;
        bool isMirror = false, hasMessage = false;
        std::string events;
    };
}

void checkScannerGeometry() {
    using namespace playerscanner;
    FurnitureTarget box{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}, {2, 3, 4}, true};
    float distance = 99;
    require(intersect(box, {0, 0, 0}, {0, 0, 1}, distance) && distance == 0, "Inside scanner box hits immediately.");
    require(!intersect(box, {5, 0, 0}, {1, 0, 0}, distance) && distance == 0,
        "Outward scan misses without changing distance.");
    require(!intersect(box, {5, 0, 0}, {-1, 0, 0}, distance), "Original outside-box test rejects a zero direction axis.");
    require(intersect(box, {5, 0, 0}, {-1, 0.1f, 0.1f}, distance) && distance == 5,
        "Furniture distance is origin magnitude, not surface intersection distance.");
    require(furnitureTarget({box, box}, {5, 0, 0}, {-1, 0.1f, 0.1f}, 100).index == 0,
        "Equal furniture distances retain the first entry.");
    require(furnitureTarget({box}, {5, 0, 0}, {-1, 0.1f, 0.1f}, 5).index == -1,
        "Furniture maximum distance is exclusive.");
    ActorTarget actor{{0, 0, 10}, 2, 17, true, true, true, true, true};
    require(actorTarget({actor}, {}, {0, 0, 1}, 100).index == 17, "Actor scan returns owner slot.");
    actor.position[0] = 2;
    require(actorTarget({actor}, {}, {0, 0, 1}, 100).index == -1, "Actor radius boundary is exclusive.");
    actor.position = {0, 0, -10};
    require(actorTarget({actor}, {}, {0, 0, 1}, 100).index == -1, "Actor behind scanner is excluded.");
}

void checkScannerInteraction(gameplay::ResourceOwner& owner) {
    const auto& models = owner.managers().preparePlayerDoor(0).catalog;
    actorcollision::Catalog collisions(owner.executable(), models);
    playerscanner::ActorBindings actorBindings(owner.managers().enemies(), owner.managers().actors(), models, collisions, owner.visibility());
    const auto* player = owner.managers().playerEvents().player(0);
    require(actorBindings.type(player->actorId) == player->actorType, "Actor scanner resolves the real registry identity.");
    const auto hit = actorBindings.query(player->position, {0, 0, 1});
    require(hit.index == -1, "Initial foyer has no eligible visible attached actor scan proxy.");
    playerscanner::FurnitureCache cache(owner.executable(), owner.furniture(), owner.managers().flags());
    std::vector<playerscanner::SheetInput> sheets;
    const auto initializeSheet = [&](const playerscanner::SheetInput& input) {
        require(input.slot < 4 && input.kind >= 1 && input.kind <= 24, "Original sheet binding range.");
        sheets.push_back(input);
    };
    require(cache.refresh(2, initializeSheet), "First room builds actual mission furniture scan entries.");
    require(cache.targets().size() == 8 && cache.row(0) == 99 && cache.row(7) == 296,
        "Foyer scanner retains mission order including model-less furniture.");
    require(!cache.refresh(2, {}), "Unchanged room retains the original scan cache.");
    bool foundMirror = false, foundHit = false;
    for (unsigned i = 0; i < cache.targets().size(); ++i) {
        if (cache.row(i) == 208) {
            bool covered = false;
            for (const auto& sheet : sheets) if (sheet.target == i) covered = true;
            require(cache.mirror(i) == !covered, "Sheet-bound mirrors are excluded by the original scanner gate.");
            foundMirror = true;
        }
        const auto before = owner.furniture().rowState(cache.row(i)).flags;
        cache.mark(i);
        require(owner.furniture().rowState(cache.row(i)).flags == (before | 0x20),
            "Scan discovery updates the shared furniture row without replacing its flags.");
        require(!cache.occupied(i), "Initial furniture occupant sentinel remains empty.");
        const auto& target = cache.targets()[i];
        if (!target.enabled) continue;
        playerscanner::Vector center;
        for (unsigned axis = 0; axis < 3; ++axis) center[axis] =
            -(target.inverse[axis] * target.inverse[3] + target.inverse[axis + 4] * target.inverse[7] +
                target.inverse[axis + 8] * target.inverse[11]);
        if (cache.query(center, {0.1f, 0.2f, 1}).index >= 0) foundHit = true;
    }
    require(foundMirror && foundHit, "Real foyer records provide mirror classification and scanner intersections.");
    require(cache.refresh(30, initializeSheet) && cache.targets().size() == 3 && cache.row(0) == 335,
        "Room change replaces scan entries with the adjoining room's mission rows.");
    require(cache.refresh(2, initializeSheet) && (owner.furniture().rowState(208).flags & 0x20),
        "Returning to a room retains shared discovery state.");
    auto& state = *owner.managers().playerEvents().player(0)->components->scanner;
    const auto savedState = state.state;
    const auto saved8 = state.field8, savedC = state.fieldC;
    const float savedTimer = state.field10;
    auto* savedCamera = state.camera;
    gameplaycamera::CameraOwner camera;
    camera.position = {};
    camera.target = {0, 0, 10};
    playerscanner::Input input{true, false, camera};
    Services services;
    bool warp = true;
    state.state = {};
    require(playerscanner::update(state, input, warp, services) && !warp && state.field10 == 2 && services.events == "s",
        "Starting scan sets delay without scanning in the same frame.");
    input.pressed = false;
    require(playerscanner::update(state, input, warp, services) && std::abs(state.field10 - 0.8f) < 0.00001f,
        "Scanner decrements the original 1.2-unit clock.");
    services.furnitureHit = {3, 10}; services.actorHit = {4, 10}; services.isMirror = true;
    require(playerscanner::update(state, input, warp, services) && warp && !state.state[0] && state.fieldC == 0xffffffff &&
        services.events == "sfakw", "Furniture wins equal distance and mirror warp remains active after timer expiry.");
    services = Services{};
    services.actorHit = {4, 3};
    state.state = {1, 0, 1}; state.field10 = 1;
    warp = true;
    require(!playerscanner::update(state, input, warp, services) && warp && services.events == "fatdmo" && !state.state[2],
        "Actor scan publishes message in original order while leaving inactive output untouched.");
    services = Services{}; services.actorHit = {4, 3}; services.type = 0x80;
    services.message = 0x123; services.expectedMessage = 0x23;
    state.state = {1, 0, 0}; state.field10 = 1;
    playerscanner::update(state, input, warp, services);
    require(services.events == "fatrdmo", "Actor placement message truncates to one byte.");
    services = Services{};
    state.state = {1, 0, 0}; state.field10 = 5; input.cancel = true;
    require(!playerscanner::update(state, input, warp, services) && services.events.empty() && state.field10 == 0,
        "Early cancellation does not query targets.");
    state.state = savedState; state.field8 = saved8; state.fieldC = savedC;
    state.field10 = savedTimer; state.camera = savedCamera;
}
