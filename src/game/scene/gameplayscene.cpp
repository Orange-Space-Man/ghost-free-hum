#include "resources/dolresources.h"
#include "game/scene/gameplayscene.h"
#include "platform/d3d11/gamerenderer.h"
#include "game/models/modeldrawmatrices.h"
#include "game/models/modelshadowgeometry.h"
#include "platform/d3d11/gameshadowrenderer.h"
#include "game/startup/startupresources.h"
#include "game/effects/barriereffectpool.h"
#include "game/effects/cobwebeffectpool.h"

#include "jlib/j3d/j3dmodelcalculation.h"
#include "jlib/j3d/j3dmaterialcalculation.h"
#include "jlib/j3d/j3dmaterialdisplaylist.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
    const roomtraversal::Bounds fullBounds{0, 0, 640, 480};
    constexpr unsigned roomLightingEnabledAddress = 0x804d07c8;
    constexpr unsigned equipmentAmbientScaleAddress = 0x804d9b60;
    constexpr unsigned lightProjectionDepthAddress = 0x804dabc4;
    constexpr unsigned actorHighlightColorAddress = 0x804da4e0;
    constexpr unsigned actorHighlightAttenuationAddress = 0x804da4e8;
    constexpr unsigned specularDistanceAddress = 0x804dd7c0;

    template<class T> T constant(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, sizeof(T), bytes, error)) throw std::runtime_error(error);
        std::reverse(bytes.begin(), bytes.end());
        T value;
        std::memcpy(&value, bytes.data(), sizeof(value));
        return value;
    }

    std::array<float, 3> transform(const dooranimation::Matrix& matrix, const std::array<float, 3>& vector, bool position) {
        std::array<float, 3> result;
        for (unsigned i = 0; i < 3; ++i) result[i] = matrix[i * 4] * vector[0] + matrix[i * 4 + 1] * vector[1] +
            matrix[i * 4 + 2] * vector[2] + (position ? matrix[i * 4 + 3] : 0);
        return result;
    }
}

gameplay::Scene::Scene(const std::filesystem::path& package, gameentry::State& entry) : resources(package, entry), packagePath(package),
    keyPresentation(package, resources.executable(), sceneControl, resources.managers().flags(), resources.managers().music(),
        *resources.managers().playback()) {
    resources.bindPlayerDemo([this](std::string_view name) { requestPlayerDemo(name); });
    resources.bindControllerInstructions([this] { requestControllerInstructions(); });
    std::string error;
    if (!trig.read(resources.executable(), error) || !poseTables.read(resources.executable(), error)) throw std::runtime_error(error);
    textureTrigonometry.resize(5120);
    for (unsigned i = 0; i < textureTrigonometry.size(); ++i) textureTrigonometry[i] = poseTables.sineAngle(i * 16);
    opacityDistance = constant<float>(resources.executable(), 0x804d9abc);
    limitedOpacity = constant<std::uint8_t>(resources.executable(), 0x804d0050);
    for (unsigned i = 0; i < 3; ++i) {
        lightOrigin[i] = constant<float>(resources.executable(), 0x8020be30 + i * 4);
        lightTarget[i] = constant<float>(resources.executable(), 0x8020be3c + i * 4);
    }
    actorLight.position.fill(constant<float>(resources.executable(), 0x804da4cc));
    const auto lightColor = constant<std::uint32_t>(resources.executable(), 0x804da4c8);
    for (unsigned i = 0; i < 3; ++i) actorLight.color[i] = ((lightColor >> (24 - i * 8)) & 255) / 255.0f;
    const auto lightDistance = constant<float>(resources.executable(), 0x804da4d8);
    const auto lightBrightness = constant<float>(resources.executable(), 0x804da4dc);
    if (lightDistance >= 0 && lightBrightness > 0 && lightBrightness < 1)
        actorLight.distance[1] = (1 - lightBrightness) / (lightBrightness * lightDistance);
    actorHighlight.primarySpecular = true;
    const auto highlightColor = constant<std::uint32_t>(resources.executable(), actorHighlightColorAddress);
    std::array<float, 3> highlightDirection{poseTables.sineAngle(0x4000), lightBrightness, poseTables.sineAngle(0)};
    float length = 0;
    for (const auto component : highlightDirection) length += component * component;
    length = std::sqrt(length);
    const float specularDistance = constant<float>(resources.executable(), specularDistanceAddress);
    for (unsigned i = 0; i < 3; ++i) {
        highlightDirection[i] /= length;
        actorHighlight.color[i] = ((highlightColor >> (24 - i * 8)) & 255) / 255.0f;
        actorHighlight.position[i] = -highlightDirection[i] * specularDistance;
        actorHighlight.direction[i] = -highlightDirection[i] + (i == 2 ? 1.0f : 0.0f);
    }
    length = 0;
    for (const auto component : actorHighlight.direction) length += component * component;
    for (auto& component : actorHighlight.direction) component /= std::sqrt(length);
    actorHighlight.angular = {0, 0, lightBrightness};
    actorHighlight.distance = {constant<float>(resources.executable(), actorHighlightAttenuationAddress), 0,
        constant<float>(resources.executable(), actorHighlightAttenuationAddress + 4)};
    publishCamera();
    auto& managers = resources.managers();
    useRoomLights = constant<std::uint32_t>(resources.executable(), roomLightingEnabledAddress) != 0;
    equipmentAmbientScale = constant<float>(resources.executable(), equipmentAmbientScaleAddress);
    lightProjectionDepth = constant<float>(resources.executable(), lightProjectionDepthAddress);
    modelTransforms = std::make_unique<j3dmodel::TransformTables>(resources.executable());
    effectLighting = std::make_unique<modeleffects::Lighting>(resources.executable());
    particleBillboards = std::make_unique<particles::Billboards>(resources.executable());
    j3dmodel::MaterialLight coneLight;
    for (unsigned i = 0; i < 3; ++i) {
        coneLight.position[i] = constant<float>(resources.executable(), 0x8020c660 + i * 16);
        coneLight.direction[i] = constant<float>(resources.executable(), 0x8020c684 + i * 16);
    }
    const auto coneColor = constant<std::uint32_t>(resources.executable(), 0x804db454);
    for (unsigned i = 0; i < 4; ++i) coneLight.color[i] = static_cast<std::uint8_t>(coneColor >> (24 - i * 8));
    const float coneOne = constant<float>(resources.executable(), 0x804db458);
    const float coneZero = constant<float>(resources.executable(), 0x804db448);
    coneLight.attenuation = {coneOne, coneZero, coneZero, coneOne, coneZero, coneZero};
    lightMaterialState.light(7, coneLight);
    for (unsigned channel = 0; channel < 2; ++channel) {
        const auto color = constant<std::uint32_t>(resources.executable(), 0x804db44c + channel * 4);
        std::array<std::uint8_t, 4> bytes;
        for (unsigned i = 0; i < 4; ++i) bytes[i] = static_cast<std::uint8_t>(color >> (24 - i * 8));
        lightMaterialState.ambientColor(channel, bytes);
    }
    const auto* player = managers.playerEvents().player(0);
    ambient = resources.rooms().entries().at(player->room() >> 24).ambient;
    managers.updateShadow();
    managers.updatePlacement(0);
    resources.furniture().initializeObjects();
    rooms = resources.prepareVisibility(visibilityProjection, fullBounds);
    updateOpacity();
    for (auto* actor : managers.actors().entries()) {
        if (!actor || !actor->model || !actor->model->model) continue;
        const auto room = actor->parent ? actor->parent->room() : actor->room();
        if (room >> 24 == 255 || !resources.visibility().marked(room >> 24, 0, 0)) continue;
        s3dmodel::Model model;
        if (!model.read(actor->model->model->bytes, error, true)) throw std::runtime_error(error);
        actors.push_back(actor);
        actorModels.push_back(std::move(model));
        actorModelResources.push_back(actor->model->model);
    }
    if (actors.empty()) throw std::runtime_error("First-area scene has no manager-owned visible actors.");
}

