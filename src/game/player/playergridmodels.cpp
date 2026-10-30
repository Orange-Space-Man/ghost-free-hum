#include "game/player/playergridmodels.h"

#include <limits>
#include <stdexcept>

playermodels::GridPoint::GridPoint() : flags{}, field8(0), fieldA(0), fieldC(0), linkCount(0), field28(0),
    field2C{}, field38{}, field44{0, 1, 0}, field50(0), field54(0) {}

playermodels::GridParameters::GridParameters() : width(16), height(16), field8{-100, 150, -200}, field14{-100, 150, 0},
    field20{100, 120, -200}, field2C{100, 120, 0}, fields38{4, 4, 11, 11}, fields40{80, 80, 80, 80}, field50(0), field54(0),
    field58(30), field5C(45), fields60{10, 10, 10, 10}, field70(80), field74(0), field78(0), field7C(0), field7D(1), fieldF0(0) {}

playermodels::GridModels::GridModels(const SharedModels& shared, unsigned width, unsigned height) : width(width), height(height) {
    if (height && width > std::numeric_limits<unsigned>::max() / height / 0x58) {
        throw std::runtime_error("Original grid allocation exceeds its address range.");
    }
    count = width * height;
    points.reset(new GridPoint[count]);
    const SharedModel kinds[]{SharedModel::Pile, SharedModel::Bed, SharedModel::BedCover};
    for (unsigned i = 0; i < 3; ++i) instances[i] = std::make_unique<j3dmodel::ModelInstance>(shared.model(kinds[i]), 0x10000);
    for (unsigned i = 0; i < 3; ++i) {
        for (auto& material : instances[i]->model->materials.entries) {
            for (unsigned slot = 0; slot < 10; ++slot) {
                if (material->texture->matrices[slot]) {
                    materials[i] = material.get();
                    slots[i] = static_cast<std::uint8_t>(slot);
                    break;
                }
            }
            if (materials[i]) break;
        }
    }
}

playermodels::GridModels::~GridModels() {
    for (auto& instance : instances) instance.reset();
    points.reset();
}

playermodels::GridOwner::GridOwner(const SharedModels& shared) {
    for (auto& grid : grids) grid = std::make_unique<GridModels>(shared, 16, 16);
}

playermodels::GridOwner::~GridOwner() {
    for (auto& grid : grids) grid.reset();
}
