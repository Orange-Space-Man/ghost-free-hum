#include "resources/dolresources.h"
#include "game/scene/gameplaymanagerservices.h"
#include "game/actors/charactereffectresources.h"
#include "game/enemies/enemymanagerruntime.h"
#include "game/actors/characterplacementruntime.h"
#include "game/enemies/enemygeneratormanager.h"
#include "game/world/cobwebplacementmanager.h"
#include "game/enemies/enemybarrierstrategy.h"
#include "game/enemies/enemyfiregeneratorstrategy.h"
#include "resources/roomarchivereader.h"
#include "game/startup/startupresources.h"
#include "game/player/playeractioncontext.h"

#include <stdexcept>
#include <cmath>

namespace {
    struct Audio {
        Audio(const gameplay::ManagerDependencies& dependencies) :
            playback(owners, dependencies.executable, dependencies.package, dependencies.audio,
                [this, &rooms = dependencies.actors.rooms](const std::array<float, 3>& position) {
                    const std::array<float, 3> rounded{std::ceil(position[0]), std::ceil(position[1]), std::ceil(position[2])};
                    const unsigned room = rooms.resolve(rounded);
                    return context.parameters(room, -1).packed;
                }),
            context(dependencies.mission, dependencies.flags, dependencies.entry.map),
            music(dependencies.package, dependencies.audio, context, dependencies.audioLife) {}
        jaudio::PositionOwners owners;
        jaudio::PositionalPlayback playback;
        gameplayaudio::Context context;
        gameplaymusic::Owner music;
    };
}

struct gameplay::ManagerServices::State {
    explicit State(const ManagerDependencies& dependencies) : dependencies(dependencies),
        commonResources(dependencies.common), mapResources(dependencies.map), collision(dependencies.mission),
        actionContext(dependencies.executable), actorCollision(dependencies.collisions, dependencies.actors.catalog),
        padSampler(dependencies.executable) {}

    ~State() {
        if (placement) {
            for (const auto* table : placementTables) placement->remove(table);
        }
        placement.reset();
        sender.reset();
        generators.reset();
        webs.reset();
        if (owners) owners->bindUpdates(nullptr);
        if (audio) audio->playback.stop();
        if (enemies) {
            for (unsigned i = static_cast<unsigned>(enemies->slots().size()); i > 0; --i) {
                if (enemies->slots()[i - 1].state == 2) enemies->deactivate(i - 1);
            }
        }
        updates.reset();
        events.reset();
        enemies.reset();
        strategies.reset();
        runtime.reset();
        owners.reset();
        effects.reset();
        models.reset();
        particles.reset();
        actors.reset();
        players.reset();
        if (restored) dependencies.flags.store(dependencies.entry.flags);
    }

