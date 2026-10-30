#include "../recovered/playersheetmaterial.h"
#include "../recovered/playersheettextures.h"
#include "game/scene/gameplayresourceowner.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
    float number(const std::array<std::uint8_t, 100>& bytes, unsigned offset) {
        const std::uint32_t bits = (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
            (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }
    float project(const j3dmodel::TextureMatrix& texture, unsigned row, const playerscanner::Vector& position) {
        const unsigned offset = 36 + row * 16;
        return ((number(texture.info, offset) * position[0] + number(texture.info, offset + 4) * position[1]) +
            number(texture.info, offset + 8) * position[2]) + number(texture.info, offset + 12);
    }
}

void checkSheetMaterial(gameplay::ResourceOwner& owner) {
    const auto& shared = *owner.managers().players().models;
    playermodels::GridModels models(shared, 16, 16);
    playersheets::Builder builder(owner.executable());
    playersheets::Grid grid(256);
    playerscanner::SheetInput input{0, 0, 0, 1, 0, {1, 0, 0, 20, 0, 1, 0, 30, 0, 0, 1, 40}, {100, 50, 100}, true, false};
    for (unsigned kind : {1U, 13U, 17U}) {
        input.kind = kind;
        const auto layout = builder.build(input, 256, 0, 13, false);
        grid.initialize(layout);
        playersheets::Motion motion(grid, owner.executable());
        playersheets::bindTextures(layout, shared, models);
        playersheets::updateMaterial(grid, motion, models);
        const auto& texture = *models.materials[layout.mode - 1]->texture->matrices[models.slots[layout.mode - 1]];
        for (unsigned y : {unsigned(layout.border[1]), unsigned(layout.border[3])}) {
            for (unsigned x : {unsigned(layout.border[0]), unsigned(layout.border[2])}) {
                const auto& point = grid.points()[y * layout.width + x];
                require(std::abs(project(texture, 0, point.position) - point.texture[0]) < 0.0001f &&
                    std::abs(project(texture, 1, point.position) - point.texture[1]) < 0.0001f,
                    "Original model material projection maps the cloth's anchored corners to its texture coordinates.");
            }
        }
        require(number(texture.info, 4) == 0 && number(texture.info, 8) == 0 && number(texture.info, 12) == 0 &&
            number(texture.info, 68) == 0 && number(texture.info, 72) == 0 && number(texture.info, 76) == 0 &&
            number(texture.info, 80) == 1 && number(texture.info, 96) == 1,
            "Cloth projection resets its texture center and original homogeneous rows.");
        const auto retained = texture.info;
        motion.detached = true;
        motion.drops[0] = -100;
        playersheets::updateMaterial(grid, motion, models);
        require(texture.info == retained && grid.points()[0].texture[0] == 0,
            "Detached cloth still updates point UVs while retaining its model material projection.");
        models.field1F8 = {2, 3, 4};
        unsigned calls = 0;
        playersheets::updateModelTransform(grid, models, shared, [&](const auto& position) {
            ++calls;
            require(position == playerscanner::Vector{grid.matrix()[3], grid.matrix()[7], grid.matrix()[11]},
                "Bed-cover lighting query uses the original sheet center.");
            return 0x123456ffU;
        });
        require(models.instances[layout.mode - 1]->matrix == grid.matrix() && models.instances[layout.mode - 1]->scale == models.field1F8,
            "Original sheet transform and retained scale reach the selected manager-owned model instance.");
        require(calls == (layout.mode == 3 ? 1U : 0U), "Only bed-cover mode queries room color.");
        if (layout.mode == 3) {
            require(shared.animation(3)->frame == 1, "Full room-color alpha selects the first bed-cover color frame.");
            playersheets::updateModelTransform(grid, models, shared, [](const auto&) { return 0xffffff00U; });
            require(shared.animation(3)->frame == 30, "Zero room-color alpha selects the original final color frame.");
        }
        motion.detached = false;
        for (auto& point : grid.points()) point.position = {11, 22, 33};
        playersheets::updateMaterial(grid, motion, models);
        require(number(texture.info, 36) == 0 && number(texture.info, 40) == 0 && number(texture.info, 44) == 0 &&
            number(texture.info, 48) == 11 && number(texture.info, 64) == 22,
            "Singular cloth projection keeps the original forward matrix instead of inserting an identity inverse.");
    }
    input.kind = 1;
    input.occupied = false;
    const auto layout = builder.build(input, 256, 0, 13, false);
    grid.initialize(layout);
    playersheets::Motion motion(grid, owner.executable());
    playersheets::bindTextures(layout, shared, models);
    const auto retained = models.materials[0]->texture->matrices[models.slots[0]]->info;
    playersheets::updateMaterial(grid, motion, models);
    require(grid.points().front().texture == std::array<float, 2>{0, 0} &&
        std::abs(grid.points().back().texture[0] - 1) < 0.0001f && std::abs(grid.points().back().texture[1] - 1) < 0.0001f,
        "Full cloth UV range includes both hanging edges.");
    require(models.materials[0]->texture->matrices[models.slots[0]]->info == retained,
        "Direct cloth mode does not change any model projection.");
    for (auto& instance : models.instances) instance->model->geometry.textureReplacements.clear();
}
