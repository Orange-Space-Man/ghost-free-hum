#include "resources/dolresources.h"
#include "game/scene/gameplaymanagerservices.h"
#include "game/actors/charactereffectresources.h"
#include "game/enemies/enemymanagerruntime.h"
#include "game/actors/characterplacementruntime.h"
#include "game/enemies/enemygeneratormanager.h"
#include "game/world/cobwebplacementmanager.h"
#include "game/world/candleplacementmanager.h"
#include "game/world/eventeffectplacements.h"
#include "game/enemies/enemybarrierstrategy.h"
#include "game/enemies/enemyfiregeneratorstrategy.h"
#include "resources/roomarchivereader.h"
#include "game/startup/startupresources.h"
#include "game/player/playeractioncontext.h"
#include "game/player/playercaptureframe.h"
#include "game/player/playersuctioncompletion.h"
#include "game/player/playerdamageframe.h"
#include "game/player/playerinputrouting.h"
#include "game/player/playerorientationframe.h"
#include "game/player/playerlightframe.h"
#include "game/player/playerlightcontacts.h"
#include "game/player/playeraudioframe.h"
#include "game/player/playerbreathframe.h"
#include "game/player/playerheadtracking.h"
#include "game/player/playeractionframe.h"
#include "game/player/playerattachedframe.h"
#include "game/player/playerreactioneffects.h"
#include "game/player/playerfurnitureowner.h"
#include "game/world/roomfurnitureresponse.h"
#include "resources/glme01addresses.h"
#include "game/audio/roomambientaudio.h"
#include "game/world/roomstorm.h"
#include "game/enemies/enemyitemstrategy.h"

#include <stdexcept>
#include <cmath>
#include <utility>
#include <cstring>
#include <limits>

namespace {
    class SuctionAudio final : public playerevents::SuctionSounds, public enemystrategy::ItemSounds {
    public:
        explicit SuctionAudio(jaudio::PositionalPlayback& playback) : playback(playback) {}
        void actor(unsigned sound, const itemsuction::Vector& position, unsigned parameter) override {
            if (parameter) throw std::runtime_error("Player suction sound requires the original default parameter.");
            playback.play(sound, position);
        }
        void weapon(unsigned sound, const itemsuction::Vector& position) override { playback.play(sound, position); }
        void play(unsigned sound, const itemsuction::Vector& position, unsigned parameter) override { actor(sound, position, parameter); }

    private:
        jaudio::PositionalPlayback& playback;
    };

    struct Audio {
        Audio(const gameplay::ManagerDependencies& dependencies, const mapcollision::State& collision) :
            context(dependencies.mission, dependencies.flags, dependencies.entry.map),
            music(dependencies.package, dependencies.audio, context, dependencies.audioLife),
            playback(owners, dependencies.executable, dependencies.package, dependencies.audio,
                [this, &rooms = dependencies.actors.rooms, &collision](const std::array<float, 3>& position, bool queryFloor) {
                    std::int16_t polygon = -1;
                    if (queryFloor) {
                        auto probe = position;
                        probe[1] += 5;
                        probe[1] = collision.floorHeight(probe) + 5;
                        polygon = static_cast<std::int16_t>(collision.floorPolygons(probe, 10)[0]);
                    }
                    const std::array<float, 3> rounded{std::ceil(position[0]), std::ceil(position[1]), std::ceil(position[2])};
                    const unsigned room = rooms.resolve(rounded);
                    return context.parameters(room, polygon).packed;
                }), ambience(dependencies.executable, playback) { playback.bindMusic(music); }
        jaudio::PositionOwners owners;
        gameplayaudio::Context context;
        gameplaymusic::Owner music;
        jaudio::PositionalPlayback playback;
        gameplayaudio::RoomAmbience ambience;
        std::array<float, 12> listener{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    };
}

struct gameplay::ManagerServices::State : playerpresentation::Services {
    void effect(unsigned id, const std::array<float, 3>& position) override {
        particles->effects().create(id, position, furnitureDirection);
    }
    bool specialItem(unsigned type) const override { return furnitureAdmission.key(type); }
    void music(unsigned sequence) override {
        if (sequence != 0x8000000e) throw std::runtime_error("Item presentation requires its special fanfare routing.");
        audio->music.requestItem(audioInterfaceFrame);
    }
    void collected(actors::MoveActor& item) override {
        auto* owner = enemies->activeOwner(item);
        if (!owner) throw std::runtime_error("Presented item has no retained owner.");
        owner->event(4);
    }
    void setFlag(unsigned flag) override { dependencies.flags.setOn(static_cast<std::uint8_t>(flag)); }
    bool equipmentOwned() const override {
        return (events->player(0)->components->bits[4]->data[0x1b] & 0x20) != 0;
    }
    void equipmentEvent(unsigned, unsigned, unsigned) override {
        throw std::runtime_error("Item presentation requires the equipped-player transition.");
    }
    void sound(unsigned id, const std::array<float, 3>& position) override { audio->playback.play(id, position); }
    void scene(unsigned scene, std::int32_t) override {
        if (scene == 1 && !events->player(0)->view.field861) return;
        throw std::runtime_error("Collected key requires its original map scene admission.");
    }
    explicit State(const ManagerDependencies& dependencies) : dependencies(dependencies),
        savedDoors(dependencies.entry.inventory.data(), dependencies.entry.inventory.size()),
        commonResources(dependencies.common), mapResources(dependencies.map), collision(dependencies.mission),
        actionContext(dependencies.executable), actorCollision(dependencies.collisions, dependencies.actors.catalog),
        padSampler(dependencies.executable), playerDecoder(dependencies.executable),
        playerCapture(dependencies.executable), playerDamage(dependencies.executable),
        playerOrientation(dependencies.executable, dependencies.actors.pose),
        playerFinish(dependencies.executable, dependencies.actors.pose), playerLight(dependencies.executable),
        lightContacts(dependencies.executable, dependencies.actors.catalog), playerAudio(dependencies.executable),
        playerLate(dependencies.executable), playerMain(dependencies.executable), playerBreath(dependencies.executable), playerHead(dependencies.executable),
        playerAction(dependencies.executable, dependencies.actors.pose), playerAttached(dependencies.executable),
        playerVelocity(dependencies.executable, dependencies.actors.pose),
        ladderPaths(dependencies.mission), furnitureAdmission(dependencies.executable, dependencies.types),
        furnitureResponse(dependencies.executable, furnitureAdmission),
        storm(dependencies.executable, dependencies.mission, dependencies.actors.pose), shadow(dependencies.executable) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        constexpr unsigned furnitureEffectDirection = 0x8021da40;
        if (!dolresources::read(dependencies.executable, furnitureEffectDirection, 12, bytes, error))
            throw std::runtime_error(error);
        for (unsigned i = 0; i < furnitureDirection.size(); ++i) {
            const unsigned bits = (unsigned(bytes[i * 4]) << 24) | (unsigned(bytes[i * 4 + 1]) << 16) |
                (unsigned(bytes[i * 4 + 2]) << 8) | bytes[i * 4 + 3];
            std::memcpy(&furnitureDirection[i], &bits, 4);
        }
    }

