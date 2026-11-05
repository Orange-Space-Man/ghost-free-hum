#include "playersheetmotion.h"
#include "resources/dolresources.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    using playerscanner::Vector;

    float scalar(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
        float result;
        std::memcpy(&result, &bits, 4);
        return result;
    }
    Vector subtract(const Vector& a, const Vector& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
    float dot(const Vector& a, const Vector& b) { return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2]; }
    float magnitude(const Vector& value) { return std::sqrt(dot(value, value)); }
    float normalize(Vector& value) {
        const float length = magnitude(value);
        if (length >= 0.0001f) for (auto& component : value) component *= 1 / length;
        else value.fill(0);
        return length;
    }
    Vector cross(const Vector& a, const Vector& b) {
        return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    }
    Vector interpolate(const Vector& a, const Vector& b, float t) {
        return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t};
    }
    float fraction(unsigned value, int first, int last) { return (float(value) - float(first)) / float(last - first); }
    float refinedRoot(float value) {
        if (!(0 < value)) return value;
        const double input = value;
        double estimate = 1.0 / std::sqrt(input);
        estimate = 0.5 * estimate * (3.0 - input * estimate * estimate);
        estimate = 0.5 * estimate * (3.0 - input * estimate * estimate);
        return static_cast<float>(input * 0.5 * estimate * (3.0 - input * estimate * estimate));
    }
    void constrain(playersheets::Point& point, const playersheets::MotionParameters& parameters,
        const Vector& direction, const Vector& origin, float distance) {
        for (unsigned i = 0; i < 3; ++i) {
            const float displacement = (origin[i] + direction[i] * distance) - point.position[i];
            point.position[i] += displacement * parameters.positionResponse;
            point.velocity[i] += displacement * parameters.velocityResponse;
        }
    }
}

playersheets::MotionParameters::MotionParameters(const std::vector<std::uint8_t>& executable) :
    attachedDamping(scalar(executable, 0x804dae90)), detachedDamping(scalar(executable, 0x804dae94)),
    gravity(scalar(executable, 0x804dae98)), positionResponse(scalar(executable, 0x804dae9c)),
    velocityResponse(scalar(executable, 0x804dae7c)) {}

playersheets::Motion::Motion(Grid& grid, const std::vector<std::uint8_t>& executable) :
    drops(grid.layout().drops), grid(grid),
    width(magnitude(subtract(grid.layout().corners[2], grid.layout().corners[0]))),
    height(magnitude(subtract(grid.layout().corners[1], grid.layout().corners[0]))),
    anchorBase(scalar(executable, 0x804dae50)), anchorWeight(scalar(executable, 0x804dae54)),
    anchorSnap(scalar(executable, 0x804dae58)), bounce(scalar(executable, 0x804dae8c)) {}

void playersheets::Motion::retract(bool hasSuctionOwner) {
    if (detached) return;
    const float response = grid.layout().response;
    if (pulling) {
        unsigned completed = 0;
        for (unsigned axis = 0; axis < 2; ++axis) {
            if (!(pullDirection[axis] > 0) && !(pullDirection[axis] < 0)) continue;
            const unsigned side = axis + (pullDirection[axis] < 0 ? 2 : 0);
            const float length = axis == 0 ? width : height;
            drops[side] -= response;
            if (drops[side] < -length * 0.5f) halfway = true;
            if (drops[side] < -length) { drops[side] = -length; ++completed; }
        }
        if ((pullDirection[0] * pullDirection[1] == 0 && completed) || completed > 1) detached = true;
        if (!detached) pulling = false;
    } else if (retracting) {
        unsigned completed = 0;
        for (unsigned side : {0U, 2U, 1U, 3U}) {
            drops[side] -= response;
            if (drops[side] < 0) { drops[side] = 0; ++completed; }
        }
        if (completed == 4) detached = true;
        else if (!hasSuctionOwner) retracting = false;
    }
}

