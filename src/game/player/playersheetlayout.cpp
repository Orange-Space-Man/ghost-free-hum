#include "playersheetlayout.h"
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
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }

    Vector subtract(const Vector& a, const Vector& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
    float dot(const Vector& a, const Vector& b) { return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2]; }
    float length(const Vector& a) { return std::sqrt(dot(a, a)); }
    float refinedRoot(float value) {
        if (!(0 < value)) return value;
        const double input = value;
        double estimate = 1.0 / std::sqrt(input);
        estimate = 0.5 * estimate * (3.0 - input * estimate * estimate);
        estimate = 0.5 * estimate * (3.0 - input * estimate * estimate);
        return static_cast<float>(input * 0.5 * estimate * (3.0 - input * estimate * estimate));
    }
    Vector cross(const Vector& a, const Vector& b) {
        return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    }
    Vector normalize(Vector value) {
        const float size = length(value);
        if (size >= 0.0001f) for (auto& component : value) component *= 1 / size;
        else value.fill(0);
        return value;
    }
    Vector interpolate(const Vector& a, const Vector& b, float t) {
        return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t};
    }
    Vector transform(const actormodel::Matrix& matrix, const Vector& value) {
        Vector result;
        for (unsigned i = 0; i < 3; ++i) result[i] =
            ((matrix[i * 4] * value[0] + matrix[i * 4 + 1] * value[1]) + matrix[i * 4 + 2] * value[2]) + matrix[i * 4 + 3];
        return result;
    }
    float fraction(unsigned value, int first, int last) {
        return (static_cast<float>(value) - static_cast<float>(first)) / static_cast<float>(last - first);
    }
    float clamp(float value) {
        if (value < 0) value = 0;
        if (1 < value) value = 1;
        return value;
    }
    unsigned unsignedBorder(std::int16_t value) { return static_cast<unsigned>(static_cast<int>(value)); }
    playersheets::Plane plane(const Vector& a, const Vector& b, const Vector& c, bool reverse) {
        const auto first = subtract(b, a), second = subtract(c, a);
        const auto normal = normalize(reverse ? cross(first, second) : cross(second, first));
        return {normal, -dot(normal, a)};
    }
}

playersheets::Builder::Builder(const std::vector<std::uint8_t>& executable) :
    diagonalScale(scalar(executable, 0x804dae30)), elasticRange(scalar(executable, 0x804dae34)),
    minimumRange(scalar(executable, 0x804dae38)), aspectThreshold(scalar(executable, 0x804dae3c)) {}

