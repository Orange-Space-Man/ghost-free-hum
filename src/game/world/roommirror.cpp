#include "game/world/roommirror.h"
#include "resources/dolresources.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    constexpr unsigned foyerPosition = 0x804d9e18, foyerFieldOfView = 0x804d9e24, mirrorDistance = 0x804d9f34;
    float scalar(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
        float result;
        std::memcpy(&result, &bits, 4);
        return result;
    }
    void normalize(std::array<float, 3>& vector) {
        const float inverse = 1 / std::sqrt((vector[0] * vector[0] + vector[1] * vector[1]) + vector[2] * vector[2]);
        for (auto& value : vector) value *= inverse;
    }
}

roommirror::Owner::Owner(const std::vector<std::uint8_t>& executable, const roomlocation::Rooms& rooms) :
    pose(executable), projection(executable), distance(scalar(executable, mirrorDistance)) {
    for (unsigned i = 0; i < 3; ++i) position[i] = scalar(executable, foyerPosition + i * 4);
    world = pose.rotation({0, -3640, 0});
    for (unsigned i = 0; i < 3; ++i) {
        world[i * 4 + 3] = position[i];
        normal[i] = world[i * 4 + 2];
    }
    roomId = rooms.resolve(position, 0);
    retained.fieldOfView = scalar(executable, foyerFieldOfView);
    retained.nearPlane = distance;
    retained.visibilityMode = 2;
}

bool roommirror::Owner::visible(const roomtraversal::State& visibility) const {
    return roomId >> 24 != 255 && visibility.marked(roomId >> 24, 0, 0);
}

gameplaycamera::DrawState roommirror::Owner::camera(const s3denvironment::Camera& main) {
    retained.target = position;
    retained.target[1] += float(height());
    std::array<float, 3> direction;
    auto surfaceNormal = normal;
    for (unsigned i = 0; i < 3; ++i) direction[i] = main.position[i] - retained.target[i];
    normalize(direction);
    normalize(surfaceNormal);
    const float dot = (direction[0] * surfaceNormal[0] + direction[1] * surfaceNormal[1]) + direction[2] * surfaceNormal[2];
    for (unsigned i = 0; i < 3; ++i) direction[i] = (2 * surfaceNormal[i]) * dot - direction[i];
    normalize(direction);
    for (unsigned i = 0; i < 3; ++i)
        retained.position[i] = retained.target[i] - distance * direction[i];
    return projection.publish(retained, pose);
}

const actormodel::Matrix& roommirror::Owner::matrix() const { return world; }
unsigned roommirror::Owner::room() const { return roomId; }
unsigned roommirror::Owner::width() const { return 70; }
unsigned roommirror::Owner::height() const { return 100; }
