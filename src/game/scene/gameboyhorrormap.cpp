#include "game/scene/gameboyhorrormap.h"
#include "resources/dolresources.h"
#include "resources/roomarchivereader.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    std::uint16_t half(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return static_cast<std::uint16_t>((unsigned(data.at(offset)) << 8) | data.at(offset + 1));
    }

    std::uint32_t word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (std::uint32_t(half(data, offset)) << 16) | half(data, offset + 2);
    }

    std::array<float, 3> cross(const std::array<float, 3>& a, const std::array<float, 3>& b) {
        return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    }

    void normalize(std::array<float, 3>& value) {
        const float length = std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
        for (auto& component : value) component /= length;
    }

    void approach(std::array<float, 3>& current, const std::array<float, 3>& target, float rate) {
        for (unsigned axis = 0; axis < current.size(); ++axis) {
            const int difference = static_cast<int>(10000.0f * (target[axis] - current[axis]));
            current[axis] += (0.18f * 0.0001f * float(difference)) * rate;
        }
    }

    std::array<float, 3> floorDirection(std::int16_t yaw, std::int16_t pitch, int floorStep, bool departing) {
        const int destinationPitch = (floorStep > 0) == departing ? -0x4000 : 0x4000;
        const float radians = 6.2831853071795864769f / 65536.0f;
        const float difference = float(destinationPitch - pitch) * radians;
        const float horizontal = std::cos(difference);
        const float angle = float(yaw) * radians;
        return {horizontal * std::sin(angle), -std::sin(difference), horizontal * std::cos(angle)};
    }
}

