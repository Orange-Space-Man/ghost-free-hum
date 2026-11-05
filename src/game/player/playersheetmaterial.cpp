#include "playersheetmaterial.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    using playerscanner::Vector;

    Vector subtract(const Vector& a, const Vector& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
    float length(const Vector& value) { return std::sqrt((value[0] * value[0] + value[1] * value[1]) + value[2] * value[2]); }
    float normalize(Vector& value) {
        const float size = length(value);
        if (size >= 0.0001f) for (auto& component : value) component *= 1 / size;
        else value.fill(0);
        return size;
    }
    float coordinate(unsigned index, unsigned count, std::int16_t first, std::int16_t last,
        float before, float after, float span) {
        const float scale = 1 / ((after + before) + span);
        if (index < unsigned(first)) return ((float(index) * scale) * before) / float(first);
        if (unsigned(last) < index)
            return scale * ((before + span) + (after * float(index - unsigned(last))) / float(count - 1 - unsigned(last)));
        return scale * (before + (span * float(index - unsigned(first))) / float(int(last) - int(first)));
    }
    void number(std::array<std::uint8_t, 100>& bytes, unsigned offset, float value) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, 4);
        for (unsigned i = 0; i < 4; ++i) bytes.at(offset + i) = static_cast<std::uint8_t>(bits >> (24 - i * 8));
    }
    void invertProjection(std::array<float, 16>& matrix) {
        const float a = matrix[0], b = matrix[1], c = matrix[2], d = matrix[3];
        const float e = matrix[4], f = matrix[5], g = matrix[6], h = matrix[7];
        const float i = matrix[8], j = matrix[9], k = matrix[10], l = matrix[11];
        const float m = matrix[12], n = matrix[13], o = matrix[14], p = matrix[15];
        const float determinant = (c * h - d * g) * (i * n - j * m) +
            (((b * g - c * f) * (i * p - l * m) + (a * h - d * e) * (j * o - k * n) +
            ((a * f - b * e) * (k * p - l * o) - (a * g - c * e) * (j * p - l * n))) -
            (b * h - d * f) * (i * o - k * m));
        constexpr float threshold = 0x1p-18f;
        if (!(determinant > threshold || determinant < -threshold)) return;
        const float inverse = 1 / determinant;
        matrix[0] = inverse * (h * (j * o - k * n) + (f * (k * p - l * o) - g * (j * p - l * n)));
        matrix[1] = inverse * -(d * (j * o - k * n) + (b * (k * p - l * o) - c * (j * p - l * n)));
        matrix[2] = inverse * (d * (f * o - g * n) + (b * (g * p - h * o) - c * (f * p - h * n)));
        matrix[3] = inverse * -(d * (f * k - g * j) + (b * (g * l - h * k) - c * (f * l - h * j)));
        matrix[4] = inverse * -(h * (i * o - k * m) + (e * (k * p - l * o) - g * (i * p - l * m)));
        matrix[5] = inverse * (d * (i * o - k * m) + (a * (k * p - l * o) - c * (i * p - l * m)));
        matrix[6] = inverse * -(d * (e * o - g * m) + (a * (g * p - h * o) - c * (e * p - h * m)));
        matrix[7] = inverse * (d * (e * k - g * i) + (a * (g * l - h * k) - c * (e * l - h * i)));
    }
}