gameplay::Scene::~Scene() {
    if (instructionGraphics) gamerenderer::releaseScene("controller-instructions");
}

void gameplay::Scene::bindControllerOptions(gamesave::State& saves, const std::uint64_t* presentationClock) {
    if (instructions.active()) throw std::logic_error("Cannot replace options while controller instructions are open.");
    if (!saves.initialized) throw std::invalid_argument("Controller instructions require initialized save options.");
    controllerOptions = &saves;
    instructionClock = presentationClock;
}

void gameplay::Scene::requestControllerInstructions() {
    if (!controllerOptions || !sceneControl.gameplayEnabled || instructions.active())
        throw std::logic_error("Controller instructions require available gameplay and bound options.");
    auto bindings = std::make_unique<controllerinstructions::GameplayServices>(*controllerOptions,
        resources.managers().playerEvents(), [this](unsigned id) { resources.managers().playback()->playGlobal(id); });
    std::string error;
    if (!instructions.open(packagePath, error)) throw std::runtime_error(error);
    instructionServices = std::move(bindings);
    instructionUpdated = false;
    presentationButtons = 0;
}

bool gameplay::Scene::controllerInstructionsActive() const { return instructions.active(); }

void gameplay::Scene::prepareGraphics() {
    gamerenderer::setInterfaceTextures(resources.interfaceTextures());
    textures = gamerenderer::loadPreviews(actorModels, 640, 480);
    gamerenderer::cacheScene("gameplay");
    if (models.empty()) prepareModelGraphics(gamerenderer::modelDevice());
}

void gameplay::Scene::prepareModelGraphics(ID3D11Device* device) {
    particleRenderer.load(device);
    pointRenderer.load(device);
    sheetRenderer.load(device);
    mirrorRenderer.load(device, resources.mirror());
    particleRenderer.prepare(resources.managers().particles().pool());
    particleRenderer.prepare(resources.managers().particles().runningDust().material());
    particleRenderer.prepare(resources.managers().particles().elemental().blowMaterial());
    for (auto kind : {playermodels::SharedModel::Coin, playermodels::SharedModel::Bill,
        playermodels::SharedModel::Bill2, playermodels::SharedModel::Bill3,
        playermodels::SharedModel::Pipe, playermodels::SharedModel::Vacuum}) {
        const auto& model = resources.managers().players().models->model(kind);
        effectRenderers[model.get()].load(device, model);
    }
    lightRenderer.load(device, resources.managers().players().models->model(playermodels::SharedModel::Light));
    for (auto* pool : resources.managers().modelEffects().registered()) {
        if (typeid(*pool) != typeid(modeleffects::Pool) && !dynamic_cast<modeleffects::BarrierPool*>(pool) && !dynamic_cast<modeleffects::CobwebPool*>(pool)) continue;
        for (const auto& owner : pool->owners) {
            const auto& model = owner->model->model;
            if (!effectRenderers.count(model.get())) effectRenderers[model.get()].load(device, model);
            if (auto* web = dynamic_cast<modeleffects::CobwebOwner*>(owner.get())) {
                const auto& secondary = web->secondary->model;
                effectRenderers[secondary.get()].load(device, secondary);
            }
        }
    }
    for (const auto& resident : resources.residentRooms().slots()) {
        if (resident.room == 0xffffffff) continue;
        prepareRoomGraphics(device, resident);
    }
    for (unsigned i = 0; i < resources.managers().doors().size(); ++i) {
        const auto& pose = resources.managers().doors().leaf(i).pose;
        prepareModel(device, pose.model(pose.selectedModel()));
        prepareModel(device, pose.model(0));
    }
}

void gameplay::Scene::prepareModel(ID3D11Device* device, const doors::ModelResource& model) {
    if (models.count(&model)) return;
    auto& batches = models[&model];
    batches.load(device, model);
    modellighting::State lighting;
    modellighting::initialize(lighting, view, {}, useRoomLights, ambient, trig);
    const auto& shadow = resources.managers().shadowProjection();
    modellighting::prepareModel(lighting, view, shadow.position, shadow.target, shadow.spotlight, trig);
    modelrenderer::PipelineState warmPipeline;
    modelrenderer::initializeModelState(warmPipeline);
    warmPipeline.scissor = true;
    std::array<modelrenderer::StageRegisters, 16> registers{};
    modeldraw::TraversalState state;
    state.overrideOpacity = true;
    for (unsigned pass = 0; pass < 3; ++pass) {
        modeldraw::traverse(model.drawing, pass, 255, state, [&](const modeldraw::DrawCommand& command) {
            batches.prepare(command, pass, lighting, warmPipeline, registers, true);
        });
    }
    for (const auto flags : {0u, 1u}) {
        state.lightingFlags = static_cast<std::uint8_t>(flags);
        modeldraw::traverse(model.drawing, 2, 100, state, [&](const modeldraw::DrawCommand& command) {
            batches.prepare(command, 2, lighting, warmPipeline, registers, true);
        });
    }
}