playersheets::Layout playersheets::Builder::build(const playerscanner::SheetInput& input, unsigned capacity,
    unsigned texture, unsigned secondaryTexture, bool reversePlanes) const {
    if (input.kind < 1 || input.kind > 24) throw std::runtime_error("Original sheet orientation requires a supported kind.");
    Layout result;
    result.texture = texture;
    result.secondaryTexture = secondaryTexture;
    result.reversePlanes = reversePlanes;
    const unsigned directions[]{0, 3, 1, 2, 0, 3, 1, 2, 2, 3, 1, 0,
        0, 3, 1, 2, 0, 3, 1, 2, 2, 3, 1, 0};
    unsigned direction = directions[input.kind - 1];
    const auto& extent = input.extent;
    const float drop = diagonalScale * extent[1];
    const bool vertical = (input.kind >= 9 && input.kind <= 12) || input.kind >= 21;
    if (input.kind <= 4) {
        result.mode = input.occupied ? 1 : 0;
        result.drops.fill(drop);
    } else if (input.kind <= 8) result.drops = {0, 0, 0, drop};
    else if (vertical) {
        result.drops.fill(0);
        result.splitRows = true;
        if (input.kind >= 21) result.anchored = false;
    } else {
        result.drops = {drop, 0, drop, 0};
        result.mode = input.kind < 17 ? 2 : 3;
        result.offsets.fill(0);
    }
    result.range = input.elastic ? 0.5f * drop : (minimumRange < drop ? drop : minimumRange);
    result.elasticity = input.elastic ? elasticRange : 0;
    if (!vertical) {
        for (unsigned i = 0; i < 4; ++i, ++direction) {
            const unsigned high = (direction & 3) >= 2;
            const Vector corner{(direction & 1) == high ? -extent[0] : extent[0], extent[1],
                (direction & 2) ? extent[2] : -extent[2]};
            const unsigned index = ((i & 1) ^ unsigned(i >= 2)) * 2 + (i >> 1);
            result.corners[index] = transform(input.matrix, corner);
        }
    } else {
        const unsigned parity = (unsigned(direction > 1) + 1) & 1;
        for (unsigned i = 0; i < 4; ++i) {
            Vector corner;
            corner[1] = i < 2 ? extent[1] : -extent[1];
            if (!(direction & 1)) {
                corner[0] = (i & 1) == parity ? -extent[0] : extent[0];
                corner[2] = direction & 2 ? extent[2] : -extent[2];
            } else {
                corner[0] = direction & 2 ? extent[0] : -extent[0];
                corner[2] = (i & 1) == parity ? extent[2] : -extent[2];
            }
            result.corners[(i & 1) * 2 + (i >> 1)] = transform(input.matrix, corner);
        }
    }
    const auto across = subtract(result.corners[0], result.corners[2]);
    const auto down = subtract(result.corners[0], result.corners[1]);
    const float a = dot(across, across), b = dot(down, down);
    result.width = static_cast<unsigned>(refinedRoot(static_cast<float>(capacity)));
    result.height = result.width;
    if (aspectThreshold * b < a) { result.height = result.width >> 1; result.width <<= 1; }
    else if (aspectThreshold * a < b) { result.height = result.width << 1; result.width >>= 1; }
    const auto width = static_cast<std::int16_t>(result.width), height = static_cast<std::int16_t>(result.height);
    if (input.kind <= 4) {
        const int border = std::max(2, static_cast<int>(std::min(result.width >> 2, result.height >> 2)));
        result.border = {static_cast<std::int16_t>(border), static_cast<std::int16_t>(border),
            static_cast<std::int16_t>(width - border - 2), static_cast<std::int16_t>(height - border - 2)};
    } else if (input.kind <= 8) result.border = {0, 0, static_cast<std::int16_t>(width - 1), 1};
    else if (input.kind >= 13 && input.kind <= 20) result.border = {
        static_cast<std::int16_t>((result.width >> 1) - 1), 0, static_cast<std::int16_t>(result.width >> 1),
        static_cast<std::int16_t>(height - 1)};
    else result.border = {0, 0, static_cast<std::int16_t>(width - 1), static_cast<std::int16_t>(height - 1)};
    return result;
}

playersheets::Grid::Grid(unsigned capacity) : capacity(capacity), vertices(capacity) {}
const playersheets::Layout& playersheets::Grid::layout() const { return parameters; }
const std::vector<playersheets::Point>& playersheets::Grid::points() const { return vertices; }
std::vector<playersheets::Point>& playersheets::Grid::points() { return vertices; }
const std::array<playersheets::Plane, 5>& playersheets::Grid::planes() const { return boundaries; }
const actormodel::Matrix& playersheets::Grid::matrix() const { return basis; }