void playersheets::updateMaterial(Grid& grid, const Motion& motion, playermodels::GridModels& models) {
    if (!models.texture.primary) return;
    const auto& layout = grid.layout();
    const auto& border = layout.border;
    const float width = length(subtract(layout.corners[2], layout.corners[0]));
    const float height = length(subtract(layout.corners[1], layout.corners[0]));
    std::array<float, 4> hanging{};
    for (unsigned i = 0; i < 4; ++i) if (motion.drops[i] >= 0) hanging[i] = motion.drops[i];
    for (unsigned y = 0; y < layout.height; ++y) {
        const float v = coordinate(y, layout.height, border[1], border[3], hanging[1], hanging[3], height);
        for (unsigned x = 0; x < layout.width; ++x) grid.points()[y * layout.width + x].texture[1] = v;
    }
    for (unsigned x = 0; x < layout.width; ++x) {
        const float u = coordinate(x, layout.width, border[0], border[2], hanging[0], hanging[2], width);
        for (unsigned y = 0; y < layout.height; ++y) grid.points()[y * layout.width + x].texture[0] = u;
    }
    if (!models.materials[0] || !models.materials[1] || !layout.mode || motion.detached) return;
    if (layout.mode > 3) throw std::runtime_error("Invalid original cloth material mode.");
    const auto& first = grid.points().at(unsigned(border[1]) * layout.width + unsigned(border[0]));
    const auto& across = grid.points().at(unsigned(border[1]) * layout.width + unsigned(border[2]));
    const auto& down = grid.points().at(unsigned(border[3]) * layout.width + unsigned(border[0]));
    auto xAxis = subtract(across.position, first.position), yAxis = subtract(down.position, first.position);
    const float xLength = normalize(xAxis), yLength = normalize(yAxis);
    Vector normal{yAxis[1] * xAxis[2] - yAxis[2] * xAxis[1], yAxis[2] * xAxis[0] - yAxis[0] * xAxis[2],
        yAxis[0] * xAxis[1] - yAxis[1] * xAxis[0]};
    normalize(normal);
    const float xScale = xLength / (across.texture[0] - first.texture[0]);
    const float yScale = yLength / (down.texture[1] - first.texture[1]);
    Vector origin;
    for (unsigned i = 0; i < 3; ++i) {
        xAxis[i] *= xScale;
        yAxis[i] *= yScale;
        origin[i] = (first.position[i] + xAxis[i] * -first.texture[0]) + yAxis[i] * -across.texture[1];
    }
    std::array<float, 16> projection{xAxis[0], yAxis[0], normal[0], origin[0],
        xAxis[1], yAxis[1], normal[1], origin[1], xAxis[2], yAxis[2], normal[2], origin[2], 0, 0, 0, 1};
    invertProjection(projection);
    projection[8] = projection[9] = projection[10] = projection[12] = projection[13] = projection[14] = 0;
    projection[11] = projection[15] = 1;
    auto& material = *models.materials.at(layout.mode - 1);
    auto& texture = material.texture->matrices.at(models.slots.at(layout.mode - 1));
    if (!texture) throw std::runtime_error("Original cloth material requires its retained texture matrix.");
    if (!(material.identityFlags & 0xc0000000)) {
        if (!material.original) throw std::runtime_error("Original cloth material mutation requires its source material.");
        material.identityFlags |= 0x80000000;
        material.mode = material.original->mode;
        material.fieldE = material.original->fieldE;
        material.animation.reset();
    }
    for (unsigned i = 0; i < projection.size(); ++i) number(texture->info, 36 + i * 4, projection[i]);
    for (unsigned offset : {4U, 8U, 12U}) number(texture->info, offset, 0);
}

void playersheets::updateModelTransform(const Grid& grid, playermodels::GridModels& models,
    const playermodels::SharedModels& shared, const std::function<std::uint32_t(const Vector&)>& roomColor) {
    const unsigned mode = grid.layout().mode;
    if (!mode) return;
    if (mode > 3) throw std::runtime_error("Invalid original cloth model mode.");
    auto& instance = *models.instances.at(mode - 1);
    instance.scale = models.field1F8;
    instance.matrix = grid.matrix();
    if (mode == 3) {
        if (!roomColor) throw std::runtime_error("Bed-cover animation requires the original room color query.");
        const auto& matrix = grid.matrix();
        const auto color = roomColor({matrix[3], matrix[7], matrix[11]});
        shared.animation(3)->frame = 1 + (29 * float(255 - (color & 0xff))) / 255;
    }
}