    void initialize() {
        if (!dependencies.entry.initialized || dependencies.entry.map != mapMode) {
            throw std::invalid_argument("First-area manager services require the selected map-two save entry.");
        }
        if (!dependencies.playerCount || dependencies.playerCount > dependencies.records.size()) {
            throw std::invalid_argument("Original manager services require one or two players.");
        }
        for (unsigned i = 0; i < dependencies.playerCount; ++i) {
            if (!dependencies.records[i]) throw std::invalid_argument("Original manager services require player records.");
        }
        if (dependencies.records[0] != &dependencies.entry.player) {
            throw std::invalid_argument("Player zero must use the selected entry's persistent record.");
        }
        dependencies.flags.restore(dependencies.entry.flags);
        restored = true;
        std::string error;
        std::vector<std::uint8_t> compressed, archive;
        if (!dependencies.package.read("files/Game/game_usa.szp", compressed, error) ||
            !roomarchive::decompress(compressed, archive, error)) throw std::runtime_error(error);
        auto doorResources = std::make_shared<doors::Resources>();
        if (!doorResources->load(archive, dependencies.executable, error)) throw std::runtime_error(error);
        auto word = [&](std::uint32_t address) {
            std::vector<std::uint8_t> bytes;
            if (!dolresources::read(dependencies.executable, address, 4, bytes, error)) throw std::runtime_error(error);
            return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
        };
        doordata::Table descriptors;
        if (!doordata::read(dependencies.executable, word(word(0x802ff218 + mapMode * 4) + 0x18), descriptors, error)) {
            throw std::runtime_error(error);
        }
        doors::SavedFlags savedDoors{};
        for (unsigned i = 0; i < 32; ++i) savedDoors[i / 4] |= unsigned(dependencies.entry.inventory[i]) << (24 - (i % 4) * 8);
        if (!doors.initialize(descriptors, doorResources, dependencies.executable, savedDoors, error)) throw std::runtime_error(error);
        missionEvents = std::make_unique<gameevent::MissionActivation>(dependencies.mission, dependencies.types,
            dependencies.entry, dependencies.flags);
        const auto* characters = dependencies.mission.getJmpResource("characterinfo");
        if (!characters) throw std::runtime_error("Original manager services require CharacterInfo.");
        const int collisionIndex = dependencies.map.findNameResource("col.mp");
        if (collisionIndex < 0) throw std::runtime_error("Original manager services require map collision.");
        collision.load(dependencies.map.resource(collisionIndex));
        audio = std::make_shared<Audio>(dependencies);
        particles = std::make_unique<particles::Scene>(dependencies.executable, commonResources, dependencies.randomState);
        particles->initializeEffectBatches();
        players = std::make_unique<players::Manager>(dependencies.common, dependencies.materials, mapMode,
            &dependencies.map, playerGlobals, dependencies.motors);
        players->initializeEntries();
        actors = std::make_unique<actors::Registry>(dependencies.executable, dependencies.actors, *players, playerGlobals,
            dependencies.camera, actorGlobals, dependencies.collisions, dependencies.roomFlags, mapMode, dependencies.flags.value(0x3d));
        properties = std::make_unique<charactercreation::Properties>(dependencies.executable, dependencies.creation,
            dependencies.actors.parameters);
        effects = std::make_unique<charactereffects::Resources>(particles->effects(), dependencies.variant);
        owners = std::make_unique<characterowner::CreationRuntime>(*effects, *properties, *dependencies.records[0], lights, audio->owners);
        runtime = std::make_unique<enemies::ManagerRuntime>(dependencies.strategies, *owners, *actors, dependencies.actors.rooms,
            dependencies.visibility, dependencies.roomFlags, dependencies.flags, mapMode, dependencies.randomState);
        enemies = std::make_unique<enemies::Manager>(dependencies.types, dependencies.creation, *runtime,
            dependencies.playerCount, mapMode);
        strategies = std::make_unique<enemystrategy::Factory>(*runtime, dependencies.flags, dependencies.mission);
        owners->bind(*enemies, *strategies);
        effects->bind(*enemies, *actors);
        runtime->bind(*enemies);
        enemies->attachCharacterInfo(*characters, dependencies.appearPoint);
        if (dependencies.prepareRooms) dependencies.prepareRooms(*enemies);
        enemies->initializeActors();
        particles->bind(*enemies, *actors, dependencies.actors.pose, dependencies.visibility);
        particles->initializeElements();
        auto* player = dynamic_cast<playeractor::Player*>(actors->actor(enemies->appearPoint(0).actor));
        if (!player) throw std::runtime_error("Original manager services require Player zero.");
        auto sound = [this](unsigned id, const std::array<float, 3>& position, unsigned parameter) {
            if (parameter) throw std::runtime_error("Positional playback requires the original default sound parameter.");
            audio->playback.play(id, position);
        };
        auto& elements = particles->elemental();
        models = std::make_unique<modeleffects::Manager>(modeleffects::Environment{commonResources, particles->pool(),
            particles->effects(), player->view, elements.pool, elements.ice, elements.water, dependencies.flags,
            collision, dependencies.randomState, sound, {}});
        models->initialize(dependencies.executable, mapMode, mapResources, dependencies.materials);
        effects->bindModels(models->factory);
        players->bindActors(*enemies, *actors, dependencies.records, &dependencies.pad, dependencies.playerContext, dependencies.audioLife);
        players->initializeAuxiliary(dependencies.executable);
        particles->refresh(nullptr);
        events = std::make_unique<playerevents::Dispatcher>(*enemies, *actors);
        capture = std::make_unique<actorcapture::Motion>(dependencies.executable, dependencies.actors.pose,
            [this] { return runtime->random(); });
        updates = std::make_unique<characterowner::UpdateServices>(characterowner::UpdateServices{
            dependencies.actors.catalog, dependencies.actors.resources, dependencies.actors.pose, dependencies.actors.parameters,
            dependencies.creation, collision, *actors, *players, *runtime, models->factory, *particles, sound});
        owners->bindUpdates(updates.get());
        ambient = dependencies.actors.rooms.entries().at(player->room() >> 24).ambient;
        barrierServices = std::make_unique<enemystrategy::BarrierEnvironment>(enemystrategy::BarrierEnvironment{
            doors, models->factory, commonResources, *runtime, player->view});
        fireServices = std::make_unique<enemystrategy::FireGeneratorEnvironment>(enemystrategy::FireGeneratorEnvironment{
            commonResources, particles->pool(), glows, player->view, dependencies.actors.rooms, *runtime});
        strategies->bindPlacement(dependencies.strategies, *barrierServices, *fireServices, ambient);
        generators = std::make_unique<charactergenerator::Manager>(dependencies.types, dependencies.creation,
            dependencies.strategies, *strategies, *enemies, *runtime, dependencies.mission);
        webs = std::make_unique<modeleffects::CobwebPlacements>(models->factory, dependencies.actors.pose);
        sender = std::make_unique<jmp::MessageSender>();
        sender->addReceiver(enemies.get());
        sender->addReceiver(webs.get());
        sender->addReceiver(generators.get());
        roomDependencies = std::make_unique<roomdependencies::Catalog>(dependencies.executable, mapMode);
        placementRuntime = std::make_unique<characterplacement::Runtime>(*enemies, *runtime, *roomDependencies);
        placement = std::make_unique<jmp::EntryActivation>(*sender, *placementRuntime, dependencies.flags, missionEvents->activation());
        for (const char* name : {"ObjInfo", "CharacterInfo", "KeyInfo", "GeneratorInfo", "EnemyInfo", "ObserverInfo"}) {
            const auto* table = dependencies.mission.getJmpResource(name);
            if (placement->add(table)) placementTables.push_back(table);
        }
    }

