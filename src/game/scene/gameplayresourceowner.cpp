#include "resources/dolresources.h"
#include "game/scene/gameplayresourceowner.h"
#include "resources/roomarchivereader.h"
#include "game/player/playeractor.h"
#include "game/scene/gameplaycamerafollow.h"
#include "game/startup/startupresources.h"
#include "game/events/entranceeventowner.h"

#include <stdexcept>
#include <utility>
#include <cstring>
#include <algorithm>

struct gameplay::ResourceOwner::State {
    explicit State(gameentry::State& entry) : entry(entry), motors(motorOutput) {}

    void initialize(const std::filesystem::path& path) {
        if (!entry.initialized || (entry.map != 1 && entry.map != 2 && entry.map != 3 && entry.map != 6)) throw std::invalid_argument("Gameplay resources require a supported selected map entry.");
        std::string error;
        std::vector<std::uint8_t> compressed, commonBytes, mapBytes;
        const auto require = [&](bool result, const char* stage) {
            if (!result) throw std::runtime_error(std::string(stage) + ": " + error);
        };
        require(package.open(path, error), "package");
        require(package.read("sys/main.dol", executable, error), "executable");
        require(package.read(gameassets::commonArchive(package.gameId()), compressed, error), "common archive");
        require(roomarchive::decompress(compressed, commonBytes, error), "common decompression");
        require(common.read(commonBytes, error), "common contents");
        require(package.read("files/Map/map" + std::to_string(entry.map) + ".szp", compressed, error), "map archive");
        require(roomarchive::decompress(compressed, mapBytes, error), "map decompression");
        require(map.read(mapBytes, error), "map contents");
        require(mission.read(mapBytes, error), "mission contents");
        require(types.read(executable, error), "enemy types");
        require(creation.read(executable, error), "enemy creation");
        require(strategies.read(executable, error), "enemy strategies");
        require(modelCatalog.read(executable, error), "actor models");
        require(materials.read(executable, error), "material defaults");
        require(parameters.loadParameters(executable, commonBytes, error), "enemy parameters");
        require(jaudio::read(package, audio, error), "audio resources");
        const auto stage = [](const char* name, auto action) {
            try {
                action();
            } catch (const std::exception& exception) {
                throw std::runtime_error(std::string(name) + ": " + exception.what());
            }
        };
        models = std::make_unique<actormodel::Resources>(modelCatalog);
        stage("embedded models", [&] { models->initializeEmbedded(common); });
        stage("initial actor pose", [&] { pose = std::make_unique<actormodel::InitialPose>(executable); });
        stage("camera projection", [&] { cameraProjection = std::make_unique<gameplaycamera::Projection>(executable); });
        stage("camera follow", [&] { cameraFollow = std::make_unique<gameplaycamera::Follow>(executable, entry.map); });
        stage("room locations", [&] { rooms = std::make_unique<roomlocation::Rooms>(executable, entry.map); });
        stage("room mirrors", [&] { mirror = std::make_unique<roommirror::Owner>(executable, *rooms, entry.map); });
        stage("actor collision", [&] { collisions = std::make_unique<actorcollision::Catalog>(executable, modelCatalog); });
        construction = std::make_unique<actors::Construction>(actors::Construction{
            modelCatalog, *models, parameters, *pose, actorGlobals, *rooms});
        visibility.initialize(static_cast<unsigned>(rooms->entries().size()));
        roomFlags.resize(rooms->entries().size());
        for (unsigned i = 0; i < rooms->entries().size(); ++i) {
            if (entry.map == 1 || entry.map == 3 || entry.map == 6) {
                roomFlags[i] = 0x8008;
                continue;
            }
            const auto number = rooms->entries()[i].id & 255;
            const auto offset = 0xa0 + number * 2;
            const unsigned saved = (unsigned(entry.inventory.at(offset)) << 8) | entry.inventory.at(offset + 1);
            auto& value = roomFlags[i];
            if (saved & 1) value |= 2;
            if (saved & 2) value |= 0x8008;
            if (saved & 4) value |= 0x20;
            if (saved & 8) value |= 0x40;
            if (number == 0x14) value |= 0x10;
            if (number == 0x31 || number == 0x33) value |= 4;
            for (unsigned room : {7u, 18u, 53u, 6u}) if (number == room) value |= 0x100;
            for (unsigned room : {44u, 43u, 26u, 64u, 31u}) if (number == room) value |= 0x200;
        }
        ManagerDependencies dependencies{executable, package, audio, common, map, mission, materials, *construction,
            *collisions, types, creation, strategies, *cameras.cameras[0], roomFlags, visibility, entry, flags,
            motors, pad, {&entry.player, &secondRecord}, randomState, audioLife};
        stage("resident rooms", [&] {
            residentRooms = std::make_unique<roomresidency::InitialRooms>(executable, entry.map, *rooms);
        });
        dependencies.prepareRooms = [this](const enemies::Manager& manager) {
            auto position = manager.appearPoint(0).position;
            std::vector<std::uint8_t> bytes;
            std::string error;
            if (!dolresources::read(executable, 0x804d9ab8, 4, bytes, error)) throw std::runtime_error(error);
            const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
            float height;
            std::memcpy(&height, &bits, 4);
            position[1] += height;
            const auto room = rooms->resolve(position, 0);
            const auto* furniture = mission.getJmpResource("FurnitureInfo");
            if (!furniture) throw std::runtime_error("Original room loading requires FurnitureInfo.");
            furnitureOwner = std::make_unique<roomfurniture::Owner>(executable, *rooms, *furniture, entry.inventory, entry.map);
            residentRooms->load(package, {room}, *furniture, flags);
            for (const auto& resident : residentRooms->slots()) {
                if (resident.room != 0xffffffff) furnitureOwner->create(resident.furniture, *furniture);
            }
        };
        dependencies.updateActorResources = [this](const std::array<std::uint8_t, 262>& mask) {
            models->update(mask, [this](std::string_view path) {
                std::vector<std::uint8_t> compressed, bytes;
                std::string error;
                if (!package.read("files" + std::string(path), compressed, error) ||
                    !roomarchive::decompress(compressed, bytes, error)) throw std::runtime_error(error);
                return bytes;
            });
        };
        dependencies.beginChestReveal = [this](const std::array<float, 3>& position) {
            cameraFollow->beginChestReveal(*cameras.cameras[0], position);
        };
        stage("manager services", [&] { managers = std::make_unique<ManagerServices>(dependencies); });
        managers->bindPlayerFurniture(*furnitureOwner);
        auto* player = managers->actors().actor(managers->enemies().appearPoint(0).actor);
        if (!player) throw std::runtime_error("Gameplay camera requires the manager-owned player.");
        gameplayspawn::Spawn spawn;
        spawn.position = player->position;
        spawn.rotation = player->rotation;
        spawn.roomId = player->room();
        cameraRoom = spawn.roomId;
        roomTransition.initialize(spawn.roomId);
        if (!gameplaycamera::initialize(executable, spawn, camera, error, entry.map)) throw std::runtime_error(error);
        auto& owner = *cameras.cameras[0];
        owner.position = camera.position;
        owner.target = camera.target;
        owner.fieldOfView = camera.fieldOfView;
        owner.aspect = camera.aspect;
        owner.nearPlane = camera.nearPlane;
        owner.farPlane = camera.farPlane;
        std::vector<unsigned> hudRooms;
        for (const auto& resident : residentRooms->slots())
            if (resident.room != 0xffffffff) hudRooms.push_back(resident.room);
        stage("gameplay HUD", [&] { hud = std::make_unique<Hud>(common, executable, hudRooms, entry.map, &package); });
        stage("message font", [&] { if (!messageFont.readExecutable(executable, error)) throw std::runtime_error(error); });
        stage("message graphics", [&] {
            messageGraphics = std::make_unique<gameevent::MessageGraphics>(common, executable, messageFont);
        });
        stage("message windows", [&] { messages = std::make_unique<gameevent::MessageWindows>(executable, gameevent::MessageWindows::Services{
            [this](const std::string& text) { return messageGraphics->measure(text); },
            [this](unsigned sound) { managers->playback()->playGlobal(sound); },
            [this](bool active) { managers->music().setCutsceneActive(active); }}); });
        if (entry.map == 1) managers->music().requestLab(1);
        if (entry.map == 3) managers->music().requestTraining(1);
        if (entry.map == 6) managers->music().requestSequence(0x8000000f, 1);
        if (entry.map == 2 && flags.value(1)) {
            auto& music = managers->music();
            music.setRoom(player->room(), -1);
            music.syncRoomLight((roomFlags.at(player->room() >> 24) & 0x8000) != 0, flags.value(0x3d));
            music.start();
        }
        stage("entrance events", [&] {
            entrance = std::make_unique<gameevent::EntranceOwner>(package, types, entry, *rooms,
                *managers, owner, executable, *messages, *cameraFollow);
        });
        managers->bindScannerMessage([this](unsigned count) { entrance->startFurnitureMessage(count); });
        if (entry.map == 6) managers->bindProfessorInteraction([this] { entrance->startGalleryConversation(); });
        if (entry.map == 2) managers->bindToadInteraction([this] { entrance->startToadConversation(); });
        entrance->bindRoomLight([this](bool enabled) { managers->setRoomLight(cameraRoom >> 24, enabled); });
        entrance->bindRandom([this] {
            randomState = randomState * 0x19660d + 0x3c6ef35f; // 80009638, the shared game RNG
            return randomState;
        });
    }