void gameplay::Scene::prepareRoomGraphics(ID3D11Device* device, const roomresidency::Resident& resident) {
    prepareModel(device, resident.drawing);
    for (const auto& model : resident.furniture.models()) prepareModel(device, model);
}

bool gameplay::Scene::updateRoomResources() {
    auto* device = gamerenderer::modelDevice();
    const bool changed = resources.updateRooms([&](const roomresidency::Resident& resident) {
        models.erase(&resident.drawing);
        for (const auto& model : resident.furniture.models()) models.erase(&model);
    }, [&](const roomresidency::Resident& resident) {
        if (device) prepareRoomGraphics(device, resident);
    });
    if (changed && device) {
        gamerenderer::setInterfaceTextures(resources.interfaceTextures());
        textures = gamerenderer::loadPreviews(actorModels, 640, 480);
        gamerenderer::cacheScene("gameplay");
    }
    return changed;
}

void gameplay::Scene::update(const playerinput::PadStatus& pad, std::int16_t port, std::uint32_t audioFrame, bool gameplayTick) {
    resources.managers().sampleInput(pad, port);
    auto& managers = resources.managers();
    managers.setAudioInterfaceFrame(audioFrame);
    if (instructions.active()) {
        instructions.update(managers.controller().pressed, instructionClock ? *instructionClock : presentationFrame, *instructionServices);
        instructionUpdated = true;
        if (instructions.active()) return;
        instructionServices.reset();
        if (instructionGraphics) {
            gamerenderer::useScene("gameplay");
            gamerenderer::releaseScene("controller-instructions");
            instructionGraphics = false;
        }
    }
    presentationButtons |= managers.controller().pressed;
    if (!gameplayTick) return;
    if (!sceneControl.gameplayEnabled) return;
    managers.lights().beginFrame();
    const bool cameraWasActive = resources.eventCameraActive();
    resources.updateEvents(ticks / 30, audioFrame);
    if (!cameraWasActive && resources.eventCameraActive()) {
        flaggedOpacity = 0;
        limitedOpacity = 255;
    }
    managers.updateGenerators();
    const std::array<playermotion::PlatformMotion, 2> platforms{};
    PlayerMainServices playerServices{nullptr, nullptr,
        {managers.playerLife(), false, ticks, {}, {}, {}, {}}, platforms};
    playerServices.keyPresentationActive = keyPresentationActive();
    playerServices.presentKey = [this](bool missingKey, bool front, std::uint8_t type) { presentKey(missingKey, front, type); };
    auto* player = managers.playerEvents().player(0);
    for (unsigned i = 0; i < managers.enemies().slots().size(); ++i) {
        const auto& slot = managers.enemies().slots()[i];
        if (slot.state == 2 && slot.owner && &slot.owner->actor() == player) managers.updatePlayerMain(0, playerServices);
        else managers.updateOwner(i);
    }
    managers.updateContacts();
    updateRoomResources();
    resources.updateCamera();
    publishCamera();
    rooms = resources.prepareVisibility(visibilityProjection, fullBounds);
    managers.updateDoors(ticks);
    managers.updateFurniture(ticks);
    auto targetAmbient = resources.rooms().entries().at(player->room() >> 24).ambient;
    if (player->view.field8B8 && player->view.field8A0 != 5)
        for (unsigned channel = 0; channel < 3; ++channel)
            targetAmbient[channel] = static_cast<std::uint8_t>(equipmentAmbientScale * targetAmbient[channel]);
    for (unsigned channel = 0; channel < 3; ++channel) {
        const int difference = int(targetAmbient[channel]) - ambient[channel];
        ambient[channel] = static_cast<std::uint8_t>(int(ambient[channel]) + std::clamp(difference, -10, 10));
    }
    updateOpacity();
    if (useRoomLights) managers.updateStorm();
    else managers.updateShadow();
    managers.updateRoomLights();
    managers.updatePoses();
    managers.completePlayerFrame({}, false);
    managers.refreshParticles();
    managers.updatePlacement(ticks / 30, false);
    managers.glows().update();
    managers.modelEffects().update(resources.rooms(), resources.visibility());
    const auto& moneyCamera = resources.camera();
    managers.updateMoney(moneyCamera.position, {moneyCamera.target[0] - moneyCamera.position[0],
        moneyCamera.target[1] - moneyCamera.position[1], moneyCamera.target[2] - moneyCamera.position[2]});
    managers.particles().update();
    resources.hud().update(player->view, resources.fullscreen(), managers.roomCleared(player->room() >> 24));
    resources.hud().updateGhosts(managers.ghostHealth(), ticks);
    resources.hud().updateDemoPanel(player->view, sceneControl.demoDisplay, resources.messages().visible());
    resources.updateMessages();
    ++ticks;
}

void gameplay::Scene::updateAudio() { resources.managers().updateAudio(view); }
std::shared_ptr<jaudio::PositionalPlayback> gameplay::Scene::playback() { return resources.managers().playback(); }

unsigned gameplay::Scene::updates() const { return ticks; }

const playeractor::View& gameplay::Scene::playerView(unsigned index) {
    const auto* player = resources.managers().playerEvents().player(static_cast<std::int32_t>(index));
    if (!player) throw std::out_of_range("Scene player view requires a registered player.");
    return player->view;
}

bool gameplay::Scene::presentKey(bool missingKey, bool front, unsigned doorType) {
    return keyPresentation.request(missingKey, front, doorType);
}

bool gameplay::Scene::requestPlayerDemo(std::string_view name) { return keyPresentation.requestNamed(name); }

bool gameplay::Scene::keyPresentationActive() const { return keyPresentation.state().owned; }
bool gameplay::Scene::gameplaySuspended() const { return !sceneControl.gameplayEnabled || instructions.active(); }


