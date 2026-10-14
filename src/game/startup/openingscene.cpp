#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/startup/openingscene.h"
#include "platform/d3d11/gamerenderer.h"
#include "platform/d3d11/gameshadowrenderer.h"

#include <cstring>
#include <algorithm>
#include <stdexcept>

bool openingscene::Scene::load(const std::filesystem::path& path, unsigned cutscene, std::string& error) {
    error.clear();
    try {
        Scene result;
        gameassets::Package package;
        std::vector<std::uint8_t> executable, table;
        if (!package.open(path, error) || !read(package, cutscene, result.resources, error) ||
            !package.read("sys/main.dol", executable, error) ||
            !dolresources::read(executable, glme01::sineTable, 5120 * 4, table, error)) throw std::runtime_error(error);
        result.trigonometry.resize(5120);
        for (unsigned i = 0; i < 5120; ++i) {
            const auto offset = i * 4;
            const unsigned bits = (unsigned(table[offset]) << 24) | (table[offset + 1] << 16) | (table[offset + 2] << 8) | table[offset + 3];
            std::memcpy(&result.trigonometry[i], &bits, 4);
        }
        result.prepared.resize(result.resources.segments.size());
        for (unsigned i = 0; i < result.prepared.size(); ++i) {
            result.timeline.segment = i;
            result.loadSegment();
        }
        result.reset();
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

void openingscene::Scene::loadSegment() {
    if (!prepared.at(timeline.segment).models.empty()) {
        const auto& cached = prepared.at(timeline.segment);
        models = cached.models;
        animations = cached.animations;
        camera = cached.camera;
        environment = cached.environment;
        shadow = cached.shadow;
        batches.clear();
        for (const auto& model : models) batches.push_back(model.batches);
        loadedSegment = timeline.segment;
        graphicsReady = false;
        return;
    }
    const auto& segment = resources.segments.at(timeline.segment);
    models.clear();
    animations.clear();
    batches.clear();
    std::string error;
    if (!camera.read(segment.scene.data, segment.settings.data, error) ||
        !environment.read(segment.scene.data, segment.settings.data, error) ||
        !shadow.read(segment.scene.data, segment.settings.data, error)) throw std::runtime_error(error);
    for (const auto& source : segment.models) {
        s3dmodel::Model model;
        if (!model.read(source.model.data, error, (source.flags & 2) != 0)) throw std::runtime_error(error);
        AnimationState state;
        state.flags = source.flags;
        for (const auto& resource : source.animations[0]) {
            s3danimation::Animation animation;
            if (!animation.read(resource.data, error)) throw std::runtime_error(error);
            state.skeletal.push_back(std::move(animation));
        }
        for (const auto& resource : source.animations[1]) {
            s3denvironment::ModelEffect effect;
            if (!effect.read(resource.data, model, error)) throw std::runtime_error(error);
            state.effects.push_back(std::move(effect));
        }
        state.active.resize(state.effects.size());
        state.effectFrames.resize(state.effects.size());
        state.effectDrawFrames.resize(state.effects.size());
        if (source.animations[2].size() != source.animations[3].size()) throw std::runtime_error("Mismatched opening morph resources.");
        for (unsigned i = 0; i < source.animations[2].size(); ++i) {
            s3dmorph::Animation morph;
            if (!morph.read(source.animations[2][i].data, source.animations[3][i].data, model.positions.size(), error)) throw std::runtime_error(error);
            state.morphs.push_back(std::move(morph));
        }
        state.morphActive.resize(state.morphs.size());
        state.morphFrames.resize(state.morphs.size());
        state.morphDrawFrames.resize(state.morphs.size());
        batches.push_back(model.batches);
        models.push_back(std::move(model));
        animations.push_back(std::move(state));
    }
    loadedSegment = timeline.segment;
    graphicsReady = false;
    prepared.at(timeline.segment) = {models, animations, camera, environment, shadow};
}

std::string openingscene::Scene::graphicsKey(unsigned segment) const {
    return "opening-" + std::to_string(resources.cutscene) + "-" + std::to_string(segment);
}

void openingscene::Scene::prepareGraphics() {
    for (unsigned i = 0; i < prepared.size(); ++i) {
        if (gamerenderer::useScene(graphicsKey(i))) continue;
        gamerenderer::setInterfaceTextures({});
        gamerenderer::loadPreviews(prepared[i].models, 640, 480);
        gamerenderer::cacheScene(graphicsKey(i));
    }
    graphicsReady = false;
}

void openingscene::Scene::reset() {
    timeline.reset(resources);
    stepped = false;
    loadSegment();
}

bool openingscene::Scene::clearsEventFlag() const { return timeline.clearFlag55; }

void openingscene::Scene::update(bool flag55) {
    if (stepped) timeline.afterDraw(resources);
    const bool changedSegment = loadedSegment != timeline.segment;
    if (changedSegment) loadSegment();
    timeline.update(resources, flag55);
    for (const auto& command : timeline.commands) {
        auto& state = animations.at(command.model);
        if (command.opcode == 0 && command.animation < state.skeletal.size()) {
            state.animation = command.animation;
            state.frame = command.frame;
        } else if (command.opcode == 1 && command.animation < state.effects.size()) {
            state.active[command.animation] = true;
            state.effectFrames[command.animation] = command.frame;
        } else if (command.opcode == 2 && command.animation < state.effects.size()) state.active[command.animation] = false;
        else if (command.opcode == 3 && command.animation < state.morphs.size()) {
            state.morphActive[command.animation] = true;
            state.morphFrames[command.animation] = command.frame;
        } else if (command.opcode == 8) state.flags |= 1;
        else if (command.opcode == 9) state.flags &= ~1U;
    }
    for (unsigned i = 0; i < models.size(); ++i) {
        auto& state = animations[i];
        state.drawFrame = state.frame;
        for (unsigned morph = 0; morph < state.morphs.size(); ++morph) if (state.morphActive[morph]) {
            state.morphDrawFrames[morph] = state.morphFrames[morph];
            state.morphFrames[morph] = state.morphs[morph].advance(state.morphFrames[morph], timeline.animationStep);
        }
        if (!state.skeletal.empty()) {
            const auto& animation = state.skeletal.at(state.animation);
            state.frame += timeline.animationStep;
            if (state.frame >= animation.frames) {
                if (animation.flags & 1) state.frame = float(animation.frames);
                else state.frame -= animation.frames;
            }
        }
        for (unsigned effect = 0; effect < state.effects.size(); ++effect) if (state.active[effect]) {
            state.effectDrawFrames[effect] = state.effectFrames[effect];
            state.effects[effect].apply(state.effectFrames[effect], models[i]);
            state.effectFrames[effect] = state.effects[effect].advance(state.effectFrames[effect], timeline.animationStep);
        }
        if (timeline.resetAnimations && !changedSegment) { state.animation = 0; state.frame = 0; }
    }
    stepped = true;
}

void openingscene::Scene::draw(bool present) {
    if (!graphicsReady) {
        if (!gamerenderer::useScene(graphicsKey(loadedSegment))) {
            gamerenderer::setInterfaceTextures({});
            gamerenderer::loadPreviews(models, 640, 480);
            gamerenderer::cacheScene(graphicsKey(loadedSegment));
        }
        texture = 0;
        graphicsReady = true;
    }
    const float sceneFrame = timeline.drawFrame;
    const auto view = camera.sample(sceneFrame);
    const auto lighting = environment.sample(sceneFrame);
    const auto fog = environment.sampleFog(sceneFrame);
    std::vector<std::vector<s3dmodel::Material>> materials;
    std::vector<std::vector<s3dmodel::Batch>> shadowBatches(models.size());
    for (unsigned i = 0; i < models.size(); ++i) {
        const auto& state = animations[i];
        materials.push_back(models[i].materials);
        for (unsigned morph = 0; morph < state.morphs.size(); ++morph) if (state.morphActive[morph])
            state.morphs[morph].apply(state.morphs[morph].advance(state.morphDrawFrames[morph], 0), models[i].positions);
        if (!state.skeletal.empty()) {
            const auto& animation = state.skeletal.at(state.animation);
            float frame = state.drawFrame;
            if (frame >= animation.frames) frame = (animation.flags & 1) ? float(animation.frames) : frame - animation.frames;
            const auto pose = animation.sample(frame);
            batches[i] = models[i].animate(pose, animation.flags, trigonometry);
            if (state.flags & 2) shadowBatches[i] = models[i].animate(pose, animation.flags, trigonometry, true);
        } else if (state.flags & 2) shadowBatches[i] = models[i].shadowBatches;
        for (unsigned effect = 0; effect < state.effects.size(); ++effect) if (state.active[effect])
            state.effects[effect].apply(state.effects[effect].advance(state.effectDrawFrames[effect], 0), models[i]);
    }
    gameshadowrenderer::begin(shadow.sample(sceneFrame), view.aspect);
    for (unsigned i = 0; i < models.size(); ++i)
        if ((animations[i].flags & 3) == 3 && !(timeline.flags & 16)) gameshadowrenderer::draw(shadowBatches[i]);
    gameshadowrenderer::finish();
    bool clear = true;
    const std::vector<s3dmodel::Batch> empty;
    for (unsigned pass = 0; pass < 2; ++pass) for (unsigned i = 0; i < models.size(); ++i) {
        const bool visible = (animations[i].flags & 1) && !(timeline.flags & 16);
        gamerenderer::drawPreview(i, models[i], visible ? batches[i] : empty, lighting, &view, clear, 1U << pass, (animations[i].flags & 4) != 0, trigonometry, &fog);
        clear = false;
    }
    for (unsigned i = 0; i < models.size(); ++i) models[i].materials = std::move(materials[i]);
    startupscene::Picture picture;
    picture.texture = texture;
    picture.width = 640;
    picture.height = 480;
    gamerenderer::drawInterface({picture}, present);
}

bool openingscene::Scene::finished() const { return timeline.finished; }

void openingscene::Scene::drawFrozen(bool present, unsigned fadeAlpha) {
    startupscene::Picture picture;
    picture.texture = texture;
    picture.width = 640;
    picture.height = 480;
    const float level = (255 - std::min(fadeAlpha, 255U)) / 255.0f;
    picture.white = {level, level, level, 1};
    gamerenderer::drawInterface({picture}, present);
}