    gameentry::State& entry;
    std::unique_ptr<gameplaycamera::Follow> cameraFollow;
    unsigned cameraRoom = 0;
    gameassets::Package package;
    std::vector<std::uint8_t> executable;
    gamearchive::Archive common, map;
    std::unique_ptr<Hud> hud;
    mission::Resources mission;
    jaudio::Resources audio;
    enemytypes::Catalog types;
    enemycreation::Catalog creation;
    enemystrategy::Catalog strategies;
    actormodel::Catalog modelCatalog;
    j3dmodel::MaterialDefaults materials;
    enemytypes::Manager parameters;
    actors::Globals actorGlobals;
    gameplaycamera::Owners cameras;
    s3denvironment::Camera camera;
    std::unique_ptr<gameplaycamera::Projection> cameraProjection;
    gameflags::State flags;
    playerinput::WindowsMotorOutput motorOutput;
    playerinput::MotorState motors;
    playerinput::Pad pad;
    playerrecord::Record secondRecord{};
    std::uint32_t randomState = 0;
    std::int32_t audioLife = 0;
    std::vector<std::uint32_t> roomFlags;
    roomtraversal::State visibility;
    roomtraversal::Transition roomTransition;
    std::unique_ptr<actormodel::Resources> models;
    std::unique_ptr<actormodel::InitialPose> pose;
    std::unique_ptr<roomlocation::Rooms> rooms;
    std::unique_ptr<roommirror::Owner> mirror;
    std::unique_ptr<actorcollision::Catalog> collisions;
    std::unique_ptr<actors::Construction> construction;
    std::unique_ptr<roomresidency::InitialRooms> residentRooms;
    std::unique_ptr<roomfurniture::Owner> furnitureOwner;
    std::unique_ptr<ManagerServices> managers;
    gamefont::Font messageFont;
    std::unique_ptr<gameevent::MessageGraphics> messageGraphics;
    std::unique_ptr<gameevent::MessageWindows> messages;
    std::unique_ptr<gameevent::EntranceOwner> entrance;
};