void gameplay::Scene::updateOpacity() {
    auto& managers = resources.managers();
    const auto* player = managers.actors().actor(managers.enemies().appearPoint(0).actor);
    if (!player) throw std::runtime_error("Room opacity requires the manager-owned player.");
    const auto room = player->room() >> 24;
    flaggedOpacity = 0;
    if (resources.roomFlags().at(room) & 4) {
        const float maximumZ = static_cast<float>(resources.rooms().entries().at(room).bounds[5]);
        flaggedOpacity = static_cast<std::uint8_t>(std::clamp(static_cast<int>(opacityDistance - (maximumZ - player->position[2])), 0, 255));
    }
    limitedOpacity = limitedOpacity < 201 ? limitedOpacity + 10 : 255;
    resources.furniture().updateOpacity(player->position[2]);
}

void gameplay::Scene::drawModel(const doors::ModelResource& model,
    const std::function<const dooranimation::Matrix&(unsigned)>& matrix, unsigned pass, std::uint8_t opacity,
    unsigned room, const roomtraversal::Bounds& bounds, bool lowOpacity) {
    auto* context = gamerenderer::modelContext();
    gamerenderer::bindSceneTarget(false, &drawViewport);
    const D3D11_RECT rectangle{bounds[0], bounds[1], bounds[2], bounds[3]};
    context->RSSetScissorRects(1, &rectangle);
    modellighting::State lighting;
    modellighting::initialize(lighting, view, resources.managers().lights().lights(), useRoomLights, ambient, trig);
    const auto& shadow = resources.managers().shadowProjection();
    modellighting::prepareModel(lighting, view, shadow.position, shadow.target, shadow.spotlight, trig);
    modeldraw::TraversalState state;
    state.lightingFlags = lowOpacity ? 1 : 0;
    state.overrideOpacity = overrideRoomOpacity;
    state.nodeLightingOverride = resources.managers().roomCleared(room);
    state.flaggedOpacity = flaggedOpacity;
    state.limitedOpacity = limitedOpacity;
    gameshadowrenderer::bindModel(view);
    modeldraw::traverse(model.drawing, pass, opacity, state, [&](const modeldraw::DrawCommand& command) {
        modelrenderer::VertexMatrices matrices;
        matrices.modelView = dooranimation::concatenate(view, matrix(command.node));
        if (!modeldraw::normalMatrix(matrices.modelView, matrices.normal)) throw std::runtime_error("Singular first-area model transform.");
        matrices.projection = projection;
        models.at(&model).draw(context, command, pass, 0, matrices, trig, lighting, pipeline, {}, retained, true);
    });
}

void gameplay::Scene::updatePresentation(bool gameplayTick) {
    ++presentationFrame;
    if (instructions.active()) return;
    if (!gameplayTick && sceneControl.gameplayEnabled) return;
    keyPresentation.advanceDraw(presentationButtons);
    presentationButtons = 0;
}

void gameplay::Scene::draw(bool present) {
    if (instructions.active()) {
        if (!instructionUpdated) { gamerenderer::drawInterface({}, present); return; }
        const auto& screen = instructions.screen();
        if (!instructionGraphics) {
            gamerenderer::setInterfaceTextures(screen.graphics().textures);
            gamerenderer::cacheScene("controller-instructions");
            instructionGraphics = true;
        } else if (!gamerenderer::useScene("controller-instructions"))
            throw std::runtime_error("Controller instruction graphics ownership is unavailable.");
        gamerenderer::drawInterface(screen.graphics().pictures(screen.layout()), present);
        return;
    }
    const bool gameplayDrawn = sceneControl.gameplayEnabled;
    if (gameplayDrawn) drawGameplay(false);
    const bool movieReady = keyPresentation.state().owned && keyPresentation.state().updated;
    if (movieReady && keyPresentation.state().mode != 1) keyPresentation.draw(present);
    else {
        if (movieReady) {
            gamerenderer::prepareDemoScreen("gameplay", resources.hud().demoViewport());
            keyPresentation.drawOverlay("gameplay", resources.hud().demoViewport());
        }
        startupscene::Picture picture;
        if (!gameplayDrawn && !gamerenderer::useScene("gameplay"))
            throw std::runtime_error("Retained gameplay graphics ownership is unavailable.");
        picture.texture = textures.at(0);
        picture.width = 640;
        picture.height = 480;
        std::vector<startupscene::Picture> pictures{picture};
        const auto ghostHealth = resources.hud().ghostPictures(view, projection, {fullBounds[0], fullBounds[1], fullBounds[2], fullBounds[3]});
        pictures.insert(pictures.end(), ghostHealth.begin(), ghostHealth.end());
        const auto hud = resources.hud().pictures();
        pictures.insert(pictures.end(), hud.begin(), hud.end());
        const auto messages = resources.messagePictures();
        pictures.insert(pictures.end(), messages.begin(), messages.end());
        gamerenderer::drawInterface(pictures, present);
    }
}

void gameplay::Scene::publishCamera() {
    const auto camera = resources.publishCamera();
    if (camera.visibilityMode != 0 && camera.visibilityMode != 2)
        throw std::runtime_error("Gameplay requires the selected camera visibility traversal.");
    overrideRoomOpacity = camera.visibilityMode == 2;
    view = camera.view;
    projection = camera.projection;
    visibilityProjection = camera.worldProjection;
    for (unsigned column = 0; column < 4; ++column) projection[8 + column] += projection[12 + column];
}

void gameplay::Scene::drawGameplay(bool present) {
    if (textures.empty()) throw std::runtime_error("Gameplay graphics are not prepared.");
    if (!gamerenderer::useScene("gameplay")) throw std::runtime_error("Gameplay graphics ownership is unavailable.");
    refreshActorModels();
    drawShadows();
    if (resources.mirror().visible(resources.visibility())) {
        const auto retainedView = view;
        const auto retainedProjection = projection;
        const auto retainedOpacity = overrideRoomOpacity;
        auto retainedRooms = std::move(rooms);
        const auto reflected = resources.mirror().camera(resources.camera());
        view = reflected.view;
        projection = reflected.projection;
        for (unsigned column = 0; column < 4; ++column) projection[8 + column] += projection[12 + column];
        overrideRoomOpacity = true;
        const auto width = static_cast<std::int16_t>(resources.mirror().width());
        const auto height = static_cast<std::int16_t>(resources.mirror().height());
        const roomtraversal::Bounds mirrorBounds{static_cast<std::int16_t>(320 - width), static_cast<std::int16_t>(240 - height),
            static_cast<std::int16_t>(320 + width), static_cast<std::int16_t>(240 + height)};
        rooms = resources.prepareVisibility(reflected.worldProjection, mirrorBounds, 5);
        drawWorld(true, reflected.camera);
        mirrorRenderer.capture(gamerenderer::modelContext(), gamerenderer::sceneTexture());
        view = retainedView;
        projection = retainedProjection;
        overrideRoomOpacity = retainedOpacity;
        rooms = std::move(retainedRooms);
        resources.managers().doors().restorePresentation(0);
    }
    drawWorld(false, resources.camera());
    startupscene::Picture picture;
    picture.texture = textures[0];
    picture.width = 640;
    picture.height = 480;
    gamerenderer::drawInterface({picture}, present);
}