    ~State() {
        if (placement) {
            for (const auto* table : placementTables) placement->remove(table);
        }
        placement.reset();
        sender.reset();
        generators.reset();
        candles.reset();
        eventEffects.reset();
        webs.reset();
        if (owners) owners->bindUpdates(nullptr);
        if (audio) audio->playback.stop();
        if (furniture) furniture->bindAudio({});
        if (enemies) {
            for (unsigned i = static_cast<unsigned>(enemies->slots().size()); i > 0; --i) {
                if (enemies->slots()[i - 1].state == 2) enemies->deactivate(i - 1);
            }
        }
        updates.reset();
        playerFurniture.reset();
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
        const auto& rooms = dependencies.actors.rooms.entries();
        if (rooms.size() > roomIds.size()) throw std::runtime_error("Door room table exceeds the original room index range.");
        for (unsigned i = 0; i < rooms.size(); ++i) roomIds[i] = rooms[i].id;
        if (!doors.initialize(descriptors, doorResources, dependencies.executable, savedDoors, error)) throw std::runtime_error(error);
        missionEvents = std::make_unique<gameevent::MissionActivation>(dependencies.mission, dependencies.types,
            dependencies.entry, dependencies.flags);
        const auto* characters = dependencies.mission.getJmpResource("characterinfo");
        if (!characters) throw std::runtime_error("Original manager services require CharacterInfo.");
        const int collisionIndex = dependencies.map.findNameResource("col.mp");
        if (collisionIndex < 0) throw std::runtime_error("Original manager services require map collision.");
        collision.load(dependencies.map.resource(collisionIndex));
        audio = std::make_shared<Audio>(dependencies, collision);
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
        particles->initializeRunningDust(dependencies.mission, dependencies.actors.rooms);
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
        players->bindActors(*enemies, *actors, dependencies.records, &dependencies.pad, &dependencies.camera, dependencies.audioLife);
        players->initializeAuxiliary(dependencies.executable);
        particles->refresh(nullptr);
        elementDuration = static_cast<std::int32_t>(word(glme01::playerElementDuration));
        suctionAudio = std::make_unique<SuctionAudio>(audio->playback);
        suction = std::make_unique<playerevents::SuctionEnvironment>(playerevents::SuctionEnvironment{
            dependencies.actors.catalog, *properties, dependencies.flags, *suctionAudio, elementDuration});
        events = std::make_unique<playerevents::Dispatcher>(*enemies, *actors, nullptr,
            [this](unsigned id, const auto& position) { audio->playback.play(id, position); }, suction.get());
        life = std::make_unique<playerevents::LifeEnvironment>(playerevents::LifeEnvironment{
            dependencies.audioLife, particles->effects(),
            [this](unsigned id, const auto& position) { audio->playback.play(id, position); },
            [this](const auto& position) { audio->playback.playDeathEffects(position); }, {}});
        capture = std::make_unique<actorcapture::Motion>(dependencies.executable, dependencies.actors.pose,
            [this] { return runtime->random(); });
        updates = std::make_unique<characterowner::UpdateServices>(characterowner::UpdateServices{
            dependencies.actors.catalog, dependencies.actors.resources, dependencies.actors.pose, dependencies.actors.parameters,
            dependencies.creation, collision, *actors, *players, *runtime, models->factory, *particles, sound});
        owners->bindUpdates(updates.get());
        itemCollection = std::make_unique<playerevents::ItemEnvironment>(playerevents::ItemEnvironment{
            *actors, *enemies, furnitureAdmission, &itemAward,
            [this](unsigned id, const auto& position) { audio->playback.play(id, position); }});
        itemForce = std::make_unique<itemsuction::Force>(player->view, particles->contexts().player.suction);
        itemEvents = std::make_unique<playerevents::SuctionEvents>(*player, *actors, *suction, [this](actors::MoveActor& actor) {
            auto* owner = dynamic_cast<characterowner::Owner*>(enemies->activeOwner(actor));
            auto* item = owner ? dynamic_cast<enemystrategy::FiniteItem*>(owner->ownership().strategy.get()) : nullptr;
            if (!item) throw std::logic_error("Item collection requires its retained strategy owner.");
            item->collectItem();
        });
        itemSuction = std::make_unique<itemsuction::Environment>(itemsuction::Environment{
            dependencies.actors.rooms, dependencies.visibility, dependencies.actors.pose, particles->contexts().player.suction,
            *itemForce, particles->effects(), dependencies.flags, *itemEvents});
        itemServices = std::make_unique<enemystrategy::ItemEnvironment>(enemystrategy::ItemEnvironment{
            dependencies.common, dependencies.types, dependencies.strategies, collision, rotatingSurfaces,
            dependencies.collisions, *itemCollection, *suction, *itemSuction, *suctionAudio, [player] {
                if (player->view.field8A0 == 1) throw std::runtime_error("Item attraction requires the original suction scratch state.");
                return itemsuction::Vector{};
            }, 0, itemDuration});
        itemServices->globalSound = [this](unsigned id) { audio->playback.playGlobal(id); };
        itemServices->life = life.get();
        itemServices->camera = &dependencies.camera;
        itemServices->audioPositions = &audio->owners;
        strategies->bindItems(itemServices.get());
        ambient = dependencies.actors.rooms.entries().at(player->room() >> 24).ambient;
        barrierServices = std::make_unique<enemystrategy::BarrierEnvironment>(enemystrategy::BarrierEnvironment{
            doors, models->factory, commonResources, *runtime, player->view});
        fireServices = std::make_unique<enemystrategy::FireGeneratorEnvironment>(enemystrategy::FireGeneratorEnvironment{
            commonResources, particles->pool(), glows, player->view, dependencies.actors.rooms, *runtime});
        strategies->bindPlacement(dependencies.strategies, *barrierServices, *fireServices, ambient);
        strategies->bindPose(dependencies.actors.pose);
        generators = std::make_unique<charactergenerator::Manager>(dependencies.types, dependencies.creation,
            dependencies.strategies, *strategies, *enemies, *runtime, dependencies.mission);
        webs = std::make_unique<modeleffects::CobwebPlacements>(models->factory, dependencies.actors.pose);
        candles = std::make_unique<particles::CandlePlacements>(commonResources, particles->pool(), glows, player->view,
            dependencies.actors.rooms, dependencies.visibility, dependencies.randomState,
            [this](unsigned room) { return runtime->roomCleared(room); },
            [this](unsigned id, const auto& position) { audio->playback.play(id, position); });
        sender = std::make_unique<jmp::MessageSender>();
        eventEffects = std::make_unique<particles::EventEffectPlacements>(commonResources, particles->pool());
        sender->addReceiver(enemies.get());
        sender->addReceiver(candles.get());
        sender->addReceiver(eventEffects.get());
        sender->addReceiver(&particles->runningDust());
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
    doors::SavedFlags savedDoors;
    std::array<std::uint32_t, 256> roomIds{};
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
    std::unique_ptr<particles::CandlePlacements> candles;
    std::unique_ptr<particles::EventEffectPlacements> eventEffects;
    std::unique_ptr<jmp::EntryActivation> placement;
    std::vector<const jmp::ToolData*> placementTables;
    gamearchive::Resources commonResources, mapResources;
    mapcollision::State collision;
    playeractions::Context actionContext;
    actorcollision::Manager actorCollision;
    playerinput::PadSampler padSampler;
    playerinput::Decoder playerDecoder;
    playeractor::CaptureFrame playerCapture;
    playerevents::DamageFrame playerDamage;
    playerorientation::Frame playerOrientation;
    playeractor::FinishFrame playerFinish;
    playeractor::LightFrame playerLight;
    playeractor::LightContacts lightContacts;
    playeractor::AudioFrame playerAudio;
    playeractor::LateFrame playerLate;
    playeractor::MainFrame playerMain;
    std::uint8_t playerSpecialActive = 0;
    playeractor::BreathFrame playerBreath;
    playercomponents::HeadFrame playerHead;
    playeractions::Frame playerAction;
    playercomponents::AttachedFrame playerAttached;
    playermotion::VelocityFrame playerVelocity;
    std::unique_ptr<playerinteraction::FurnitureOwner> playerFurniture;
    std::uint32_t audioInterfaceFrame = 0;
    playerorientation::Globals turning;
    ladders::Paths ladderPaths;
    characteritems::Admission furnitureAdmission;
    roomfurniture::Response furnitureResponse;
    roomfurniture::Owner* furniture = nullptr;
    std::array<float, 3> furnitureDirection{};
    std::int32_t elementDuration = 0;
    std::unique_ptr<SuctionAudio> suctionAudio;
    std::unique_ptr<playerevents::SuctionEnvironment> suction;
    roomlighting::Pool lights;
    roomlighting::Storm storm;
    roomshadow::Projector shadow;
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
    std::unique_ptr<playerevents::LifeEnvironment> life;
    std::unique_ptr<actorcapture::Motion> capture;
    std::unique_ptr<characterowner::UpdateServices> updates;
    actors::RotatingSurfaces rotatingSurfaces;
    playerevents::ItemAward itemAward{};
    std::uint8_t itemDuration = 0;
    std::unique_ptr<playerevents::ItemEnvironment> itemCollection;
    std::unique_ptr<itemsuction::Force> itemForce;
    std::unique_ptr<playerevents::SuctionEvents> itemEvents;
    std::unique_ptr<itemsuction::Environment> itemSuction;
    std::unique_ptr<enemystrategy::ItemEnvironment> itemServices;
};

gameplay::ManagerServices::ManagerServices(const ManagerDependencies& dependencies) : state(std::make_unique<State>(dependencies)) {
    state->initialize();
}

gameplay::ManagerServices::~ManagerServices() = default;
enemies::Manager& gameplay::ManagerServices::enemies() { return *state->enemies; }
actors::Registry& gameplay::ManagerServices::actors() { return *state->actors; }
players::Manager& gameplay::ManagerServices::players() { return *state->players; }
playerevents::Dispatcher& gameplay::ManagerServices::playerEvents() { return *state->events; }
playerevents::LifeEnvironment& gameplay::ManagerServices::playerLife() { return *state->life; }
particles::Scene& gameplay::ManagerServices::particles() { return *state->particles; }
modeleffects::Manager& gameplay::ManagerServices::modelEffects() { return *state->models; }
mapcollision::State& gameplay::ManagerServices::collision() { return state->collision; }
s3dshadow::Projection gameplay::ManagerServices::shadowProjection() const {
    return state->shadow.projection(state->events->player(0)->view, state->dependencies.camera);
}

roomlighting::Pool& gameplay::ManagerServices::lights() { return state->lights; }

std::shared_ptr<jaudio::PositionalPlayback> gameplay::ManagerServices::playback() {
    return {state->audio, &state->audio->playback};
}

const gameplayaudio::Context& gameplay::ManagerServices::audioContext() const { return state->audio->context; }
gameplaymusic::Owner& gameplay::ManagerServices::music() { return state->audio->music; }
gameflags::State& gameplay::ManagerServices::flags() { return state->dependencies.flags; }
const playerinput::Pad& gameplay::ManagerServices::controller() const { return state->dependencies.pad; }
bool gameplay::ManagerServices::roomCleared(unsigned room) const { return state->runtime->roomCleared(room); }

bool gameplay::ManagerServices::beginPlayerCapture(std::int32_t index, const std::array<float, 3>& position,
    std::int32_t recipient) {
    auto* player = state->events->player(index);
    return player && state->playerCapture.begin(*player, position, recipient, *state->capture,
        [this](unsigned id, const auto& origin) { state->audio->playback.play(id, origin); });
}

void gameplay::ManagerServices::updatePlayerCapture(std::int32_t index) {
    if (auto* player = state->events->player(index)) state->playerCapture.run(*player, *state->events, state->particles->effects());
}

void gameplay::ManagerServices::updatePlayerLight(unsigned index) {
    if (index >= state->dependencies.playerCount) throw std::out_of_range("Player light index exceeds the bound player count.");
    auto* player = state->events->player(static_cast<std::int32_t>(index));
    if (!player) throw std::runtime_error("Flashlight update requires its registered player owner.");
    const playeractor::LightServices services{state->lights, *state->players->models, *state->players->state,
        [this](unsigned sound, const auto& position) { state->audio->playback.play(sound, position); },
        [this](auto&, const auto& light) {
            state->lightContacts.update(light, *state->enemies, state->dependencies.actors.catalog, state->dependencies.visibility);
        }};
    state->playerLight.update(*player, roomCleared(player->room() >> 24),
        static_cast<std::uint32_t>(state->playerGlobals.field8BF8), state->playerFinish, services, state->dependencies.actors.pose);
}

bool gameplay::ManagerServices::releasePlayerCapture(std::int32_t index, const std::array<float, 3>* position,
    const std::array<float, 3>* direction, bool launched) {
    auto* player = state->events->player(index);
    return player && state->playerCapture.release(*player, position, direction, launched, state->dependencies.actors.pose);
}

void gameplay::ManagerServices::updateOwner(unsigned slot) { state->enemies->update(slot); }

void gameplay::ManagerServices::updatePoses() {
    state->actors->updatePoses([&] {
        for (const auto* actor : state->actors->entries()) {
            if (actor && (actor->flags & 0x80000))
                throw std::runtime_error("Live captured-actor poses require the original capture path binding.");
        }
    }, [&](actors::MoveActor& actor) {
        actor.updateAnimationSound(state->audio->playback.animationServices(actor.position));
    });
}

playeractions::Input gameplay::ManagerServices::preparePlayerAction(unsigned index) {
    if (index >= state->dependencies.playerCount) throw std::out_of_range("Player action index exceeds the bound player count.");
    auto* player = dynamic_cast<playeractor::Player*>(state->actors->actor(state->enemies->appearPoint(index).actor));
    if (!player) throw std::runtime_error("Player action input requires its registered player owner.");
    return state->actionContext.build(*player, state->collision, state->dependencies.actors.catalog, state->dependencies.actors.resources);
}

void gameplay::ManagerServices::updateGenerators() { state->generators->update(); }
playerinteraction::DoorServices gameplay::ManagerServices::preparePlayerDoor(unsigned index) {
    if (index >= state->dependencies.playerCount) throw std::out_of_range("Player door index exceeds the bound player count.");
    auto* player = dynamic_cast<playeractor::Player*>(state->actors->actor(state->enemies->appearPoint(index).actor));
    if (!player) throw std::runtime_error("Door interaction requires its registered player owner.");
    return {state->doors, state->savedDoors, *player->components->flags, state->roomIds, *state->enemies,
        state->dependencies.actors.catalog, state->dependencies.actors.resources, state->dependencies.flags.value(0x53) == 1};
}
playerinteraction::LadderServices gameplay::ManagerServices::preparePlayerLadder(unsigned index) {
    if (index >= state->dependencies.playerCount) throw std::out_of_range("Player ladder index exceeds the bound player count.");
    auto* player = dynamic_cast<playeractor::Player*>(state->actors->actor(state->enemies->appearPoint(index).actor));
    if (!player) throw std::runtime_error("Ladder interaction requires its registered player owner.");
    return {state->ladderPaths, state->dependencies.actors.catalog, state->dependencies.actors.resources};
}

void gameplay::ManagerServices::sampleInput(const playerinput::PadStatus& pad, std::int16_t port) {
    state->dependencies.pad.port = port;
    state->padSampler.update(state->dependencies.pad, pad);
}

playerenvironment::Services gameplay::ManagerServices::preparePlayerEnvironment(unsigned index) {
    if (index >= state->dependencies.playerCount || !state->events->player(static_cast<std::int32_t>(index)))
        throw std::out_of_range("Player environment requires a registered player owner.");
    const auto& actor = state->dependencies.actors;
    return {state->collision, actor.catalog, actor.resources, actor.pose, *state->enemies, *state->players->entries,
        [](auto&) { throw std::runtime_error("Player type changes require the original model-replacement binding."); },
        [](const auto&) -> playeractor::Target* { throw std::runtime_error("Captured-player targeting requires its original capture-owner binding."); },
        [this](unsigned room) { return roomCleared(room); },
        [](bool) { throw std::runtime_error("Captured-player audio requires its original capture-state binding."); },
        [this](unsigned room) { state->audio->music.notifyRoom(room, state->mapMode); }};
}

void gameplay::ManagerServices::updatePlayerInput(unsigned index, bool inputAvailable,
    const std::function<void(playeractor::Player&, bool)>& scannerTransition) {
    if (index >= state->dependencies.playerCount) throw std::out_of_range("Player input index exceeds the bound player count.");
    auto* player = state->events->player(static_cast<std::int32_t>(index));
    if (!player) throw std::runtime_error("Player input requires its registered owner.");
    playerinput::route(*player, state->playerDecoder,
        {state->dependencies.camera, inputAvailable, state->mapMode, scannerTransition});
}

void gameplay::ManagerServices::updatePlayerOrientation(unsigned index,
    const playercomponents::AttachedOutput& attachment, unsigned frame) {
    if (index >= state->dependencies.playerCount) throw std::out_of_range("Player orientation index exceeds the bound player count.");
    auto* player = state->events->player(static_cast<std::int32_t>(index));
    if (!player) throw std::runtime_error("Player orientation requires its registered owner.");
    state->playerOrientation.run(*player,
        {attachment.limits[1] != 0, state->turning.damping, state->turning.scannerDamping, frame});
}

void gameplay::ManagerServices::updatePlayerMotion(unsigned index, const PlayerMotionServices& services) {
    if (index >= state->dependencies.playerCount) throw std::out_of_range("Player motion index exceeds the bound player count.");
    auto* player = state->events->player(static_cast<std::int32_t>(index));
    if (!player) throw std::runtime_error("Player motion requires its registered owner.");
    const auto& actor = state->dependencies.actors;
    const auto sound = [this](unsigned id, const auto& position) { state->audio->playback.play(id, position); };
    state->playerAction.run(*player, state->collision,
        {state->dependencies.flags, actor.catalog, actor.resources, services.globalSound, sound});
    const auto attachment = state->playerAttached.run(*player, actor.catalog, actor.resources, actor.pose);
    const auto& action = *player->components->actions;
    const auto& upper = *player->components->attached;
    if (player->state.flags792 & 12) {
        player->components->weapon->update(state->dependencies.executable, *player, player->state.field7A4, player->state.field7A8,
            action.field60, upper.field248, upper.field2C0, action.query74544(), player->view.field8C0, actor.pose,
            [this] { return state->runtime->random(); });
    }
    state->playerVelocity.run(*player, state->collision, services.platforms, sound);
    state->playerVelocity.constrain(*player);
    updatePlayerOrientation(index, attachment, services.frame);
    playeractor::updateReactionEffects(*player, actor.catalog, actor.resources,
        state->particles->effects(), state->models->factory, sound);
    state->playerFinish.run(*player, preparePlayerFinish(index, services.life, services.doubledDamage));
}

void gameplay::ManagerServices::updatePlayerMain(unsigned index, const PlayerMainServices& services) {
    if (index >= state->dependencies.playerCount) throw std::out_of_range("Player main index exceeds the bound player count.");
    auto* player = state->events->player(static_cast<std::int32_t>(index));
    if (!player || !state->playerFurniture) throw std::logic_error("Player main requires its registered actor and furniture services.");
    if ((player->flags & 0x70) || player->fieldF4) return;
    if (&services.timers.life != state->life.get()) throw std::invalid_argument("Player main requires the scene-owned health environment.");
    unsigned ownerSlot = 0, ownerCount = 0;
    const auto& slots = state->enemies->slots();
    for (unsigned slot = 0; slot < slots.size(); ++slot) {
        if (slots[slot].state == 2 && slots[slot].owner && &slots[slot].owner->actor() == player) {
            ownerSlot = slot;
            ++ownerCount;
        }
    }
    if (ownerCount != 1) throw std::logic_error("Player main requires exactly one active character owner.");
    auto door = preparePlayerDoor(index);
    door.keyPresentationActive = services.keyPresentationActive;
    door.presentKey = services.presentKey;
    const auto globalSound = [this](unsigned id) { state->audio->playback.playGlobal(id); };
    playerinteraction::DoorFrameServices doorFrame{door, *state->actors, state->dependencies.flags, *state->life,
        services.timers.doubledDamage, globalSound};
    playeractor::MainFrameServices frame{preparePlayerEnvironment(index), doorFrame, preparePlayerLadder(index),
        services.presentation ? services.presentation : state.get(), services.scanner, *state->events, services.timers, state->playerSpecialActive};
    frame.owner = [this, ownerSlot] { updateOwner(ownerSlot); };
    frame.input = [this, index, &services] { updatePlayerInput(index, services.inputAvailable, services.scannerTransition); };
    frame.furniture = [this, index] { updatePlayerFurniture(index); };
    frame.motion = [this, index, &services, &globalSound] {
        updatePlayerMotion(index, {*state->life, services.timers.doubledDamage, services.timers.frame, services.platforms, globalSound});
    };
    frame.sound = [this](unsigned id, const auto& position) { state->audio->playback.play(id, position); };
    state->playerMain.run(*player, frame);
}

playeractor::FinishServices gameplay::ManagerServices::preparePlayerFinish(unsigned index,
    playerevents::LifeEnvironment& life, bool doubledDamage) {
    if (index >= state->dependencies.playerCount) throw std::out_of_range("Player completion index exceeds the bound player count.");
    auto* player = state->events->player(static_cast<std::int32_t>(index));
    if (!player) throw std::runtime_error("Player frame completion requires its registered owner.");
    return {state->dependencies.actors.catalog, state->dependencies.actors.resources,
        [this, player, &life, doubledDamage](auto& recipient, const auto& payload) {
            if (&recipient != player) throw std::logic_error("Player completion services used with another actor.");
            state->playerDamage.run(recipient, payload, false, *state->actors,
                state->dependencies.actors.catalog, doubledDamage, life);
        },
        [this](bool active) {
            state->generators->enabled = !active;
            state->particles->pool().setPaused(active);
            state->models->setPaused(active);
            state->actors->setInteractionPaused(active);
        },
        [this, player, index](auto& recipient) {
            if (&recipient != player) throw std::logic_error("Player completion services used with another actor.");
            state->events->send(static_cast<std::int32_t>(index), 0x3a);
        }, [this](actors::MoveActor& target) {
            for (const auto& slot : state->enemies->slots()) {
                if (slot.state != 2 || &slot.owner->actor() != &target) continue;
                auto* owner = dynamic_cast<characterowner::Owner*>(slot.owner.get());
                if (!owner) throw std::runtime_error("Player interaction target requires its original character owner.");
                owner->ownership().resumeAttachments();
                return;
            }
            throw std::runtime_error("Player interaction target is absent from the active character manager.");
        }};
}
void gameplay::ManagerServices::updateContacts() {
    state->actors->updateContacts(*state->enemies, state->actorCollision, *state->players->records,
        state->collision, state->dependencies.collisions, state->dependencies.flags);
    state->players->records->finishContacts();
}
doors::State& gameplay::ManagerServices::doors() { return state->doors; }
charactergenerator::Manager& gameplay::ManagerServices::generators() { return *state->generators; }
particles::GlowPool& gameplay::ManagerServices::glows() { return state->glows; }
unsigned gameplay::ManagerServices::candleCount() const { return state->candles->count(); }

void gameplay::ManagerServices::updatePlacement(std::int32_t elapsedSeconds, bool force) {
    if (state->placementFailed) throw std::runtime_error("Original placement activation failed; reload the scene before retrying.");
    state->missionEvents->updateAppearance(elapsedSeconds);
    bool roomChanged = false;
    for (unsigned i = 0; i < state->dependencies.playerCount; ++i)
        roomChanged |= state->events->player(i)->view.fieldF2C != 0;
    if (!force && !roomChanged) return;
    try {
        state->placement->update();
        if (state->dependencies.updateActorResources)
            state->dependencies.updateActorResources(state->enemies->resourceMask());
    } catch (...) {
        state->placementFailed = true;
        throw;
    }
}

const jmp::ActiveEntries& gameplay::ManagerServices::placements() const { return state->placement->entries(); }
const gameevent::MissionActivation& gameplay::ManagerServices::missionEvents() const { return *state->missionEvents; }
gameevent::MissionActivation& gameplay::ManagerServices::missionEvents() { return *state->missionEvents; }

void gameplay::ManagerServices::updateCaptures(const playermodels::VacuumPath* path, const actorcapture::Vector* suctionDirection,
    actormodel::Matrix& retainedTransform, const actorcapture::Vector* originalScratch, const float* originalRegister,
    const bool* emptyCompletion) {
    state->actors->updateCaptures(*state->capture, path, suctionDirection, retainedTransform, originalScratch, originalRegister, emptyCompletion);
}

void gameplay::ManagerServices::updateAudio(const std::array<float, 12>& listener) {
    state->audio->listener = listener;
    state->audio->playback.update(listener);
}

void gameplay::ManagerServices::updateShadow() {
    const auto* player = state->events->player(0);
    if (!player) throw std::runtime_error("Shadow projection requires the manager-owned player.");
    const auto& dependencies = state->dependencies;
    state->shadow.selectPlayerLight(player->view);
    state->shadow.update(dependencies.actors.rooms.entries().at(player->room() >> 24),
        dependencies.roomFlags.at(player->room() >> 24), dependencies.camera, player->position[1], dependencies.actors.pose);
}

void gameplay::ManagerServices::updateStorm() {
    const auto* player = state->events->player(0);
    if (!player) throw std::runtime_error("Room storm requires the manager-owned player.");
    const auto& dependencies = state->dependencies;
    updateShadow();
    const auto strikes = state->storm.strikes();
    state->storm.update(player->room(), dependencies.actors.rooms.entries().at(player->room() >> 24),
        dependencies.roomFlags.at(player->room() >> 24), dependencies.camera, player->position[1], state->lights,
        [this] { return state->runtime->random(); }, [this] { state->audio->playback.playThunder(); });
    if (state->storm.strikes() != strikes) state->shadow.strike(state->storm.setting(player->room()), dependencies.actors.pose);
}

void gameplay::ManagerServices::setAudioInterfaceFrame(std::uint32_t frame) { state->audioInterfaceFrame = frame; }
const std::array<std::uint32_t, 2>& gameplay::ManagerServices::playerFootstepFrames() const { return state->playerAudio.footsteps; }

void gameplay::ManagerServices::updatePlayerLate(unsigned index, const playeractor::LateFrameServices& services) {
    if (index >= state->dependencies.playerCount) throw std::out_of_range("Player late-frame index exceeds the bound player count.");
    auto* player = state->events->player(static_cast<std::int32_t>(index));
    if (!player) throw std::runtime_error("Player late frame requires its registered owner.");
    state->playerLate.begin(*player, services);
    const auto& actor = state->dependencies.actors;
    const auto& actions = *player->components->actions;
    const float scale = state->playerFinish.bodyScale(player->state);
    if (!((player->state.field7A4->flags | player->state.field7A8->flags) & 0xc000)) {
        if (!services.pipe) throw std::runtime_error("Player late frame requires the visible vacuum pipe update.");
        services.pipe(*player, scale, !(actions.field310 == 2 && actions.field314 == 0x20),
            !player->state.flags134C[0], player->state.field1338 == 4);
    }
    if (!(player->flags & 0x40)) updatePlayerAudio(index);
    state->playerBreath.run(*player, actor.catalog, actor.resources, roomCleared(player->room() >> 24));
    state->playerHead.run(*player, *state->actors, *state->enemies, actor.catalog, actor.pose);
    updatePlayerLight(index);
    player->components->attached->finish(player->state.fieldF6C, actor.catalog, actor.resources,
        [this](unsigned sound, const auto& position) { state->audio->playback.play(sound, position); },
        state->audio->playback.animationServices(player->position));
    auto retained = services;
    if (!retained.sheets && state->playerFurniture) {
        retained.sheets = [this](auto&) { state->playerFurniture->updateSheets(); };
    }
    state->playerLate.finish(*player, *state->players->models, retained);
}

void gameplay::ManagerServices::bindPlayerFurniture(roomfurniture::Owner& furniture) {
    if (state->playerFurniture) throw std::logic_error("Player furniture services are already bound.");
    auto* player = state->events->player(0);
    if (!player) throw std::runtime_error("Player furniture requires its registered owner.");
    state->playerFurniture = std::make_unique<playerinteraction::FurnitureOwner>(*player, *state->events,
        *state->players->models, furniture, state->dependencies.flags, state->audio->playback,
        state->particles->effects(), state->dependencies.executable);
    state->furniture = &furniture;
    furniture.bindAudio([this](const auto& position) { return state->audio->playback.animationServices(position); });
}

void gameplay::ManagerServices::updateFurniture(unsigned frame) {
    if (!state->furniture) throw std::logic_error("Furniture update requires its room-owned pool.");
    auto* player = state->events->player(0);
    if (!player) throw std::logic_error("Furniture update requires player zero.");
    if (state->dependencies.flags.value(1) || player->state.field1180 == 1)
        throw std::runtime_error("Equipped furniture interaction requires its original vacuum and animated-trigger services.");
    roomfurniture::ResponseServices services;
    services.roomCleared = [this](unsigned room) { return roomCleared(room); };
    services.selectItem = [this, player](unsigned table) {
        auto item = characteritems::select(state->dependencies.mission, static_cast<std::int32_t>(table),
            static_cast<std::int16_t>(player->life), *state->runtime);
        if (!state->furnitureAdmission.nothing(item))
            throw std::runtime_error("Furniture contents require their original creation and scatter services.");
        return item;
    };
    services.effect = [this](unsigned type, const auto& position) {
        state->particles->effects().create(type, position, state->furnitureDirection);
    };
    services.sound = [this](unsigned sound, const auto& position, const float* amplitude) {
        if (amplitude && sound != 0x1868)
            throw std::runtime_error("Furniture suction sound requires its original amplitude binding.");
        auto* handle = state->audio->playback.play(sound, position);
        if (handle && amplitude) state->audio->playback.setInteractionVolume(*handle, *amplitude);
    };
    services.animation = [this](auto& object, unsigned animation) { state->furniture->selectAnimation(object.row, animation); };
    services.releaseOccupant = [](unsigned, const auto&) {
        throw std::runtime_error("Occupied furniture requires its original actor-release route.");
    };
    services.scanMessage = [](unsigned) {
        throw std::runtime_error("Furniture scanning requires its original message scene.");
    };
    state->furniture->update(state->dependencies.visibility, 0, [&](auto& object, auto& row, bool visible) {
        if (object.behavior == 17) {
            state->furnitureResponse.rotating(object, row, visible, false, services);
            return true;
        }
        if (object.behavior != 0 && object.behavior != 15 && object.behavior != 22) return false;
        state->furnitureResponse.basic(object, row, visible, false, static_cast<unsigned>(object.animations.size()), frame, services);
        return true;
    });
}

void gameplay::ManagerServices::updateDoors(unsigned frame) {
    const auto* player = state->events->player(0);
    if (!player) throw std::logic_error("Door update requires player zero.");
    doors::UpdateServices services;
    services.positionalSound = [this](unsigned id, const auto& position) { state->audio->playback.play(id, position); };
    services.globalSound = [this](unsigned id) { state->audio->playback.playGlobal(id); };
    services.playerInfluence = [this, player](float radius, const auto& position) {
        const auto& view = player->view;
        if (view.field89E != 1 || view.field8A0 != 1 || !view.field8B4) return 0;
        std::array<float, 3> direction;
        if (!view.field8B4->intersects(radius, direction, position)) return 0;
        const auto& context = state->particles->contexts().player.suction;
        if (!context.position) throw std::logic_error("Door attraction requires the retained player context.");
        const float x = position[0] - (*context.position)[0];
        const float y = position[1] - (*context.position)[1];
        const float z = position[2] - (*context.position)[2];
        constexpr float attractionDistance = 480, influenceScale = 100;
        const float fraction = static_cast<float>(double(std::sqrt(std::fma(z, z, x * x) + y * y)) / attractionDistance);
        const float strength = influenceScale * ((1 - (fraction > 1 ? 1 : fraction)) * context.strength);
        if (!std::isfinite(strength) || strength < -2147483648.0 || strength >= 2147483648.0)
            return std::numeric_limits<std::int32_t>::min();
        return static_cast<int>(strength);
    };
    services.influenceSound = [this](float amount, std::uint8_t material, const auto& position) {
        constexpr unsigned materialSounds = 0x8038eb8c;
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(state->dependencies.executable, materialSounds + (material > 25 ? 0 : material) * 4, 4, bytes, error))
            throw std::runtime_error(error);
        const auto id = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
        if (auto* sound = state->audio->playback.play(id, position)) state->audio->playback.setInteractionVolume(*sound, amount);
    };
    state->doors.update(state->dependencies.entry.map, frame, state->dependencies.flags, state->savedDoors, services);
}