gameplay::ResourceOwner::ResourceOwner(const std::filesystem::path& path, gameentry::State& entry) : state(std::make_unique<State>(entry)) {
    state->initialize(path);
}

gameplay::ResourceOwner::~ResourceOwner() = default;
gameplay::ManagerServices& gameplay::ResourceOwner::managers() { return *state->managers; }
gameevent::MessageWindows& gameplay::ResourceOwner::messages() { return *state->messages; }
void gameplay::ResourceOwner::updateMessages() {
    state->messages->update(0);
    state->managers->setMessageSpeaking(state->messages->speaking());
}
std::vector<roommaterials::Texture> gameplay::ResourceOwner::interfaceTextures() const {
    auto textures = state->hud->textures();
    const auto& messages = state->messageGraphics->textures();
    textures.insert(textures.end(), messages.begin(), messages.end());
    return textures;
}
std::vector<startupscene::Picture> gameplay::ResourceOwner::messagePictures() const {
    auto pictures = state->messageGraphics->pictures(*state->messages);
    for (auto& picture : pictures) picture.texture += state->hud->textures().size();
    return pictures;
}
const s3denvironment::Camera& gameplay::ResourceOwner::camera() const { return state->camera; }
const std::array<std::int32_t, 4>& gameplay::ResourceOwner::cameraViewport() const { return state->cameras.active->viewport; }