void gameplay::Scene::drawShadows() {
    gameshadowrenderer::begin(resources.managers().shadowProjection(), 4.0f / 3.0f);
    const auto shadowRooms = resources.prepareVisibility(gameshadowrenderer::worldProjection(), fullBounds, 2);
    const auto submit = [&](const doors::ModelResource& model,
        const std::function<const dooranimation::Matrix&(unsigned)>& matrix, unsigned pass, std::uint8_t opacity) {
        modeldraw::TraversalState state;
        state.mode = 1;
        state.overrideOpacity = overrideRoomOpacity;
        state.flaggedOpacity = flaggedOpacity;
        state.limitedOpacity = limitedOpacity;
        std::vector<s3dmodel::Batch> batches;
        modeldraw::traverse(model.drawing, pass, opacity, state, [&](const modeldraw::DrawCommand& command) {
            batches.push_back(modelshadow::prepare(model, command, matrix(command.node)));
        });
        gameshadowrenderer::draw(batches, true);
    };
    for (const auto& room : shadowRooms) for (const auto& resident : resources.residentRooms().slots()) {
        if (resident.room != (room.room & 255) || room.marker != -1) continue;
        for (unsigned pass = 0; pass < 2; ++pass)
            submit(resident.drawing, [&](unsigned node) -> const dooranimation::Matrix& {
                return resident.drawing.initialMatrices.at(node);
            }, pass, 255);
    }
    resources.managers().doors().submitOpaque(2, false, [&](const doors::ModelResource& model,
        const doors::PoseOwner& pose, unsigned pass, std::uint8_t opacity, bool) {
        submit(model, [&](unsigned node) -> const dooranimation::Matrix& { return pose.matrix(node); }, pass, opacity);
    });
    for (const auto& object : resources.furniture().slots()) {
        if ((object.flags & 3) != 3 || !object.model || !resources.visibility().marked(object.room >> 24, 2, 0) ||
            resources.visibility().rawMarker(object.room >> 24, 2) != -1 || object.opacity != 255) continue;
        for (unsigned pass = 0; pass < 2; ++pass)
            submit(*object.model, [&](unsigned node) -> const dooranimation::Matrix& { return object.matrices.at(node); }, pass, 255);
    }
    for (unsigned i = 0; i < actors.size(); ++i) {
        const auto& actor = *actors[i];
        if (!(actor.flags & 0x200) &&
            (actor.room() >> 24 == 255 || !resources.visibility().marked(actor.room() >> 24, 2, 0))) continue;
        auto world = actor.matrix;
        if (const auto* player = dynamic_cast<const playeractor::Player*>(&actor)) {
            world = playerorientation::toMatrix(player->state.quaternion);
            for (unsigned row = 0; row < 3; ++row) {
                for (unsigned column = 0; column < 3; ++column) world[row * 4 + column] *= actor.scale[column];
                world[row * 4 + 3] = actor.position[row];
            }
        }
        if (!(actor.flags & 0x4000))
            actor.model->prepareView(dooranimation::concatenate(gameshadowrenderer::viewMatrix(), world));
        if (!actor.model->viewPrepared() || !(actor.model->color & 255) || (actor.flags & 0x400)) continue;
        auto batches = actorModels[i].shadowBatches;
        for (auto& batch : batches) for (auto& vertex : batch.vertices)
            vertex.position = transform(actor.model->matrix(1, vertex.matrix),
                actor.surface().positions.at(vertex.positionIndex), true);
        gameshadowrenderer::draw(batches, false, true);
    }
    gameshadowrenderer::finish();
    resources.managers().doors().restorePresentation(0);
}

