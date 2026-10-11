#include "resources/dolresources.h"
#include "game/scene/gameplayresourceowner.h"
#include "resources/roomarchivereader.h"
#include "game/player/playeractor.h"
#include "game/startup/startupresources.h"

#include <stdexcept>
#include <cstring>
#include <algorithm>

struct gameplay::ResourceOwner::State {
    explicit State(gameentry::State& entry) : entry(entry), motors(motorOutput) {}

    void initialize(const std::filesystem::path& path) {
        if (!entry.initialized || entry.map != 2) throw std::invalid_argument("Gameplay resources require a selected map-two entry.");
        std::string error;
        std::vector<std::uint8_t> compressed, commonBytes, mapBytes;
        if (!package.open(path, error) || !package.read("sys/main.dol", executable, error) ||
            !package.read("files/Game/game_usa.szp", compressed, error) ||
            !roomarchive::decompress(compressed, commonBytes, error) || !common.read(commonBytes, error) ||
            !package.read("files/Map/map2.szp", compressed, error) ||
            !roomarchive::decompress(compressed, mapBytes, error) || !map.read(mapBytes, error) ||
            !mission.read(mapBytes, error) || !types.read(executable, error) ||
            !creation.read(executable, error) || !strategies.read(executable, error) ||
            !modelCatalog.read(executable, error) || !materials.read(executable, error) ||
            !parameters.loadParameters(executable, commonBytes, error) || !jaudio::read(package, audio, error)) {
            throw std::runtime_error(error);
        }
        models = std::make_unique<actormodel::Resources>(modelCatalog);
        models->initializeEmbedded(common);
        pose = std::make_unique<actormodel::InitialPose>(executable);
        rooms = std::make_unique<roomlocation::Rooms>(executable, entry.map);
        collisions = std::make_unique<actorcollision::Catalog>(executable, modelCatalog);
        construction = std::make_unique<actors::Construction>(actors::Construction{
            modelCatalog, *models, parameters, *pose, actorGlobals, *rooms});
        visibility.initialize(static_cast<unsigned>(rooms->entries().size()));
        roomFlags.resize(rooms->entries().size());
        for (unsigned i = 0; i < rooms->entries().size(); ++i) {
            const auto number = rooms->entries()[i].id & 255;
            const auto offset = 0xa0 + number * 2;
            const unsigned saved = (unsigned(entry.inventory.at(offset)) << 8) | entry.inventory.at(offset + 1);
            auto& value = roomFlags[i];
            if (saved & 1) value |= 2;
            if (saved & 2) value |= 8;
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
        residentRooms = std::make_unique<roomresidency::InitialRooms>(executable, entry.map, *rooms);
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
        managers = std::make_unique<ManagerServices>(dependencies);
        auto* player = managers->actors().actor(managers->enemies().appearPoint(0).actor);
        if (!player) throw std::runtime_error("Gameplay camera requires the manager-owned player.");
        gameplayspawn::Spawn spawn;
        spawn.position = player->position;
        spawn.rotation = player->rotation;
        spawn.roomId = player->room();
        if (!gameplaycamera::initialize(executable, spawn, camera, error)) throw std::runtime_error(error);
        auto& owner = *cameras.cameras[0];
        owner.position = camera.position;
        owner.target = camera.target;
        owner.fieldOfView = camera.fieldOfView;
        owner.aspect = camera.aspect;
        owner.nearPlane = camera.nearPlane;
        owner.farPlane = camera.farPlane;
    }

    gameentry::State& entry;
    gameassets::Package package;
    std::vector<std::uint8_t> executable;
    gamearchive::Archive common, map;
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
    gameflags::State flags;
    playerinput::WindowsMotorOutput motorOutput;
    playerinput::MotorState motors;
    playerinput::Pad pad;
    playerrecord::Record secondRecord{};
    std::uint32_t randomState = 0;
    std::int32_t audioLife = 0;
    std::vector<std::uint32_t> roomFlags;
    roomtraversal::State visibility;
    std::unique_ptr<actormodel::Resources> models;
    std::unique_ptr<actormodel::InitialPose> pose;
    std::unique_ptr<roomlocation::Rooms> rooms;
    std::unique_ptr<actorcollision::Catalog> collisions;
    std::unique_ptr<actors::Construction> construction;
    std::unique_ptr<roomresidency::InitialRooms> residentRooms;
    std::unique_ptr<roomfurniture::Owner> furnitureOwner;
    std::unique_ptr<ManagerServices> managers;
};

gameplay::ResourceOwner::ResourceOwner(const std::filesystem::path& path, gameentry::State& entry) : state(std::make_unique<State>(entry)) {
    state->initialize(path);
}

gameplay::ResourceOwner::~ResourceOwner() = default;
gameplay::ManagerServices& gameplay::ResourceOwner::managers() { return *state->managers; }
const s3denvironment::Camera& gameplay::ResourceOwner::camera() const { return state->camera; }
const roomlocation::Rooms& gameplay::ResourceOwner::rooms() const { return *state->rooms; }
const std::vector<std::uint8_t>& gameplay::ResourceOwner::executable() const { return state->executable; }
gameassets::Package& gameplay::ResourceOwner::package() { return state->package; }
roomtraversal::State& gameplay::ResourceOwner::visibility() { return state->visibility; }
std::vector<std::uint32_t>& gameplay::ResourceOwner::roomFlags() { return state->roomFlags; }
const roomresidency::InitialRooms& gameplay::ResourceOwner::residentRooms() const { return *state->residentRooms; }
roomfurniture::Owner& gameplay::ResourceOwner::furniture() { return *state->furnitureOwner; }

std::vector<roomtraversal::Submission> gameplay::ResourceOwner::prepareVisibility(const roomportal::Matrix& projection,
    const roomtraversal::Bounds& viewport, unsigned buffer,
    const std::function<bool(const std::array<std::int32_t, 3>&)>& centerVisible) {
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
    if (player->room() != 0x02000102) throw std::runtime_error("Gameplay visibility requires the room-transition context outside the initial foyer.");
    auto submissions = traversal.stationary(player->room() >> 24, buffer, projection, viewport, 1, 255);
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