bool playersheets::Motion::crop() {
    std::array<float, 4> bounds{0, 0, 1, 1}, hanging{};
    for (unsigned i = 0; i < 4; ++i) {
        if (drops[i] >= 0) hanging[i] = drops[i];
        else bounds[i] = i < 2 ? -drops[i] / (i == 0 ? width : height) :
            1 + drops[i] / (i == 2 ? width : height);
    }
    if (bounds == cropBounds && !detached) return false;
    const auto& layout = grid.layout();
    const auto& border = layout.border;
    const auto& corners = layout.corners;
    const float xSpan = bounds[2] - bounds[0], ySpan = bounds[3] - bounds[1];
    const std::array<float, 4> steps{hanging[0] / border[0], hanging[1] / border[1],
        hanging[2] / float((layout.width - 1) - int(border[2])),
        hanging[3] / float((layout.height - 1) - int(border[3]))};
    const float xStep = ((width + std::min(0.0f, drops[0])) + std::min(0.0f, drops[2])) / (border[2] - border[0]);
    const float yStep = ((height + std::min(0.0f, drops[1])) + std::min(0.0f, drops[3])) / (border[3] - border[1]);
    for (unsigned y = 0; y < layout.height; ++y) {
        const float v = fraction(y, border[1], border[3]);
        const float mappedY = bounds[1] + v * ySpan;
        const auto left = interpolate(corners[0], corners[1], mappedY);
        const auto right = interpolate(corners[2], corners[3], mappedY);
        for (unsigned x = 0; x < layout.width; ++x) {
            auto& point = grid.points()[y * layout.width + x];
            const float u = fraction(x, border[0], border[2]);
            const bool inside = 0 <= v && v <= 1 && !(u < 0) && !(1 < u);
            if (inside) {
                point.anchor = interpolate(left, right, bounds[0] + u * xSpan);
                point.active = true;
                point.fixed = !detached;
                point.edge = !detached && (u == 0 || u == 1 || v == 0 || v == 1);
            } else {
                point.fixed = point.edge = false;
                point.active = !((bounds[0] > 0 && u < 0) || (bounds[1] > 0 && v < 0) ||
                    (bounds[2] < 1 && u > 1) || (bounds[3] < 1 && v > 1));
            }
            const float sx = x < unsigned(int(border[0])) ? steps[0] : unsigned(int(border[2])) < x ? steps[2] : xStep;
            const float sy = y < unsigned(int(border[1])) ? steps[1] : unsigned(int(border[3])) < y ? steps[3] : yStep;
            for (unsigned i = 0; i < point.linkCount; ++i) {
                const float a = sx * point.links[i].x, b = sy * point.links[i].y;
                point.links[i].length = refinedRoot(a * a + b * b);
            }
        }
    }
    cropBounds = bounds;
    return true;
}

void playersheets::Motion::damp(const MotionParameters& parameters) {
    const float factor = 1 - (1 - (detached ? parameters.detachedDamping : parameters.attachedDamping));
    for (auto& point : grid.points()) if (point.active && (detached || !point.edge))
        for (auto& value : point.velocity) value *= factor;
}

void playersheets::Motion::applyGravity(const MotionParameters& parameters) {
    if (detached) return;
    const float gravity = grid.layout().reversePlanes ? -parameters.gravity : parameters.gravity;
    for (auto& point : grid.points()) if (point.active && !point.fixed && !point.edge) point.velocity[1] += gravity;
}

void playersheets::Motion::constrainAnchors(const MotionParameters& parameters) {
    for (auto& point : grid.points()) {
        if (!point.fixed || point.edge || point.touched) continue;
        auto direction = subtract(point.anchor, point.position);
        const float distance = normalize(direction);
        if (!grid.layout().splitRows) {
            if (point.attracted || distance >= anchorSnap)
                constrain(point, parameters, direction, point.anchor, point.attracted ? -0.5f * distance : 0);
            else { point.position = point.anchor; point.velocity = {}; }
        } else if (!point.attracted) {
            constrain(point, parameters, direction, point.anchor, -distance * (anchorBase + point.weight * anchorWeight));
        }
    }
}