void gameplay::Scene::drawWorld(bool reflection, const s3denvironment::Camera& camera) {
    const unsigned visibilityBuffer = reflection ? 5 : 0;
    drawViewport = reflection ? D3D11_VIEWPORT{320 - float(resources.mirror().width()), 240 - float(resources.mirror().height()),
        float(resources.mirror().width() * 2), float(resources.mirror().height() * 2), 0, 1} : D3D11_VIEWPORT{0, 0, 640, 480, 0, 1};
    gamerenderer::bindSceneTarget(true, &drawViewport);
    if (!reflection && resources.mirror().visible(resources.visibility()))
        mirrorRenderer.draw(gamerenderer::modelContext(), view, projection);
    modelrenderer::initializeModelState(pipeline);
    pipeline.scissor = true;
    const auto drawRooms = [&](unsigned stage) {
        for (const auto& submission : rooms) for (const auto& resident : resources.residentRooms().slots()) {
            if (resident.room != (submission.room & 255)) continue;
            if (!stage && submission.marker != -1) continue;
            for (unsigned pass = stage ? 2u : 0u; pass < (stage ? 3u : 2u); ++pass)
                drawModel(resident.drawing, [&](unsigned node) -> const dooranimation::Matrix& { return resident.drawing.initialMatrices.at(node); },
                    pass, static_cast<std::uint8_t>(submission.marker), submission.room >> 24, submission.bounds);
        }
    };
    const auto submitDoor = [&](const doors::ModelResource& model, const doors::PoseOwner& pose, unsigned modelPass,
        std::uint8_t opacity, bool lowOpacity) {
        drawModel(model, [&](unsigned node) -> const dooranimation::Matrix& { return pose.matrix(node); },
            modelPass, opacity, rooms.front().room >> 24, fullBounds, lowOpacity);
    };
    const auto drawFurniture = [&](unsigned stage) {
        for (const auto& object : resources.furniture().slots()) {
            if ((object.flags & 3) != 3 || !object.model || !resources.visibility().marked(object.room >> 24, visibilityBuffer, 0)) continue;
            const auto marker = resources.visibility().rawMarker(object.room >> 24, visibilityBuffer);
            if (!stage && (marker != -1 || object.opacity != 255)) continue;
            for (unsigned pass = stage ? 2u : 0u; pass < (stage ? 3u : 2u); ++pass)
                drawModel(*object.model, [&](unsigned node) -> const dooranimation::Matrix& { return object.matrices.at(node); },
                    pass, std::min(object.opacity, static_cast<std::uint8_t>(marker)), object.room >> 24, fullBounds);
        }
    };
    prepareAuxiliary();
    drawRooms(0);
    resources.managers().doors().submitOpaque(visibilityBuffer, false, submitDoor);
    drawFurniture(0);
    drawActors(0, camera, visibilityBuffer);
    if (!reflection) drawAuxiliary(2);
    drawSheets(2);
    gamerenderer::bindSceneTarget(false, &drawViewport);
    particleRenderer.draw(gamerenderer::modelContext(), resources.managers().particles().prepareActorGlows(
        resources.managers().actors(), resources.visibility(), view, visibilityBuffer), view, projection);
    drawActors(1, camera, visibilityBuffer);
    drawAuxiliary(reflection ? 1 : 3);
    if (!reflection) drawVacuumEffects();
    drawFurniture(1);
    drawRooms(1);
    resources.managers().doors().submitTranslucent(visibilityBuffer, false, submitDoor);
    if (reflection) return;
    const auto& players = resources.managers().players();
    pointRenderer.draw(gamerenderer::modelContext(), *players.points, players.pointSettings, view, projection, {640, 480});
    drawModelEffects();
    drawMoney();
    particleRenderer.draw(gamerenderer::modelContext(), resources.managers().particles().prepareGlows(
        resources.managers().glows(), view), view, projection);
    drawPlayerLight();
    drawAuxiliary(5);
    particleRenderer.draw(gamerenderer::modelContext(), particleBillboards->prepare(resources.managers().particles().pool(),
        resources.rooms(), resources.visibility(), view), view, projection);
    particleRenderer.draw(gamerenderer::modelContext(), resources.managers().particles().runningDust().prepare(view), view, projection);
    particleRenderer.draw(gamerenderer::modelContext(), resources.managers().particles().elemental().prepareBlow(view), view, projection);
}

void gameplay::Scene::drawSheets(unsigned pass) {
    gamerenderer::bindSceneTarget(false, &drawViewport);
    const auto* player = resources.managers().playerEvents().player(0);
    const auto& owner = *player->components->grids;
    modellighting::State lighting;
    modellighting::initialize(lighting, view, resources.managers().lights().lights(), useRoomLights, ambient, trig);
    const auto& shadow = resources.managers().shadowProjection();
    modellighting::prepareModel(lighting, view, shadow.position, shadow.target, shadow.spotlight, trig);
    gameshadowrenderer::bindModel(view);
    modelrenderer::VertexMatrices matrices{view, view, projection};
    for (unsigned i = 0; i < owner.grids.size(); ++i)
        if (owner.fields16BC[i]) sheetRenderer.draw(gamerenderer::modelContext(), *owner.grids[i], pass, matrices, lighting);
}

void gameplay::Scene::drawModelEffects() {
    auto& managers = resources.managers();
    auto& effects = managers.modelEffects();
    const auto mask = effectLighting->prepare(effectMaterialState, view, managers.lights().lights(), trig);
    const auto instances = effects.prepare(resources.rooms(), resources.visibility(), view, *modelTransforms, mask);
    visibleModelEffects = static_cast<unsigned>(instances.size());
    std::map<const j3dmodel::MaterialPacket*, j3dmodel::ModelInstance*> owners;
    for (auto* instance : instances)
        for (auto& packet : instance->materials) owners.emplace(&packet, instance);
    gamerenderer::bindSceneTarget(false);
    for (unsigned pass = 0; pass < effects.drawBuffers.size(); ++pass) {
        effects.drawBuffers[pass]->visit([&](j3dmodel::MaterialPacket& packet) {
            auto& instance = *owners.at(&packet);
            effectRenderers.at(instance.model.get()).draw(gamerenderer::modelContext(), instance, packet,
                effectMaterialState, projection, instance.field8 & 12, pass + 3);
        });
    }
}

void gameplay::Scene::prepareAuxiliary() {
    auto& managers = resources.managers();
    auto& shared = *managers.players().models;
    for (unsigned i = 2; i < 8; ++i) shared.buffer(i).clear();
    auto& auxiliary = *managers.players().auxiliary;
    if (!auxiliary.field3C) return;
    const auto mask = effectLighting->prepare(equipmentMaterialState, view, managers.lights().lights(), trig);
    const auto& player = *managers.playerEvents().player(0);
    auxiliary.weapon->pipe->lighting(mask, {player.fieldsE9[0], player.fieldsE9[1], player.fieldsE9[2], player.fieldEC});
    auxiliary.prepare(0, view, *modelTransforms, shared);
}

void gameplay::Scene::drawAuxiliary(unsigned stage) {
    auto& managers = resources.managers();
    auto& auxiliary = *managers.players().auxiliary;
    if (!auxiliary.field3C) return;
    auto& shared = *managers.players().models;
    auxiliary.prepare(stage, view, *modelTransforms, shared);
    gamerenderer::bindSceneTarget(false, &drawViewport);
    const auto draw = [&](unsigned index, bool beam) {
        auto& instance = beam ? *auxiliary.vacuum->model->instance : *auxiliary.weapon->pipe->instance;
        auto& state = beam ? lightMaterialState : equipmentMaterialState;
        const auto& buffer = shared.buffer(index);
        for (unsigned i = 0; i < buffer.entries.size(); ++i) {
            const auto bucket = buffer.draw ? buffer.entries.size() - 1 - i : i;
            for (auto* packet = buffer.entries[bucket]; packet; packet = packet->next)
                effectRenderers.at(instance.model.get()).draw(gamerenderer::modelContext(), instance, *packet,
                    state, projection, instance.field8 & 12, (index & 1) ? 4 : 3);
        }
    };
    if (stage == 1) {
        for (unsigned index : {2u, 6u, 3u, 7u}) draw(index, false);
    } else if (stage == 2 || stage == 3) {
        draw(stage, false);
        draw(stage + 2, true);
    } else if (stage == 5) {
        draw(6, false);
        draw(7, false);
    }
}