void gameplay::ResourceOwner::updateCamera() {
    const auto* player = state->managers->playerEvents().player(0);
    if (!player) throw std::runtime_error("Camera update requires the manager-owned player.");
    if (player->room()) state->cameraRoom = player->room();
    const auto& transition = state->roomTransition;
    const bool paintingTransition = (transition.currentRoom >> 24) == 3 || (state->cameraRoom >> 24) == 3 ||
        (transition.mode != roomtraversal::TransitionMode::Idle && (transition.previousRoom >> 24) == 3);
    if (state->entry.map == 6 && !paintingTransition) state->roomTransition.initialize(state->cameraRoom);
    else state->roomTransition.update(state->cameraRoom);
    if (state->entrance->updateCamera()) return;
    if (player->view.field895) {
        state->cameraFollow->updateScanner(*state->cameras.cameras[0], player->view);
        return;
    }
    state->cameraFollow->update(*state->cameras.cameras[0], player->view, state->cameraRoom);
}

void gameplay::ResourceOwner::setCurrentRoomLight(bool enabled) {
    state->managers->setRoomLight(state->cameraRoom >> 24, enabled);
}

void gameplay::ResourceOwner::bindPlayerDemo(std::function<void(std::string_view)> request) {
    state->entrance->bindPlayerDemo(std::move(request));
}

void gameplay::ResourceOwner::bindControllerInstructions(std::function<void()> request) {
    state->entrance->bindControllerInstructions(std::move(request));
}
void gameplay::ResourceOwner::bindSave(std::function<void()> callback) {
    state->entrance->bindSave([this, callback = std::move(callback)] {
        state->managers->snapshotFlags();
        callback();
    });
}

void gameplay::ResourceOwner::completeSave() { state->entrance->completeSave(); }

void gameplay::ResourceOwner::updateEvents(unsigned elapsedSeconds, std::uint32_t audioFrame) {
    state->entrance->update(elapsedSeconds, audioFrame);
}

bool gameplay::ResourceOwner::entranceActive() const { return state->entrance->active(); }
bool gameplay::ResourceOwner::menuEnabled() const { return state->entrance->menuEnabled(); }
bool gameplay::ResourceOwner::fullscreen() const { return state->entrance->fullscreen(); }
gameplay::Hud& gameplay::ResourceOwner::hud() { return *state->hud; }
bool gameplay::ResourceOwner::eventCameraActive() const { return state->entrance->cameraActive(); }

gameplaycamera::DrawState gameplay::ResourceOwner::publishCamera() {
    auto result = state->cameraProjection->publish(*state->cameras.active, *state->pose);
    state->camera = result.camera;
    return result;
}
const roomlocation::Rooms& gameplay::ResourceOwner::rooms() const { return *state->rooms; }
roommirror::Owner& gameplay::ResourceOwner::mirror() { return *state->mirror; }
const std::vector<std::uint8_t>& gameplay::ResourceOwner::executable() const { return state->executable; }
gameassets::Package& gameplay::ResourceOwner::package() { return state->package; }
roomtraversal::State& gameplay::ResourceOwner::visibility() { return state->visibility; }
std::vector<std::uint32_t>& gameplay::ResourceOwner::roomFlags() { return state->roomFlags; }
const roomresidency::InitialRooms& gameplay::ResourceOwner::residentRooms() const { return *state->residentRooms; }
const std::array<std::uint8_t, 0x220>& gameplay::ResourceOwner::inventory() const { return state->entry.inventory; }
bool gameplay::ResourceOwner::updateRooms(const std::function<void(const roomresidency::Resident&)>& release,
    const std::function<void(const roomresidency::Resident&)>& create) {
    if (!release || !create) throw std::invalid_argument("Room updates require renderer ownership callbacks.");
    std::vector<std::uint32_t> rooms;
    for (unsigned i = 0; i < state->managers->enemies().playerCount(); ++i) {
        const auto* player = state->managers->playerEvents().player(static_cast<std::int32_t>(i));
        if (player) rooms.push_back(player->room());
    }
    const auto& table = state->furnitureOwner->placementTable();
    return state->residentRooms->update(state->package, rooms, table, state->flags,
        [&](const roomresidency::Resident& resident) {
            release(resident);
            for (const auto& placement : resident.furniture.placements()) state->furnitureOwner->release(placement.row);
        }, [&](const roomresidency::Resident& resident) {
            state->furnitureOwner->create(resident.furniture, table);
            state->hud->addRoom(state->common, state->executable, resident.room);
            create(resident);
        });
}
roomfurniture::Owner& gameplay::ResourceOwner::furniture() { return *state->furnitureOwner; }