void playersheets::Grid::initialize(const Layout& layout) {
    if (!layout.width || !layout.height || layout.width > capacity / layout.height)
        throw std::runtime_error("Sheet grid dimensions exceed the original point allocation.");
    parameters = layout;
    const auto& corners = layout.corners;
    const auto& border = layout.border;
    const auto across = normalize(subtract(corners[2], corners[0]));
    const auto down = normalize(subtract(corners[1], corners[0]));
    const auto normal = normalize(cross(down, across));
    const auto center = interpolate(interpolate(corners[0], corners[1], 0.5f), interpolate(corners[2], corners[3], 0.5f), 0.5f);
    for (unsigned i = 0; i < 3; ++i) {
        basis[i * 4] = across[i]; basis[i * 4 + 1] = normal[i];
        basis[i * 4 + 2] = down[i]; basis[i * 4 + 3] = center[i];
    }
    auto lowerFirst = corners[0], lowerLast = corners[3];
    lowerFirst[1] -= 100; lowerLast[1] -= 100;
    boundaries = {plane(corners[0], corners[2], corners[3], layout.reversePlanes),
        plane(corners[0], corners[1], lowerFirst, layout.reversePlanes),
        plane(corners[0], lowerFirst, corners[2], layout.reversePlanes),
        plane(corners[3], corners[2], lowerLast, layout.reversePlanes),
        plane(corners[3], lowerLast, corners[1], layout.reversePlanes)};
    for (unsigned i = 0; i < 4; ++i) boundaries[i + 1].distance -= layout.offsets[i];
    const float acrossLength = length(subtract(corners[2], corners[0]));
    const float downLength = length(subtract(corners[1], corners[0]));
    const std::array<float, 4> steps{layout.drops[0] / static_cast<float>(border[0]),
        layout.drops[1] / static_cast<float>(border[1]),
        layout.drops[2] / static_cast<float>((layout.width - 1) - static_cast<int>(border[2])),
        layout.drops[3] / static_cast<float>((layout.height - 1) - static_cast<int>(border[3]))};
    const float xStep = (acrossLength + std::min(0.0f, layout.drops[0]) + std::min(0.0f, layout.drops[2])) / (border[2] - border[0]);
    const float yStep = (downLength + std::min(0.0f, layout.drops[1]) + std::min(0.0f, layout.drops[3])) / (border[3] - border[1]);
    for (unsigned y = 0; y < layout.height; ++y) {
        const float v = fraction(y, border[1], border[3]);
        const auto left = interpolate(corners[0], corners[1], clamp(v));
        const auto right = interpolate(corners[2], corners[3], clamp(v));
        for (unsigned x = 0; x < layout.width; ++x) {
            auto& point = vertices[y * layout.width + x];
            auto& link = point.links[0];
            link.x = x < unsignedBorder(border[0]) ? 1 : unsignedBorder(border[2]) < x ? -1 : 0;
            link.y = y < unsignedBorder(border[1]) ? 1 : unsignedBorder(border[3]) < y ? -1 :
                layout.splitRows ? (y < (layout.height >> 1) ? 1 : -1) : 0;
            point.linkCount = 0;
            if (link.x || link.y) {
                point.linkCount = 1;
                point.links[1].x = point.links[1].y = 0;
                if (!link.x) { point.links[1].x = x < (layout.width >> 1) ? 1 : -1; ++point.linkCount; }
                else if (!link.y) { point.links[1].y = y < (layout.height >> 1) ? 1 : -1; ++point.linkCount; }
            }
            const float u = fraction(x, border[0], border[2]);
            point.active = true;
            point.fixed = 0 <= v && v <= 1 && !(u < 0) && !(1 < u);
            point.edge = point.fixed && (u == 0 || u == 1 || v == 0 || v == 1);
            if (point.fixed) point.anchor = interpolate(left, right, u);
            point.position = point.fixed ? point.anchor : interpolate(left, right, clamp(u));
            const float dx = x < unsignedBorder(border[0]) ? steps[0] * (unsignedBorder(border[0]) - x) :
                unsignedBorder(border[2]) < x ? steps[2] * (x - unsignedBorder(border[2])) : 0;
            const float dy = y < unsignedBorder(border[1]) ? steps[1] * (unsignedBorder(border[1]) - y) :
                unsignedBorder(border[3]) < y ? steps[3] * (y - unsignedBorder(border[3])) : 0;
            if (!point.fixed) point.position[1] -= refinedRoot(dx * dx + dy * dy);
            const float sx = x < unsignedBorder(border[0]) ? steps[0] : unsignedBorder(border[2]) < x ? steps[2] : xStep;
            const float sy = y < unsignedBorder(border[1]) ? steps[1] : unsignedBorder(border[3]) < y ? steps[3] : yStep;
            for (unsigned i = 0; i < point.linkCount; ++i) {
                const float a = sx * point.links[i].x, b = sy * point.links[i].y;
                point.links[i].length = refinedRoot(a * a + b * b);
            }
            point.velocity = {};
            point.touched = false;
            point.weight = 0;
            if (unsignedBorder(border[0]) <= x && x <= unsignedBorder(border[2]) &&
                unsignedBorder(border[1]) <= y && y <= unsignedBorder(border[3])) {
                const int midX = (int(border[2]) + int(border[0])) >> 1;
                const int midY = (int(border[3]) + 1 + int(border[1])) >> 1;
                const float a = 2 * std::abs(static_cast<float>(static_cast<int>(x) - midX)) / (int(border[2]) + 1 - int(border[0]));
                const float b = 2 * std::abs(static_cast<float>(static_cast<int>(y) - midY)) / (int(border[3]) + 1 - int(border[1]));
                point.weight = 1 - std::max(a, b);
            }
        }
    }
}
