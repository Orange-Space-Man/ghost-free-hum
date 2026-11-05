#include "game/player/playerlightcontacts.h"
#include "resources/dolresources.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    template<class T> T constant(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, sizeof(T), bytes, error)) throw std::runtime_error(error);
        std::reverse(bytes.begin(), bytes.end());
        T result;
        std::memcpy(&result, bytes.data(), sizeof(result));
        return result;
    }

    constexpr unsigned excludedCategoryAddress = 0x804d3f54, eventAddress = 0x804d2098;
    constexpr unsigned paddingAddress = 0x804dabc8, nearRadiusAddress = 0x804dabc0, epsilonAddress = 0x804daba8;

    float number(std::uint32_t value) {
        float result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
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

playeractor::LightContacts::LightContacts(const std::vector<std::uint8_t>& executable, const actormodel::Catalog& models) :
    collisions(executable, models), pose(executable), excludedCategory(constant<std::int32_t>(executable, excludedCategoryAddress)),
    message(constant<unsigned>(executable, eventAddress)), padding(constant<float>(executable, paddingAddress)),
    nearRadius(constant<float>(executable, nearRadiusAddress)), epsilon(constant<float>(executable, epsilonAddress)) {}

bool playeractor::LightContacts::visible(const gameplaycamera::CameraOwner& camera, const std::array<float, 3>& position) const {
    std::array<float, 4> projected;
    for (unsigned row = 0; row < 4; ++row) {
        const auto& matrix = camera.matrix;
        projected[row] = ((matrix[row * 4 + 3] + matrix[row * 4 + 2] * position[2]) +
            matrix[row * 4] * position[0]) + matrix[row * 4 + 1] * position[1];
    }
    if (projected[3] == 0) return false;
    const float inverse = 1 / projected[3];
    for (unsigned i = 0; i < 3; ++i) projected[i] *= inverse;
    return !(projected[0] <= -1 || projected[0] >= 1 || projected[1] <= -1 || projected[1] >= 1 || projected[2] >= 0);
}

bool playeractor::LightContacts::contains(const playermodels::LightModel& light, const std::array<float, 3>& position,
    double radius, const gameplaycamera::CameraOwner* camera) const {
    if (!light.state) return false;
    std::array<float, 3> offset;
    for (unsigned i = 0; i < 3; ++i) offset[i] = position[i] - light.fieldC[i];
    const float squared = (offset[0] * offset[0] + offset[1] * offset[1]) + offset[2] * offset[2];
    if (!(squared < light.field50)) return false;
    if (nearRadius * float(radius * radius) <= squared) {
        float threshold = light.field44[0];
        if (radius > 0) {
            const float width = light.field44[1] + float(radius / double(light.field44[0]));
            double length = float(double(light.field50) + double(width * width));
            if (length > 0) {
                double inverse = 1.0 / std::sqrt(length);
                inverse = 0.5 * inverse * (3.0 - length * inverse * inverse);
                inverse = 0.5 * inverse * (3.0 - length * inverse * inverse);
                length = float(length * 0.5 * inverse * (3.0 - length * inverse * inverse));
            }
            threshold = float(double(light.field4C) / length);
        }
        const float length = std::sqrt(squared);
        if (length >= epsilon) {
            const float inverse = float(1.0 / double(length));
            for (auto& component : offset) component *= inverse;
        } else offset.fill(0);
        const float dot = (offset[0] * light.field24[0] + offset[1] * light.field24[1]) + offset[2] * light.field24[2];
        if (!(threshold <= dot)) return false;
    }
    return !camera || visible(*camera, position);
}

void playeractor::LightContacts::update(const playermodels::LightModel& light, enemies::Manager& manager,
    const actormodel::Catalog& models, const roomtraversal::State& visibility) const {
    for (const auto& slot : manager.slots()) {
        if (slot.state != 2) continue;
        const auto& actor = dynamic_cast<const actors::MoveActor&>(slot.owner->actor());
        if (models.type(actor.actorType).field14 == excludedCategory) continue;
        const unsigned room = actor.fieldB4 >> 24;
        if (!visibility.marked(room, 0, 0) && !visibility.marked(room, 1, 0)) continue;
        if (!light.state) continue;
        for (const auto& record : collisions.type(actor.actorType).records) {
            std::array<float, 3> center{number(record[2]), number(record[3]), number(record[4])};
            const auto joint = static_cast<std::int32_t>(record[0]);
            if (joint < 0) {
                auto matrix = pose.rotation(actor.rotation);
                for (unsigned i = 0; i < 3; ++i) matrix[i * 4 + 3] = actor.position[i];
                center = transform(matrix, center);
            } else if (actor.model) {
                center = transform(actor.model->matrix(0, static_cast<unsigned>(joint)), center);
                center = transform(actor.matrix, center);
            } else center.fill(0);
            if (!contains(light, center, float(double(number(record[1])) + padding), light.camera) || !light.owner) continue;
            characterevent::Event event(message);
            slot.owner->event(event);
            break;
        }
    }
}