    ManagerDependencies dependencies;
    unsigned mapMode = 2;
    bool restored = false;
    bool placementFailed = false;
    doors::State doors;
    particles::GlowPool glows;
    std::array<std::uint8_t, 4> ambient{};
    std::unique_ptr<enemystrategy::BarrierEnvironment> barrierServices;
    std::unique_ptr<enemystrategy::FireGeneratorEnvironment> fireServices;
    std::unique_ptr<gameevent::MissionActivation> missionEvents;
    std::unique_ptr<roomdependencies::Catalog> roomDependencies;
    std::unique_ptr<characterplacement::Runtime> placementRuntime;
    std::unique_ptr<jmp::MessageSender> sender;
    std::unique_ptr<charactergenerator::Manager> generators;
    std::unique_ptr<modeleffects::CobwebPlacements> webs;
    std::unique_ptr<jmp::EntryActivation> placement;
    std::vector<const jmp::ToolData*> placementTables;
    gamearchive::Resources commonResources, mapResources;
    mapcollision::State collision;
    playeractions::Context actionContext;
    actorcollision::Manager actorCollision;
    playerinput::PadSampler padSampler;
    roomlighting::Pool lights;
    players::Globals playerGlobals;
    playeractor::Globals actorGlobals;
    std::shared_ptr<Audio> audio;
    std::unique_ptr<players::Manager> players;
    std::unique_ptr<actors::Registry> actors;
    std::unique_ptr<particles::Scene> particles;
    std::unique_ptr<modeleffects::Manager> models;
    std::unique_ptr<charactercreation::Properties> properties;
    std::unique_ptr<charactereffects::Resources> effects;
    std::unique_ptr<characterowner::CreationRuntime> owners;
    std::unique_ptr<enemies::ManagerRuntime> runtime;
    std::unique_ptr<enemystrategy::Factory> strategies;
    std::unique_ptr<enemies::Manager> enemies;
    std::unique_ptr<playerevents::Dispatcher> events;
    std::unique_ptr<actorcapture::Motion> capture;
    std::unique_ptr<characterowner::UpdateServices> updates;
};