void gameplay::Scene::drawVacuumEffects() {
    auto& managers = resources.managers();
    auto& shared = *managers.players().models;
    shared.buffer(4).clear();
    shared.buffer(5).clear();
    std::map<j3dmodel::MaterialPacket*, j3dmodel::ModelInstance*> owners;
    const auto prepare = [&](playermodels::VacuumModel& model) {
        model.prepare(3, view, *modelTransforms, shared);
        model.prepare(0, view, *modelTransforms, shared);
        for (auto& packet : model.instance->materials) owners[&packet] = model.instance.get();
    };
    for (const auto* actor : actors) {
        const auto* player = dynamic_cast<const playeractor::Player*>(actor);
        if (!player) continue;
        auto& vacuum = *player->components->vacuum;
        if (vacuum.fields8[0] && !vacuum.field74) prepare(*vacuum.model);
    }
    for (const auto& entry : managers.players().entries->entries)
        if (entry->effect->state) prepare(*entry->effect->model);
    gamerenderer::bindSceneTarget(false, &drawViewport);
    for (unsigned index : {4u, 5u}) {
        const auto& buffer = shared.buffer(index);
        for (unsigned i = 0; i < buffer.entries.size(); ++i) {
            const auto bucket = buffer.draw ? buffer.entries.size() - 1 - i : i;
            for (auto* packet = buffer.entries[bucket]; packet; packet = packet->next) {
                auto& instance = *owners.at(packet);
                effectRenderers.at(instance.model.get()).draw(gamerenderer::modelContext(), instance, *packet,
                    lightMaterialState, projection, instance.field8 & 12, (index & 1) ? 4 : 3);
            }
        }
    }
}

void gameplay::Scene::drawPlayerLight() {
    auto& managers = resources.managers();
    auto* player = managers.playerEvents().player(0);
    auto& light = *player->components->light;
    auto& shared = *managers.players().models;
    shared.buffer(4).clear();
    shared.buffer(5).clear();
    if (!(player->state.flags792 & 4) || player->state.flags1140[2] || !light.state) return;
    light.prepare(view, lightProjectionDepth, *modelTransforms, shared);
    gamerenderer::bindSceneTarget(false);
    for (unsigned index : {4u, 5u}) {
        const auto& buffer = shared.buffer(index);
        for (unsigned i = 0; i < buffer.entries.size(); ++i) {
            const auto bucket = buffer.draw ? buffer.entries.size() - 1 - i : i;
            for (auto* packet = buffer.entries[bucket]; packet; packet = packet->next)
                lightRenderer.draw(gamerenderer::modelContext(), *light.instance, *packet, lightMaterialState,
                    projection, light.instance->field8 & 12, index == 4 ? 3 : 4);
        }
    }
}

void gameplay::Scene::refreshActorModels() {
    std::vector<actors::MoveActor*> current;
    for (auto* actor : resources.managers().actors().entries()) {
        if (actor && actor->model && actor->model->model) current.push_back(actor);
    }
    bool changed = current != actors;
    if (!changed) {
        for (unsigned i = 0; i < current.size(); ++i)
            changed |= current[i]->model->model != actorModelResources[i];
    }
    if (!changed) return;
    std::vector<s3dmodel::Model> updatedModels;
    std::vector<actormodel::ResourceRef> retained;
    for (auto* actor : current) {
        const auto& resource = actor->model->model;
        const auto previous = std::find(actors.begin(), actors.end(), actor);
        const auto index = static_cast<std::size_t>(previous - actors.begin());
        if (previous != actors.end() && actorModelResources[index] == resource)
            updatedModels.push_back(std::move(actorModels[index]));
        else {
            s3dmodel::Model model;
            std::string error;
            if (!model.read(resource->bytes, error, true)) throw std::runtime_error("Actor " + std::to_string(actor->type()) + ": " + error);
            updatedModels.push_back(std::move(model));
        }
        retained.push_back(resource);
    }
    actors = std::move(current);
    actorModels = std::move(updatedModels);
    actorModelResources = std::move(retained);
    textures = gamerenderer::loadPreviews(actorModels, 640, 480);
    gamerenderer::cacheScene("gameplay");
}

