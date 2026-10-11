#include "game/actors/moveactor.h"
#include "game/world/mapcollisionstate.h"

#include <cstring>
#include <cmath>

namespace {
    float number(std::uint32_t bits) {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    std::array<float, 3> transform(const actormodel::Matrix& matrix, const std::array<float, 3>& value) {
        std::array<float, 3> result;
        for (unsigned row = 0; row < 3; ++row) {
            const unsigned offset = row * 4;
            const float xy = std::fma(matrix[offset], value[0], matrix[offset + 1] * value[1]);
            result[row] = xy + std::fma(matrix[offset + 2], value[2], matrix[offset + 3]);
        }
        return result;
    }
}

bool actors::MoveActor::resolveMapContact(const mapcollision::State& collision, const actorcollision::Catalog& catalog,
    std::int16_t& wall) {
    std::array<float, 3> displacement;
    for (unsigned i = 0; i < 3; ++i) displacement[i] = position[i] - previousPosition[i];
    field78C = 1;
    wall = -1;
    fieldB2 = -1;
    bool grounded = false;
    for (const auto& record : catalog.type(actorType).records) {
        std::array<float, 3> center{}, offset{number(record[2]), number(record[3]), number(record[4])};
        if (static_cast<std::int32_t>(record[0]) < 0) {
            auto placement = construction.pose.rotation(rotation);
            for (unsigned i = 0; i < 3; ++i) placement[i * 4 + 3] = position[i];
            center = transform(placement, offset);
        } else if (model) {
            center = transform(matrix, transform(model->matrix(0, record[0]), offset));
        }
        const auto contact = collision.sweepSphere(center, number(record[1]), displacement);
        if (contact.floor != -1 || contact.wall != -1) {
            for (unsigned i = 0; i < 3; ++i) {
                position[i] += contact.correction[i];
                field5C[i] += contact.correction[i];
            }
        }
        if (contact.floor != -1) {
            grounded = true;
            field78C = contact.floorNormal[1];
            fieldB2 = static_cast<std::int16_t>(contact.floor);
        }
        if (contact.wall != -1) wall = static_cast<std::int16_t>(contact.wall);
    }
    auto probe = position;
    probe[1] += 5;
    probe[1] = collision.floorHeight(probe) + 5;
    const auto polygons = collision.floorPolygons(probe, 10);
    for (unsigned i = 0; i < polygons.size(); ++i) {
        fieldsB8[i * 2] = static_cast<std::uint8_t>(polygons[i] >> 8);
        fieldsB8[i * 2 + 1] = static_cast<std::uint8_t>(polygons[i]);
    }
    fieldsB8[8] = fieldsB8[9] = 255;
    return grounded;
}

void actors::MoveActor::updateMapContact(const mapcollision::State& collision, const actorcollision::Catalog& catalog) {
    fieldB0 = -1;
    const auto& animations = construction.catalog.type(actorType).animations;
    if (!(animations.at(animationState)[5] & 0x200)) return;
    std::int16_t wall;
    if (!resolveMapContact(collision, catalog, wall)) {
        flags &= ~0x10000u;
        const float height = collision.floorHeight(position);
        if (position[1] - height >= 49.8f) {
            if (fieldA0 == -1) {
                const auto falling = animations.at(animationState)[3];
                if (animations.at(falling)[5] & 0x20) selectAnimation(falling);
            }
        } else if ((animations.at(animationState)[5] & 0x20) && velocity[1] >= -0.1f &&
            !(animations.at(animationState)[5] & 0x10)) {
            position[1] = height;
            selectAnimation(animations.at(animationState)[4]);
            velocity[1] = 0;
        }
    } else {
        flags |= 0x10000;
        if ((animations.at(animationState)[5] & 0x30) == 0x30) {
            selectAnimation(animations.at(animationState)[4]);
            velocity[1] = 0;
        }
    }
    fieldB0 = wall;
}