std::vector<roomtraversal::Submission> gameplay::ResourceOwner::prepareVisibility(const roomportal::Matrix& projection,
    const roomtraversal::Bounds& viewport, unsigned buffer,
    const std::function<bool(const std::array<std::int32_t, 3>&)>& centerVisible, bool scannerMode) {
    std::vector<roomtraversal::SceneRoom> rooms;
    std::vector<std::uint16_t> saved(256);
    for (unsigned i = 0; i < state->rooms->entries().size(); ++i) {
        const auto id = state->rooms->entries()[i].id;
        rooms.push_back({id, state->residentRooms->flags(i), state->residentRooms->links(i)});
        const auto offset = 0xa0 + (id & 255) * 2;
        saved[id & 255] = static_cast<std::uint16_t>((unsigned(state->entry.inventory.at(offset)) << 8) |
            state->entry.inventory.at(offset + 1));
    }
    auto& doors = state->managers->doors();
    roomtraversal::Walk traversal(rooms, state->residentRooms->descriptors(), state->visibility, state->roomFlags,
        saved, state->entry.map, {[&doors](unsigned id) { return doors.blocked(id); }, centerVisible});
    const auto* player = state->managers->actors().actor(state->managers->enemies().appearPoint(0).actor);
    if (!player) throw std::runtime_error("Room visibility requires the manager-owned player.");
    if (player->room() >> 24 >= rooms.size() || rooms[player->room() >> 24].id != player->room())
        throw std::runtime_error("Gameplay visibility requires a resolved player room.");
    if (std::none_of(state->residentRooms->slots().begin(), state->residentRooms->slots().end(),
        [&](const roomresidency::Resident& resident) { return resident.room == (player->room() & 255); }))
        throw std::runtime_error("Gameplay room transition requires the original room resource lifecycle.");
    auto submissions = traversal.stationary(player->room() >> 24, buffer, projection, viewport, 1,
        state->roomTransition.floorOpacity, &state->roomTransition, scannerMode);
    for (const auto& room : rooms) {
        const auto offset = 0xa0 + (room.id & 255) * 2;
        state->entry.inventory.at(offset) = static_cast<std::uint8_t>(saved[room.id & 255] >> 8);
        state->entry.inventory.at(offset + 1) = static_cast<std::uint8_t>(saved[room.id & 255]);
    }
    std::vector<doors::RoomVisit> visits;
    std::array<std::uint32_t, 256> roomIds{};
    for (unsigned i = 0; i < rooms.size(); ++i) roomIds[i] = rooms[i].id;
    for (unsigned i = 0; i < state->visibility.visitCount(buffer); ++i) {
        const auto index = state->visibility.visit(buffer, i);
        doors::RoomVisit visit;
        visit.marker = static_cast<std::uint8_t>(state->visibility.rawMarker(index, buffer));
        for (auto link : rooms.at(index).portals) visit.doorIds.push_back(state->residentRooms->descriptors().entries.at(link).identity);
        visits.push_back(std::move(visit));
    }
    doors.updatePresentation(buffer, visits, roomIds, player->room(), player->position);
    submissions.erase(std::remove_if(submissions.begin(), submissions.end(), [&](const roomtraversal::Submission& submission) {
        return std::none_of(state->residentRooms->slots().begin(), state->residentRooms->slots().end(),
            [&](const roomresidency::Resident& resident) { return resident.room == (submission.room & 255); });
    }), submissions.end());
    return submissions;
}

const scenetransition::Request& gameplay::ResourceOwner::transition() const { return state->entrance->transition(); }