void gameplay::Scene::drawActors(unsigned stage, const s3denvironment::Camera& camera, unsigned visibilityBuffer) {
    for (unsigned i = 0; i < actors.size(); ++i) {
        const auto& actor = *actors[i];
        if (actor.room() >> 24 == 255 ||
            !resources.visibility().marked(actor.room() >> 24, visibilityBuffer, 0)) continue;
        const auto* player = dynamic_cast<const playeractor::Player*>(&actor);
        auto world = actor.matrix;
        if (player) {
            world = playerorientation::toMatrix(player->state.quaternion);
            for (unsigned row = 0; row < 3; ++row) {
                for (unsigned column = 0; column < 3; ++column) world[row * 4 + column] *= actor.scale[column];
                world[row * 4 + 3] = actor.position[row];
            }
        }
        if (!stage && !(actor.flags & 0x4000))
            actor.model->prepareView(dooranimation::concatenate(view, world));
        if ((actor.flags & 0x6100) || !(actor.model->color & 255)) continue;
        const auto& surface = actor.surface();
        actorModels[i].materials = surface.materials;
        for (auto& material : actorModels[i].materials) {
            for (unsigned channel = 0; channel < 4; ++channel) {
                if (channel < 3 && !material.stages.empty()) continue;
                const auto source = static_cast<unsigned>(std::round(material.color[channel] * 255));
                const auto actorColor = (actor.model->color >> ((3 - channel) * 8)) & 255;
                material.color[channel] = static_cast<float>((source * actorColor) / 255) / 255;
            }
        }
        auto batches = actorModels[i].batches;
        for (auto& batch : batches) for (auto& vertex : batch.vertices) {
            const auto matrix = dooranimation::concatenate(world, actor.model->matrix(0, vertex.matrix));
            vertex.position = transform(matrix, surface.positions.at(vertex.positionIndex), true);
            dooranimation::Matrix normal;
            if (!modeldraw::normalMatrix(matrix, normal)) {
                const auto& retainedNormal = actor.model->matrix(2, vertex.matrix);
                normal.fill(0);
                for (unsigned row = 0; row < 3; ++row) for (unsigned column = 0; column < 3; ++column)
                    for (unsigned axis = 0; axis < 3; ++axis)
                        normal[row * 4 + column] += view[axis * 4 + row] * retainedNormal[axis * 4 + column];
            }
            vertex.normal = transform(normal, vertex.normal, false);
            vertex.binormal = transform(normal, vertex.binormal, false);
            vertex.tangent = transform(normal, vertex.tangent, false);
        }
        s3denvironment::Lighting lighting;
        const auto lightMode = constant<std::uint16_t>(resources.executable(), 0x8033b598 + actor.type() * 0x1c) & 6;
        if (lightMode == 2) {
            if (useRoomLights) {
                modellighting::State roomLighting;
                const dooranimation::Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
                modellighting::initialize(roomLighting, identity, resources.managers().lights().lights(), true,
                    {actor.fieldsE9[0], actor.fieldsE9[1], actor.fieldsE9[2], 255}, trig);
                lighting = modellighting::actorEnvironment(roomLighting);
            } else {
                for (unsigned channel = 0; channel < 3; ++channel) lighting.ambient[channel] = actor.fieldsE9[channel] / 255.0f;
                lighting.lights.push_back(actorLight);
            }
        } else if (lightMode == 6) {
            for (unsigned channel = 0; channel < 3; ++channel) lighting.ambient[channel] = actor.fieldsE9[channel] / 255.0f;
            auto highlight = actorHighlight;
            for (unsigned axis = 0; axis < 3; ++axis) {
                highlight.position[axis] = highlight.direction[axis] = 0;
                for (unsigned component = 0; component < 3; ++component) {
                    highlight.position[axis] += view[component * 4 + axis] * (actorHighlight.position[component] - view[component * 4 + 3]);
                    highlight.direction[axis] += view[component * 4 + axis] * actorHighlight.direction[component];
                }
            }
            lighting.lights.push_back(highlight);
        } else if (lightMode != 0) throw std::runtime_error("Invalid original actor lighting mode.");
        const auto& roomLights = resources.managers().lights().lights();
        if (!roomLights.empty()) {
            lighting.embossLight = true;
            lighting.embossPosition = roomLights.front().position;
        }
        const auto shadow = resources.managers().shadowProjection();
        if (lightMode == 2 && useRoomLights && shadow.spotlight) {
            modellighting::State shadowLighting;
            const dooranimation::Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
            modellighting::prepareModel(shadowLighting, identity, shadow.position, shadow.target, true, trig);
            const auto& source = shadowLighting.lights[7];
            lighting.shadowSpotlight = true;
            for (unsigned axis = 0; axis < 3; ++axis) {
                lighting.shadowLight.position[axis] = source.position[axis];
                lighting.shadowLight.direction[axis] = source.direction[axis];
                lighting.shadowLight.angular[axis] = source.angular[axis];
                lighting.shadowLight.distance[axis] = source.distance[axis];
                lighting.shadowLight.color[axis] = 1;
            }
        }
        gamerenderer::drawPreview(i, actorModels[i], batches, lighting, &camera, false, 1U << stage, !(actor.flags & 0x800),
            textureTrigonometry, nullptr, &drawViewport,
            (constant<std::uint16_t>(resources.executable(), 0x8033b598 + actor.type() * 0x1c) & 8) != 0);
    }
}

std::string gameplay::Scene::debugInfo() {
    unsigned particleCount = 0;
    const auto& emitters = *resources.managers().particles().pool().destination;
    for (const auto* emitter = emitters.sentinel.next; emitter != &emitters.sentinel; emitter = emitter->next)
        particleCount += emitter->nodes.count + emitter->shortNodes.count;
    unsigned activeEffects = 0;
    for (const auto* pool : resources.managers().modelEffects().registered())
        for (const auto& owner : pool->owners) if (owner->flags & 1) ++activeEffects;
    unsigned ownedActors = 0, furniture = 0, residentRooms = 0, visibleDoors = 0;
    for (const auto* actor : resources.managers().actors().entries()) if (actor) ++ownedActors;
    for (const auto& object : resources.furniture().slots()) if (object.flags & 2) ++furniture;
    for (const auto& room : resources.residentRooms().slots()) if (room.room != 0xffffffff) ++residentRooms;
    auto& doors = resources.managers().doors();
    for (unsigned i = 0; i < doors.size(); ++i) if (doors.leaf(i).flags & 1) ++visibleDoors;
    return "Foyer | Registered actors: " + std::to_string(ownedActors) + " | Scene models: " + std::to_string(actors.size()) +
        " | Furniture: " + std::to_string(furniture) + " | Visible doors: " + std::to_string(visibleDoors) +
        " | Resident rooms: " + std::to_string(residentRooms) + "\nModel effects: " + std::to_string(activeEffects) +
        " | Drawing: " + std::to_string(visibleModelEffects) + " | Candles: " + std::to_string(resources.managers().candleCount()) +
        " | Particles: " + std::to_string(particleCount) + " | Dust: " +
        std::to_string(resources.managers().particles().runningDust().particleCount()) + "\nSimulation: 30 Hz | Tick: " + std::to_string(ticks);
}

void gameplay::Scene::drawMoney() {
    auto& managers = resources.managers();
    effectLighting->prepare(effectMaterialState, view, managers.lights().lights(), trig);
    const auto instances = managers.moneyModels();
    gamerenderer::bindSceneTarget(false);
    for (auto* instance : instances) {
        j3dmodel::calculateBasic(*instance, *modelTransforms);
        for (auto& material : instance->model->materials.entries)
            j3dmodel::calculateMaterial(*material, instance->jointMatrices[0], view, instance->field8 & 12, *modelTransforms);
        for (auto& packet : instance->materials) j3dmodel::updateMaterialPacket(packet);
        j3dmodel::calculateViewMatrices(*instance, view);
    }
    for (unsigned pass = 0; pass < 2; ++pass)
        for (auto* instance : instances)
            for (auto& packet : instance->materials) {
                if (((packet.material->mode & 3) ? 0u : 1u) != pass) continue;
                effectRenderers.at(instance->model.get()).draw(gamerenderer::modelContext(), *instance, packet,
                    effectMaterialState, projection, instance->field8 & 12, pass + 3);
            }
}


const scenetransition::Request& gameplay::Scene::transition() const { return resources.transition(); }