void gameplay::ManagerServices::completePlayerFrame(const playeractor::LateFrameServices& services, bool resetLights) {
    if (resetLights) state->lights.beginFrame();
    for (unsigned index = 0; index < state->dependencies.playerCount; ++index) {
        updatePlayerLate(index, services);
        state->playerLight.publishShadow(*state->events->player(static_cast<std::int32_t>(index)), state->lights, state->playerFinish);
    }
    state->players->state->update(state->lights);
    state->players->advanceClock();
}

void gameplay::ManagerServices::updatePlayerFurniture(unsigned index) {
    if (index != 0) throw std::out_of_range("Player furniture requires player zero.");
    if (!state->playerFurniture) throw std::logic_error("Player furniture services have not been bound.");
    const auto& dependencies = state->dependencies;
    state->playerFurniture->update(dependencies.actors.rooms, *state->enemies, dependencies.actors.catalog,
        state->actorCollision, state->collision, dependencies.entry.map, [this] { return state->runtime->random(); });
}

void gameplay::ManagerServices::updatePlayerAudio(unsigned index) {
    if (index >= state->dependencies.playerCount) throw std::out_of_range("Player audio index exceeds the bound player count.");
    auto* player = state->events->player(static_cast<std::int32_t>(index));
    if (!player) throw std::runtime_error("Player audio requires its registered owner.");
    state->playerAudio.run(*player, {state->dependencies.actors.catalog, state->dependencies.actors.resources,
        state->collision, state->audio->playback, state->audioInterfaceFrame,
        [this](unsigned room, std::int16_t polygon) {
            state->audio->playback.setPlayerContext(state->audio->context.parameters(room, polygon).packed);
            state->audio->music.setRoom(room, polygon);
            state->audio->ambience.update(state->audio->context.parameters(room, polygon), state->audio->listener);
        }, [this](unsigned sound) { return state->audio->music.currentSound() == sound; },
        [this, player](unsigned preset) { return player->state.field7A0->repeats(state->dependencies.executable, preset); }});
}

bool gameplay::ManagerServices::eventEffect(std::string_view placement, std::string_view command) {
    for (const auto* table : state->placementTables) {
        const auto field = table->searchItemInfo("CodeName");
        if (field < 0) continue;
        const auto row = table->findEntryByValue(field, placement);
        if (row < 0) continue;
        jmp::ToolDataRef entry{table, row};
        const std::string text(command);
        return state->sender->dispatch(&jmp::MessageReceiver::message, entry, text.c_str());
    }
    return false;
}
