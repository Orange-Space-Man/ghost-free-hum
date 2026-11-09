#include "game/player/playersheetgeometry.h"

#include <stdexcept>

bool playersheets::draws(const playermodels::GridModels& grid, unsigned pass) {
    return !grid.field161 && (pass == 2 || (pass == 1 && grid.parameters.field7D));
}

std::vector<modeldraw::Vertex> playersheets::prepareGeometry(const playermodels::GridModels& grid) {
    if (!grid.width || !grid.height || grid.width > grid.count / grid.height)
        throw std::runtime_error("Original sheet dimensions exceed its point storage.");
    std::vector<modeldraw::Vertex> result;
    result.reserve((grid.width - 1) * (grid.height - 1) * 6);
    for (unsigned x = 0; x + 1 < grid.width; ++x) {
        for (unsigned y = 0; y + 1 < grid.height; ++y) {
            const unsigned corners[]{y * grid.width + x, y * grid.width + x + 1,
                (y + 1) * grid.width + x + 1, (y + 1) * grid.width + x};
            if (!grid.points[corners[0]].flags[0] || !grid.points[corners[1]].flags[0] || !grid.points[corners[3]].flags[0]) continue;
            for (unsigned corner : {0u, 1u, 2u, 0u, 2u, 3u}) {
                const auto& point = grid.points[corners[corner]];
                modeldraw::Vertex vertex{};
                vertex.position = point.field2C;
                vertex.normals[0] = point.field44;
                vertex.color = {255, 255, 255, 255};
                vertex.coordinates[0] = {point.field50, point.field54};
                result.push_back(vertex);
            }
        }
    }
    return result;
}