bool gameboyhorror::Map::load(const std::vector<std::uint8_t>& executable,
    const dooranimation::PoseTables& pose, const roomlocation::Rooms& rooms,
    const doordata::Table& doors, std::string& error) {
    error.clear();
    try {
        poseTables = pose;
        std::vector<std::uint8_t> compressed, archive, modelBytes, table;
        if (!dolresources::readStaticArchiveResource(executable, "guidemap.szp", compressed, error) ||
            !roomarchive::decompress(compressed, archive, error) ||
            !roomarchive::read(archive, "gidemap.bin", modelBytes, error) ||
            !mapModel.read(modelBytes, pose, error)) return false;
        if (!roomarchive::read(archive, "gide_lui.bin", modelBytes, error) ||
            !playerMarker.read(modelBytes, pose, error)) return false;
        constexpr std::array<const char*, 7> markerNames{
            "door.bin", "door_un.bin", "g_key1.bin", "g_key2.bin", "g_key3.bin", "g_key4.bin", "g_key5.bin"};
        for (unsigned i = 0; i < doorModels.size(); ++i)
            if (!roomarchive::read(archive, markerNames[i], modelBytes, error) ||
                !doorModels[i].read(modelBytes, pose, error)) return false;
        if (!dolresources::read(executable, 0x80300058, 28, table, error)) return false;
        for (unsigned i = 0; i < mapColors.size(); ++i)
            for (unsigned channel = 0; channel < 4; ++channel) mapColors[i][channel] = table[i * 4 + channel];
        if (!dolresources::read(executable, 0x80300240, 20, table, error)) return false;
        for (unsigned i = 0; i < floorHeights.size(); ++i) {
            const auto bits = word(table, i * 4);
            std::memcpy(&floorHeights[i], &bits, sizeof(bits));
        }
        const auto& definitions = rooms.entries();
        unsigned largestRoom = 0;
        for (const auto& definition : definitions) largestRoom = std::max(largestRoom, definition.id & 255);
        if (!dolresources::read(executable, 0x80300074, (largestRoom + 1) * 2, table, error))
            return false;
        mapRooms.clear();
        mapRooms.reserve(definitions.size());
        for (unsigned i = 0; i < definitions.size(); ++i) {
            const auto id = definitions[i].id;
            const auto node = static_cast<std::int16_t>(half(table, (id & 255) * 2));
            mapRooms.push_back({static_cast<std::uint8_t>(id), static_cast<std::uint8_t>(id >> 16),
                static_cast<std::uint8_t>(id >> 8), node, 0});
        }
        mapDoors.clear();
        mapDoors.reserve(doors.liveCount);
        for (const auto& descriptor : doors.entries) {
            if (descriptor.identity == 0xff) continue;
            const auto savedId = static_cast<std::uint8_t>(descriptor.identity >> 16);
            const unsigned room = descriptor.rooms[(descriptor.attributes[0] & 1) != 0];
            if (room >= definitions.size()) throw std::out_of_range("Map door room index.");
            const auto type = (descriptor.identity >> 8) & 255;
            std::vector<std::uint8_t> configuration;
            if (!dolresources::read(executable, 0x802ff95c + type * 20, 1, configuration, error)) return false;
            const float scale = 0.1f;
            const std::array<float, 3> position{scale * descriptor.center[0],
                scale * (descriptor.center[1] - descriptor.dimensions[1] * 0.5f),
                scale * descriptor.center[2]};
            const std::array<std::int32_t, 3> rotation{0,
                (descriptor.attributes[0] & 2) ? 0x4000 : 0, 0};
            const float width = configuration[0] == 2 ? 2.0f : 1.0f;
            const auto transform = pose.world(position, rotation, {width, 1.0f, 1.0f});
            unsigned key = 2;
            if (savedId == 3) key = 3;
            else if (savedId == 0x2a) key = 4;
            else if (savedId == 0x3b) key = 5;
            else if (savedId == 0x48) key = 6;
            mapDoors.push_back({transform, position, static_cast<std::uint8_t>(definitions[room].id >> 16),
                savedId, 1, static_cast<std::uint8_t>(key), false});
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

void gameboyhorror::Map::refresh(const std::vector<std::uint32_t>& roomFlags,
    const std::uint8_t* savedFlags, std::size_t savedFlagBytes,
    const std::uint8_t* keyFlags, std::size_t keyFlagBytes) {
    if (roomFlags.size() != mapRooms.size()) throw std::invalid_argument("Map rooms require their original room flags.");
    for (unsigned i = 0; i < mapRooms.size(); ++i) {
        auto& room = mapRooms[i];
        const auto flags = roomFlags[i];
        if (!(flags & 2)) room.color = 0;
        else if (!room.area) room.color = 2;
        else if (!(flags & 8) || room.room == 0x46) room.color = 1;
        else room.color = static_cast<std::uint8_t>(room.area + 2);
        if (room.color >= mapColors.size()) throw std::out_of_range("Original map room color index.");
    }
    if (!savedFlags || savedFlagBytes < 32 || !keyFlags || keyFlagBytes < 32)
        throw std::invalid_argument("Original map door and player key flags are unavailable.");
    for (auto& marker : mapDoors) {
        const bool known = (savedFlags[marker.savedId >> 3] & (0x80u >> (marker.savedId & 7))) != 0;
        const bool owned = (keyFlags[marker.savedId >> 3] & (1u << (marker.savedId & 7))) != 0;
        marker.doorModel = known ? 0 : 1;
        marker.keyVisible = known && owned;
    }
}

void gameboyhorror::Map::open(std::uint32_t playerRoom, const std::array<float, 3>& playerPosition) {
    if (mapRooms.empty()) throw std::logic_error("The Game Boy Horror map is not loaded.");
    playerFloor = static_cast<std::int8_t>(playerRoom >> 16);
    if (playerFloor < -1 || playerFloor > 3) throw std::out_of_range("Original map floor index.");
    selectedFloor = playerFloor;
    floorStep = 0;
    floorFrame = idleFrames = keyFocusFrame = 0;
    cameraTarget = {0.0f, 10.0f, -257.5f};
    overviewTarget = floorTransitionTarget = cameraTarget;
    markerPosition = playerPosition;
    cameraDistance = 1600.0f;
    cameraYaw = 0x1200;
    cameraPitch = 0x1900;
    zoomed = false;
    markerAngle = 0;
    keyFocusTransition = false;
    opened = true;
}

void gameboyhorror::Map::openKey(std::uint32_t playerRoom,
    const std::array<float, 3>& playerPosition, unsigned keyId) {
    open(playerRoom, playerPosition);
    const auto found = std::find_if(mapDoors.rbegin(), mapDoors.rend(), [keyId](const MapDoor& marker) {
        return marker.savedId == keyId;
    });
    if (found == mapDoors.rend()) throw std::out_of_range("Collected key has no original map door marker.");
    for (auto& marker : mapDoors)
        if (marker.savedId == keyId) marker.keyVisible = true;
    selectedFloor = static_cast<std::int8_t>(found->floor);
    cameraTarget = found->position;
    overviewTarget = floorTransitionTarget = cameraTarget;
    cameraDistance = 400.0f;
    zoomed = true;
    keyFocusTransition = true;
    keyFocusFrame = 0;
}

unsigned gameboyhorror::Map::update(std::uint32_t held, std::uint32_t pressed, bool fullRotation) {
    if (!opened) return 0;
    markerAngle += 0x300;
    keyAngle += 0x200;
    if (pressed & 0xa00) { opened = false; return 0; }
    // 8002AC8C holds the collected door for 60 frames, then moves to the
    // overview until frame 110 before accepting normal map camera input.
    if (keyFocusTransition) {
        if (keyFocusFrame++ >= 60) {
            zoomed = false;
            overviewTarget = {0.0f, floorHeights.at(selectedFloor + 1), -257.5f};
            approach(cameraTarget, overviewTarget, 1.0f);
            cameraDistance += std::clamp(1600.0f - cameraDistance, -40.0f, 40.0f);
        }
        if (keyFocusFrame < 110) return 0;
        cameraTarget = overviewTarget;
        cameraDistance = 1600.0f;
        floorTransitionTarget = overviewTarget;
        keyFocusTransition = false;
        idleFrames = 21;
        return 0;
    }
    unsigned sound = 0;
    if (floorStep && floorFrame == 20) {
        selectedFloor += floorStep;
        floorStep = 0;
        floorFrame = idleFrames = 0;
    }
    if (idleFrames <= 20) ++idleFrames;
    else if (!floorStep) {
        const int step = pressed & 0x8000000 ? 1 : pressed & 0x4000000 ? -1 :
            pressed & 0x200 ? playerFloor - selectedFloor : 0;
        const int next = selectedFloor + step;
        if (step && next >= -1 && next <= 3) {
            floorStep = next - selectedFloor;
            floorFrame = 0;
            sound = step == 1 ? 0x3818 : step == -1 ? 0x3819 : 0;
        }
    }
    if (floorStep) {
        if (floorFrame == 0 || floorFrame == 19) {
            overviewTarget[1] = floorHeights.at(selectedFloor + 1);
            const auto direction = floorDirection(cameraYaw, cameraPitch, floorStep, floorFrame == 0);
            const auto& origin = floorFrame == 0 ? cameraTarget : overviewTarget;
            for (unsigned axis = 0; axis < cameraTarget.size(); ++axis)
                floorTransitionTarget[axis] = origin[axis] + 2000.0f * direction[axis];
            if (floorFrame == 0) zoomed = false;
        }
        approach(cameraTarget, floorTransitionTarget, 0.3f);
        ++floorFrame;
        return sound;
    }
    if (pressed & 0x100 && selectedFloor == playerFloor) {
        zoomed = !zoomed;
        keyFocusTransition = false;
        sound = zoomed ? 0x3815 : 0x3816;
        if (zoomed) cameraTarget = {0.1f * markerPosition[0],
            floorHeights.at(selectedFloor + 1), 0.1f * markerPosition[2]};
        else cameraTarget = {0.0f, floorHeights.at(selectedFloor + 1), -257.5f};
        floorTransitionTarget = cameraTarget;
    }
    const float destination = zoomed ? 400.0f : 1600.0f;
    cameraDistance += std::clamp(destination - cameraDistance, -40.0f, 40.0f);
    overviewTarget[1] = floorHeights.at(selectedFloor + 1);
    if (!zoomed) {
        approach(floorTransitionTarget, overviewTarget, 1.0f);
        cameraTarget = floorTransitionTarget;
    }
    else if (keyFocusTransition) {
        const std::array<float, 3> playerTarget{0.1f * markerPosition[0],
            floorHeights.at(selectedFloor + 1), 0.1f * markerPosition[2]};
        approach(cameraTarget, playerTarget, 1.0f);
        floorTransitionTarget = cameraTarget;
    }
    else floorTransitionTarget = cameraTarget;
    const int yawStep = held & 0x1000000 ? 0x100 : held & 0x2000000 ? -0x100 : 0;
    const int nextYaw = int(cameraYaw) + yawStep;
    if (fullRotation) {
        cameraYaw = static_cast<std::int16_t>(static_cast<std::uint16_t>(nextYaw));
        if (yawStep) sound = 0x3017;
    } else if (nextYaw > 0x2000) cameraYaw = 0x2000;
    else if (nextYaw < -0x2000) cameraYaw = -0x2000;
    else {
        cameraYaw = static_cast<std::int16_t>(nextYaw);
        if (yawStep) sound = 0x3017;
    }
    if (!zoomed) return sound;
    const float x = held & 0x10000 ? -4.0f : held & 0x20000 ? 4.0f : 0.0f;
    const float z = held & 0x80000 ? -4.0f : held & 0x40000 ? 4.0f : 0.0f;
    if (x || z) {
        const float angle = float(cameraYaw) * 6.2831853071795864769f / 65536.0f;
        cameraTarget[0] = std::clamp(cameraTarget[0] + std::cos(angle) * x + std::sin(angle) * z,
            -600.0f, 600.0f);
        cameraTarget[2] = std::clamp(cameraTarget[2] - std::sin(angle) * x + std::cos(angle) * z,
            -700.0f, 100.0f);
    }
    return sound;
}

bool gameboyhorror::Map::active() const { return opened; }
bool gameboyhorror::Map::zoomedIn() const { return zoomed; }
int gameboyhorror::Map::floor() const { return selectedFloor; }
int gameboyhorror::Map::currentFloor() const { return playerFloor; }
const std::array<float, 3>& gameboyhorror::Map::target() const { return cameraTarget; }
float gameboyhorror::Map::distance() const { return cameraDistance; }
std::int16_t gameboyhorror::Map::yaw() const { return cameraYaw; }
std::int16_t gameboyhorror::Map::pitch() const { return cameraPitch; }
float gameboyhorror::Map::markerSize() const {
    return 0.08f + 0.01f * std::abs(std::sin(float(markerAngle) * 6.2831853071795864769f / 65536.0f));
}
float gameboyhorror::Map::markerBounce() const {
    return 5.0f * std::abs(std::sin(float(markerAngle) * 6.2831853071795864769f / 65536.0f));
}

void gameboyhorror::Map::camera(dooranimation::Matrix& view, std::array<float, 16>& projection) const {
    const float yawAngle = float(cameraYaw) * 6.2831853071795864769f / 65536.0f;
    const float pitchAngle = float(cameraPitch) * 6.2831853071795864769f / 65536.0f;
    const float horizontal = cameraDistance * std::cos(pitchAngle);
    const std::array<float, 3> eye{cameraTarget[0] + horizontal * std::sin(yawAngle),
        cameraTarget[1] + cameraDistance * std::sin(pitchAngle),
        cameraTarget[2] + horizontal * std::cos(yawAngle)};
    std::array<float, 3> backward{eye[0] - cameraTarget[0], eye[1] - cameraTarget[1], eye[2] - cameraTarget[2]};
    normalize(backward);
    auto right = cross({0.0f, 1.0f, 0.0f}, backward);
    normalize(right);
    const std::array<std::array<float, 3>, 3> rows{right, cross(backward, right), backward};
    for (unsigned row = 0; row < 3; ++row) {
        for (unsigned column = 0; column < 3; ++column) view[row * 4 + column] = rows[row][column];
        view[row * 4 + 3] = -(eye[0] * rows[row][0] + eye[1] * rows[row][1] + eye[2] * rows[row][2]);
    }
    projection.fill(0);
    const float cotangent = 1.0f / std::tan(26.0f * 3.1415926535897932385f / 360.0f);
    const float nearPlane = 16.0f, farPlane = 8192.0f;
    projection[0] = cotangent / (416.0f / 312.0f);
    projection[5] = cotangent;
    projection[10] = -nearPlane / (farPlane - nearPlane);
    projection[11] = -farPlane * nearPlane / (farPlane - nearPlane);
    projection[14] = -1.0f;
    for (unsigned column = 0; column < 4; ++column) projection[8 + column] += projection[12 + column];
}

const doors::ModelResource& gameboyhorror::Map::model() const { return mapModel; }
const doors::ModelResource& gameboyhorror::Map::marker() const { return playerMarker; }
const doors::ModelResource& gameboyhorror::Map::doorModel(unsigned index) const { return doorModels.at(index); }
dooranimation::Matrix gameboyhorror::Map::keyTransform(const MapDoor& marker) const {
    return poseTables.world(marker.position, {0, keyAngle, 0}, {1.0f, 1.0f, 1.0f});
}
const std::vector<gameboyhorror::MapDoor>& gameboyhorror::Map::doorMarkers() const { return mapDoors; }
const std::vector<gameboyhorror::MapRoom>& gameboyhorror::Map::rooms() const { return mapRooms; }
const std::array<std::array<std::uint8_t, 4>, 7>& gameboyhorror::Map::colors() const { return mapColors; }
