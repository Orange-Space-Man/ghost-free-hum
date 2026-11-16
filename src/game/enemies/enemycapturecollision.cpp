#include "game/enemies/enemycapturecollision.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    float number(std::uint32_t bits) {
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    ghostcapture::Vector transform(const actormodel::Matrix& matrix, const ghostcapture::Vector& value) {
        ghostcapture::Vector result;
        for (unsigned row = 0; row < 3; ++row) {
            const unsigned offset = row * 4;
            const float xy = std::fma(matrix[offset], value[0], matrix[offset + 1] * value[1]);
            result[row] = xy + std::fma(matrix[offset + 2], value[2], matrix[offset + 3]);
        }
        return result;
    }
}

ghostcapture::Collision::Collision(const actorcollision::Catalog& shapes, const actormodel::InitialPose& pose,
    const roomlocation::Rooms& rooms, const mapcollision::State& map) : shapes(shapes), pose(pose), rooms(rooms), map(map) {
    reset();
}

void ghostcapture::Collision::reset() {
    contact = 0;
    polygonContact = false;
    room = 0xffffffff;
    boundaryNormal.fill(0);
    minimum.fill(0);
    maximum.fill(0);
}

ghostcapture::Vector ghostcapture::Collision::sphereCenter(const actors::MoveActor& actor,
    const actorcollision::Record& record) const {
    Vector result{number(record[2]), number(record[3]), number(record[4])};
    const auto joint = static_cast<std::int32_t>(record[0]);
    if (joint < 0) {
        auto matrix = pose.rotation(actor.rotation);
        for (unsigned i = 0; i < 3; ++i) matrix[i * 4 + 3] = actor.position[i];
        return transform(matrix, result);
    }
    if (!actor.model) return {};
    return transform(actor.matrix, transform(actor.model->matrix(0, static_cast<unsigned>(joint)), result));
}

void ghostcapture::Collision::contain(actors::MoveActor& actor) const {
    for (const auto& record : shapes.type(actor.type()).records) {
        const auto sphere = sphereCenter(actor, record);
        auto corrected = sphere;
        const float radius = number(record[1]);
        for (unsigned i = 0; i < 3; ++i) {
            const float low = minimum[i] + radius, high = maximum[i] - radius;
            if (corrected[i] < low) corrected[i] = low;
            if (high < corrected[i]) corrected[i] = high;
            corrected[i] = actor.position[i] + (corrected[i] - sphere[i]);
        }
        actor.setPosition(corrected);
    }
}

void ghostcapture::Collision::update(actors::MoveActor& actor, double padding) {
    if (room != actor.fieldB4) {
        if (room != 0xffffffff) contain(actor);
        if (actor.fieldB4 != 0xffffffff) {
            room = actor.fieldB4;
            const auto& bounds = rooms.entries().at(room >> 24).bounds;
            for (unsigned i = 0; i < 3; ++i) {
                minimum[i] = static_cast<float>(bounds[i]);
                maximum[i] = static_cast<float>(bounds[i + 3]);
                center[i] = (minimum[i] + maximum[i]) * 0.5f;
                size[i] = maximum[i] - minimum[i];
            }
        }
    }
    bool lowX = false, highX = false, lowZ = false, highZ = false;
    for (const auto& record : shapes.type(actor.type()).records) {
        const auto sphere = sphereCenter(actor, record);
        const float radius = static_cast<float>(double(number(record[1])) + padding);
        lowX |= sphere[0] - radius < minimum[0];
        highX |= maximum[0] < sphere[0] + radius;
        lowZ |= sphere[2] - radius < minimum[2];
        highZ |= maximum[2] < sphere[2] + radius;
    }
    const unsigned nextContact = unsigned(lowX || highX) + unsigned(lowZ || highZ);
    Vector normal{lowX ? 1.0f : highX ? -1.0f : 0.0f, 0, lowZ ? 1.0f : highZ ? -1.0f : 0.0f};
    const float length = std::sqrt((normal[0] * normal[0] + normal[1] * normal[1]) + normal[2] * normal[2]);
    if (length >= 0.0001f) {
        const float scale = static_cast<float>(1.0 / double(length));
        for (auto& component : normal) component *= scale;
    } else normal.fill(0);
    changed = contact != nextContact || boundaryNormal != normal;
    if (changed) {
        contact = nextContact;
        boundaryNormal = normal;
    }
    polygonContact = static_cast<std::int16_t>(actor.fieldB0) >= 0;
    if (polygonContact && !map.polygonNormal(static_cast<std::int16_t>(actor.fieldB0), polygonNormal)) {
        throw std::runtime_error("Capture collision refers to an absent original polygon.");
    }
}
