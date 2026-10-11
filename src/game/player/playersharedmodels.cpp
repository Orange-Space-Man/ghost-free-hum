#include "game/player/playersharedmodels.h"

#include <algorithm>
#include <stdexcept>

namespace {
    std::vector<std::uint8_t> resource(const gamearchive::Archive& archive, const char* name) {
        const auto index = archive.findTypeResource(0x4b545f53, name);
        if (index < 0) {
            throw std::runtime_error(std::string("Missing original player shared resource: ") + name);
        }
        return archive.resource(static_cast<unsigned>(index));
    }

    std::shared_ptr<j3dmodel::ModelData> loadModel(const gamearchive::Archive& archive, const char* name,
        const j3dmodel::MaterialDefaults& defaults) {
        auto result = std::make_shared<j3dmodel::ModelData>();
        std::string error;
        if (!result->read(resource(archive, name), 0x54100000, defaults, error)) {
            throw std::runtime_error(std::string(name) + ": " + error);
        }
        j3dmodel::prepareMaterialAnimations(*result);
        return result;
    }

    std::shared_ptr<j3dmodel::Animation> loadAnimation(const gamearchive::Archive& archive, const char* name,
        j3dmodel::ModelData& model) {
        auto result = std::make_shared<j3dmodel::Animation>();
        std::string error;
        if (!result->read(resource(archive, name), error)) {
            throw std::runtime_error(std::string(name) + ": " + error);
        }
        j3dmodel::bindAnimation(model, result);
        return result;
    }
}

playermodels::DrawBuffer::DrawBuffer(unsigned capacity) : entries(capacity), interval((far - near) / static_cast<float>(capacity)) {
    clear();
}

void playermodels::DrawBuffer::clear() {
    std::fill(entries.begin(), entries.end(), nullptr);
}

playermodels::SharedModels::SharedModels(const gamearchive::Archive& common, const j3dmodel::MaterialDefaults& defaults,
    unsigned mapMode, const gamearchive::Archive* mapArchive) {
    for (const auto index : {0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 10, 11}) {
        buffers[index] = std::make_unique<DrawBuffer>(256);
    }
    models[0] = loadModel(common, "suikom_new1.bmd", defaults);
    animations[0] = loadAnimation(common, "suikom_new1.btk", *models[0]);
    animations[1] = loadAnimation(common, "suikom_new1.bpk", *models[0]);
    models[1] = loadModel(common, "handlight_ef.bmd", defaults);
    animations[2] = loadAnimation(common, "handlight_ef.btk", *models[1]);
    models[2] = loadModel(common, "pipe.bmd", defaults);
    models[3] = loadModel(common, "coin.bmd", defaults);
    models[4] = loadModel(common, "osatu.bmd", defaults);
    models[5] = loadModel(common, "osatu2.bmd", defaults);
    models[6] = loadModel(common, "osatu3.bmd", defaults);
    models[7] = loadModel(common, "leaf.bmd", defaults);
    models[8] = loadModel(common, "pile.bmd", defaults);
    models[9] = loadModel(common, "bed.bmd", defaults);
    models[10] = loadModel(common, "bedcv_1.bmd", defaults);
    animations[3] = loadAnimation(common, "bedcv_1.bpk", *models[10]);
    models[11] = loadModel(common, "scana.bmd", defaults);
    animations[4] = loadAnimation(common, "scana.btk", *models[11]);
    models[12] = loadModel(common, "warp.bmd", defaults);
    animations[5] = loadAnimation(common, "warp.bck", *models[12]);
    animations[6] = loadAnimation(common, "warp01.btk", *models[12]);
    animations[7] = loadAnimation(common, "warp02.btk", *models[12]);
    animations[8] = loadAnimation(common, "warp.brk", *models[12]);
    const char* textureNames[] = {"kurosu_1.bti", "tape_1.bti", "tape2.bti", "pos_1.bti", "kurosu05.bti", "bedcv_1.bti",
        "pos_2.bti", "bed02.bti", "kurosu03.bti", "kurosu04.bti", "sukurin.bti", "tape01.bti", "westan.bti",
        "tere_pos.bti", "monster.bti", "test.bti"};
    for (unsigned i = 0; i < textures.size(); ++i) {
        textures[i] = std::make_shared<const std::vector<std::uint8_t>>(resource(common, textureNames[i]));
    }
    if (mapMode == 9) {
        if (!mapArchive) {
            throw std::runtime_error("Original map-mode-9 player resources require the mission archive.");
        }
        models[13] = loadModel(*mapArchive, "koopa_sui.bmd", defaults);
        animations[9] = loadAnimation(*mapArchive, "koopa_sui.btk", *models[13]);
        animations[10] = loadAnimation(*mapArchive, "koopa_sui.bpk", *models[13]);
    }
}

const std::shared_ptr<j3dmodel::ModelData>& playermodels::SharedModels::model(SharedModel kind) const {
    return models.at(static_cast<unsigned>(kind));
}

const std::shared_ptr<j3dmodel::Animation>& playermodels::SharedModels::animation(unsigned index) const {
    return animations.at(index);
}

const std::shared_ptr<const std::vector<std::uint8_t>>& playermodels::SharedModels::texture(unsigned index) const {
    return textures.at(index);
}

playermodels::DrawBuffer& playermodels::SharedModels::buffer(unsigned index) {
    return *buffers.at(index);
}