gameplay::ManagerServices::ManagerServices(const ManagerDependencies& dependencies) : state(std::make_unique<State>(dependencies)) {
    state->initialize();
}

gameplay::ManagerServices::~ManagerServices() = default;
enemies::Manager& gameplay::ManagerServices::enemies() { return *state->enemies; }
actors::Registry& gameplay::ManagerServices::actors() { return *state->actors; }
players::Manager& gameplay::ManagerServices::players() { return *state->players; }
playerevents::Dispatcher& gameplay::ManagerServices::playerEvents() { return *state->events; }
particles::Scene& gameplay::ManagerServices::particles() { return *state->particles; }
modeleffects::Manager& gameplay::ManagerServices::modelEffects() { return *state->models; }
mapcollision::State& gameplay::ManagerServices::collision() { return state->collision; }
roomlighting::Pool& gameplay::ManagerServices::lights() { return state->lights; }

std::shared_ptr<jaudio::PositionalPlayback> gameplay::ManagerServices::playback() {
    return {state->audio, &state->audio->playback};
}

const gameplayaudio::Context& gameplay::ManagerServices::audioContext() const { return state->audio->context; }
gameplaymusic::Owner& gameplay::ManagerServices::music() { return state->audio->music; }

void gameplay::ManagerServices::updateOwner(unsigned slot) { state->enemies->update(slot); }

playeractions::Input gameplay::ManagerServices::preparePlayerAction(unsigned index) {
    if (index >= state->dependencies.playerCount) throw std::out_of_range("Player action index exceeds the bound player count.");
    auto* player = dynamic_cast<playeractor::Player*>(state->actors->actor(state->enemies->appearPoint(index).actor));
    if (!player) throw std::runtime_error("Player action input requires its registered player owner.");
    return state->actionContext.build(*player, state->collision, state->dependencies.actors.catalog, state->dependencies.actors.resources);
}

void gameplay::ManagerServices::updateGenerators() { state->generators->update(); }
void gameplay::ManagerServices::sampleInput(const playerinput::PadStatus& pad, std::int16_t port) {
    state->dependencies.pad.port = port;
    state->padSampler.update(state->dependencies.pad, pad);
}
void gameplay::ManagerServices::updateContacts() {
    state->actors->updateContacts(*state->enemies, state->actorCollision, *state->players->records,
        state->collision, state->dependencies.collisions, state->dependencies.flags);
    state->players->records->finishContacts();
}
doors::State& gameplay::ManagerServices::doors() { return state->doors; }
charactergenerator::Manager& gameplay::ManagerServices::generators() { return *state->generators; }
particles::GlowPool& gameplay::ManagerServices::glows() { return state->glows; }

void gameplay::ManagerServices::updatePlacement(std::int32_t elapsedSeconds) {
    if (state->placementFailed) throw std::runtime_error("Original placement activation failed; reload the scene before retrying.");
    state->missionEvents->updateAppearance(elapsedSeconds);
    try {
        state->placement->update();
    } catch (...) {
        state->placementFailed = true;
        throw;
    }
}

const jmp::ActiveEntries& gameplay::ManagerServices::placements() const { return state->placement->entries(); }
const gameevent::MissionActivation& gameplay::ManagerServices::missionEvents() const { return *state->missionEvents; }

void gameplay::ManagerServices::updateCaptures(const playermodels::VacuumPath* path, const actorcapture::Vector* suctionDirection,
    actormodel::Matrix& retainedTransform, const actorcapture::Vector* originalScratch, const float* originalRegister,
    const bool* emptyCompletion) {
    state->actors->updateCaptures(*state->capture, path, suctionDirection, retainedTransform, originalScratch, originalRegister, emptyCompletion);
}

void gameplay::ManagerServices::updateAudio(const std::array<float, 12>& listener) {
    state->audio->playback.update(listener);
}