void playersheets::Motion::constrainNeighbors(const MotionParameters& parameters, const Vector* suctionPosition) {
    const auto& layout = grid.layout();
    const unsigned midX = (int(layout.border[0]) + int(layout.border[2])) >> 1;
    const unsigned midY = (int(layout.border[1]) + int(layout.border[3])) >> 1;
    for (unsigned row = 0; row < layout.height; ++row) {
        const unsigned y = row <= midY ? midY - row : row;
        for (unsigned column = 0; column < layout.width; ++column) {
            const unsigned x = column <= midX ? midX - column : column;
            auto& point = grid.points().at(y * layout.width + x);
            if (point.edge) { point.position = point.anchor; point.velocity = {}; continue; }
            for (unsigned i = 0; i < point.linkCount; ++i) {
                const auto& link = point.links[i];
                const int neighborX = int(x) + link.x, neighborY = int(y) + link.y;
                if (neighborX < 0 || neighborY < 0 || neighborX >= int(layout.width) || neighborY >= int(layout.height))
                    throw std::runtime_error("Original sheet constraint points outside its grid.");
                auto& neighbor = grid.points().at(neighborY * layout.width + neighborX);
                if (point.touched) point.velocity = {};
                if (!point.active) { point.position = {}; point.velocity = {}; }
                else if (layout.splitRows || !point.fixed) {
                    auto direction = subtract(point.position, neighbor.position);
                    normalize(direction);
                    constrain(point, parameters, direction, neighbor.position, link.length);
                }
                if (parameters.reciprocalConstraints && point.active && neighbor.active && !neighbor.edge && !neighbor.touched) {
                    auto direction = subtract(neighbor.position, point.position);
                    normalize(direction);
                    constrain(neighbor, parameters, direction, point.position, link.length);
                }
            }
            if (point.active && point.touched) {
                if (!suctionPosition) throw std::runtime_error("Captured sheet point requires its original suction owner position.");
                point.position = *suctionPosition;
            }
        }
    }
}

void playersheets::Motion::integrate() {
    for (auto& point : grid.points()) if (point.active && !point.touched)
        for (unsigned i = 0; i < 3; ++i) point.position[i] += point.velocity[i];
}

void playersheets::Motion::updateNormals() {
    const auto& layout = grid.layout();
    for (unsigned y = 0; y < layout.height; ++y) for (unsigned x = 0; x < layout.width; ++x) {
        auto& point = grid.points()[y * layout.width + x];
        if (!point.active) continue;
        const std::array<bool, 4> exists{x != 0, y != 0, x + 1 < layout.width, y + 1 < layout.height};
        const std::array<int, 4> offsets{-1, -int(layout.width), 1, int(layout.width)};
        std::array<Vector, 4> directions{};
        for (unsigned i = 0; i < 4; ++i) if (exists[i]) {
            directions[i] = subtract(grid.points()[y * layout.width + x + offsets[i]].position, point.position);
            normalize(directions[i]);
        }
        Vector normal{};
        unsigned count = 0;
        for (unsigned i = 0; i < 4; ++i) {
            const unsigned next = (i + 1) & 3;
            if (!exists[i] || !exists[next]) continue;
            auto face = cross(directions[next], directions[i]);
            normalize(face);
            for (unsigned axis = 0; axis < 3; ++axis) normal[axis] += face[axis];
            ++count;
        }
        if (!count) point.normal = {0, 1, 0};
        else { normalize(normal); point.normal = normal; }
    }
}

const playersheets::Plane* playersheets::Motion::collisionPlane(unsigned index) const {
    const auto& layout = grid.layout();
    if (index >= layout.width * layout.height) throw std::out_of_range("Sheet collision point.");
    const float u = fraction(index % layout.width, layout.border[0], layout.border[2]);
    const float v = fraction(index / layout.width, layout.border[1], layout.border[3]);
    if (0 <= v && v <= 1) return &grid.planes()[0 <= u ? (u <= 1 ? 0 : 3) : 1];
    if (0 <= u && u <= 1) {
        if (v < 0) return &grid.planes()[2];
        if (v > 1) return &grid.planes()[4];
    }
    return nullptr;
}

float playersheets::Motion::collide(Point& point, const Plane& plane, bool elastic) const {
    const float penetration = plane.distance + dot(point.position, plane.normal);
    if (!(penetration < 0)) return 0;
    for (unsigned i = 0; i < 3; ++i) point.position[i] += plane.normal[i] * -penetration;
    const float speed = dot(point.velocity, plane.normal);
    if (!(speed < 0)) return 1;
    const float response = elastic ? bounce * -speed : -speed;
    for (unsigned i = 0; i < 3; ++i) point.velocity[i] += plane.normal[i] * response;
    return -speed;
}
