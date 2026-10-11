#include "game/actors/actortrailcontroller.h"

#include <cmath>
#include <stdexcept>

namespace {
    void gradient(actortrail::Segment& segment, unsigned alpha, unsigned step) {
        const float decrement = 0.2f * float(step);
        for (unsigned i = 0; i < 12; ++i) {
            const float reduction = decrement * float(5 - i / 2);
            segment.alphas[i] = static_cast<std::uint8_t>(static_cast<int>(float(alpha) - reduction));
        }
    }

    actortrail::Vector transform(const std::array<float, 12>& matrix, const actortrail::Vector& point) {
        actortrail::Vector result;
        for (unsigned i = 0; i < 3; ++i) result[i] =
            ((matrix[i * 4] * point[0] + matrix[i * 4 + 1] * point[1]) + matrix[i * 4 + 2] * point[2]) + matrix[i * 4 + 3];
        return result;
    }
}

std::array<std::array<float, 2>, 12> actortrail::textureCoordinates() {
    std::array<std::array<float, 2>, 12> result;
    for (unsigned i = 0; i < result.size(); ++i) result[i] = {float(i % 2), 0.2f * float(i / 2)};
    return result;
}

bool actortrail::Segment::create(const Endpoints& previous, const Endpoints& current, std::uint8_t kind,
    std::uint32_t rgba, std::uint8_t life, std::uint8_t countdown, float expansion, float threshold) {
    if (remaining || !life || kind >= 5) return false;
    Endpoints delta;
    std::array<float, 2> distances;
    for (unsigned edge = 0; edge < 2; ++edge) {
        for (unsigned axis = 0; axis < 3; ++axis) delta[edge][axis] = current[edge][axis] - previous[edge][axis];
        const auto& value = delta[edge];
        distances[edge] = std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]);
    }
    if (!(distances[0] + distances[1] > threshold)) return false;
    flags = countdown ? 1 : 0;
    remaining = life;
    type = kind;
    color = rgba;
    spread = expansion;
    for (auto& edge : delta) for (auto& value : edge) value *= 0.2f;
    for (unsigned i = 0; i < vertices.size(); ++i) {
        for (unsigned axis = 0; axis < 3; ++axis) vertices[i][axis] =
            previous[i % 2][axis] + delta[i % 2][axis] * float(i / 2);
    }
    const unsigned alpha = color & 255;
    alphaStep = static_cast<std::uint8_t>(alpha / life);
    gradient(*this, alpha, flags & 1 ? alpha : alphaStep);
    return true;
}

void actortrail::Segment::update() {
    if (!remaining || !(flags & 2)) return;
    if (spread != 0) {
        for (unsigned i = 0; i < vertices.size(); i += 2) {
            for (unsigned axis = 0; axis < 3; ++axis) {
                const float delta = (vertices[i][axis] - vertices[i + 1][axis]) * spread;
                vertices[i][axis] += delta;
                vertices[i + 1][axis] -= delta;
            }
        }
    }
    if (flags & 1) {
        const unsigned alpha = color & 255;
        const unsigned next = alpha < alphaStep ? 0 : alpha - alphaStep;
        color = (color & 0xffffff00) | next;
        gradient(*this, next, next);
    } else {
        for (auto& alpha : alphas) alpha = alpha < alphaStep ? 0 : static_cast<std::uint8_t>(alpha - alphaStep);
    }
    --remaining;
}

actortrail::Pool::Pool(unsigned mode) : segments(mode == 10 ? 400 : 100) {}

void actortrail::Pool::load(const gamearchive::Archive& archive) {
    const std::array<const char*, 5> names{"mapu_tail.bti", "test_t1.bti", "rampbase.bti", "startail.bti", "tail_sb.bti"};
    for (unsigned i = 0; i < names.size(); ++i) {
        const int entry = archive.findTypeResource(0x414a494f, names[i]);
        textures[i] = entry < 0 ? std::vector<std::uint8_t>{} : archive.resource(static_cast<unsigned>(entry));
    }
}

bool actortrail::Pool::create(const Endpoints& previous, const Endpoints& current, std::uint8_t type,
    std::uint32_t color, std::uint8_t life, std::uint8_t countdown, float spread) {
    for (auto& segment : segments) if (segment.create(previous, current, type, color, life, countdown, spread, movementThreshold)) return true;
    return false;
}

void actortrail::Pool::update() {
    for (auto& segment : segments) segment.update();
}

void actortrail::Pool::draw(const std::function<void(const Segment&, const std::vector<std::uint8_t>&)>& submit) {
    for (unsigned type = 0; type < textures.size(); ++type) {
        for (auto& segment : segments) {
            if (!segment.remaining || segment.type != type) continue;
            if (textures[type].empty()) throw std::runtime_error("Original trail texture is not loaded.");
            submit(segment, textures[type]);
            segment.flags |= 2;
        }
    }
}

const std::vector<actortrail::Segment>& actortrail::Pool::entries() const { return segments; }

void actortrail::Controller::initialize(actors::MoveActor* object, const std::array<std::int32_t, 2>& indices,
    const Endpoints& points, std::uint8_t kind, std::uint32_t rgba, std::uint8_t lifetime, std::uint8_t delay, float expansion) {
    actor = object;
    joints = indices;
    endpoints = points;
    type = kind;
    color = rgba;
    life = lifetime;
    interval = delay;
    spread = expansion;
    reset();
}

void actortrail::Controller::reset() { countdown = static_cast<std::uint8_t>(interval + 2); }

void actortrail::Controller::update(Pool* pool) {
    auto current = endpoints;
    if (actor) {
        for (unsigned i = 0; i < current.size(); ++i) {
            if (joints[i] >= 0) {
                if (!actor->model) throw std::runtime_error("Original trail actor model is absent.");
                current[i] = transform(actor->model->matrix(0, joints[i]), current[i]);
            }
            current[i] = transform(actor->matrix, current[i]);
        }
    }
    if (countdown < 2 && pool) {
        if (!previous) throw std::runtime_error("Original trail endpoint storage has not been initialized.");
        pool->create(*previous, current, type, color, life, countdown, spread);
    }
    if (countdown) --countdown;
    previous = current;
}
