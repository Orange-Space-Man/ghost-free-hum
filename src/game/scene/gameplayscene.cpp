#include "resources/dolresources.h"
#include "game/scene/gameplayscene.h"
#include "platform/d3d11/gamerenderer.h"
#include "game/models/modeldrawmatrices.h"
#include "game/startup/startupresources.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
    const roomtraversal::Bounds fullBounds{0, 0, 640, 480};

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

gameplay::Scene::Scene(const std::filesystem::path& package, gameentry::State& entry) : resources(package, entry),
    keyPresentation(package, resources.executable(), sceneControl, resources.managers().flags(), resources.managers().music()) {
    std::string error;
    if (!trig.read(resources.executable(), error) || !poseTables.read(resources.executable(), error)) throw std::runtime_error(error);
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
    const auto& camera = resources.camera();
    const auto vector = [](const std::array<float, 3>& value) { return DirectX::XMVectorSet(value[0], value[1], value[2], 0); };
    DirectX::XMFLOAT4X4 viewMatrix, projectionMatrix;
    DirectX::XMStoreFloat4x4(&viewMatrix, DirectX::XMMatrixLookAtRH(vector(camera.position), vector(camera.target), vector(camera.up)));
    DirectX::XMStoreFloat4x4(&projectionMatrix, DirectX::XMMatrixPerspectiveFovRH(camera.fieldOfView * DirectX::XM_PI / 180,
        camera.aspect, camera.nearPlane, camera.farPlane));
    for (unsigned row = 0; row < 4; ++row) for (unsigned column = 0; column < 4; ++column) {
        projection[row * 4 + column] = projectionMatrix.m[column][row];
        if (row < 3) view[row * 4 + column] = viewMatrix.m[column][row];
    }
    auto gx = projection;
    for (unsigned column = 0; column < 4; ++column) gx[8 + column] -= gx[12 + column];
    for (unsigned row = 0; row < 4; ++row) for (unsigned column = 0; column < 4; ++column) {
        for (unsigned k = 0; k < 4; ++k) visibilityProjection[row * 4 + column] += gx[row * 4 + k] * viewMatrix.m[column][k];
    }
    auto& managers = resources.managers();
    managers.updatePlacement(0);
    resources.furniture().initializeObjects();
    rooms = resources.prepareVisibility(visibilityProjection, fullBounds);
    updateOpacity();
    for (auto* actor : managers.actors().entries()) {
        if (!actor || !actor->model || !actor->model->model || actor->room() >> 24 == 255 ||
            !resources.visibility().marked(actor->room() >> 24, 0, 0)) continue;
        s3dmodel::Model model;
        if (!model.read(actor->model->model->bytes, error)) throw std::runtime_error(error);
        actors.push_back(actor);
        actorModels.push_back(std::move(model));
    }
    if (actors.empty()) throw std::runtime_error("First-area scene has no manager-owned visible actors.");
}

void gameplay::Scene::prepareGraphics() {
    textures = gamerenderer::loadPreviews(actorModels, 640, 480);
    gamerenderer::cacheScene("gameplay");
    const auto load = [&](const doors::ModelResource& model) {
        if (!models.count(&model)) models[&model].load(gamerenderer::modelDevice(), model);
    };
    for (const auto& resident : resources.residentRooms().slots()) {
        if (resident.room == 0xffffffff) continue;
        load(resident.drawing);
        for (const auto& model : resident.furniture.models()) load(model);
    }
    for (unsigned i = 0; i < resources.managers().doors().size(); ++i) {
        const auto& pose = resources.managers().doors().leaf(i).pose;
        load(pose.model(pose.selectedModel()));
        load(pose.model(0));
    }
}

void gameplay::Scene::update(const playerinput::PadStatus& pad, std::int16_t port) {
    resources.managers().sampleInput(pad, port);
    auto& managers = resources.managers();
    presentationButtons = managers.controller().pressed;
    presentationTick = true;
    if (!sceneControl.gameplayEnabled) {
        managers.updateAudio(view);
        return;
    }
    managers.updatePlacement(ticks / 30);
    managers.updateGenerators();
    for (unsigned i = 0; i < managers.enemies().slots().size(); ++i) managers.updateOwner(i);
    resources.furniture().updateIdle(resources.visibility(), 0);
    rooms = resources.prepareVisibility(visibilityProjection, fullBounds);
    updateOpacity();
    managers.updateAudio(view);
    ++ticks;
}

unsigned gameplay::Scene::updates() const { return ticks; }

bool gameplay::Scene::presentKey(bool missingKey, bool front, unsigned doorType) {
    return keyPresentation.request(missingKey, front, doorType);
}

bool gameplay::Scene::keyPresentationActive() const { return keyPresentation.state().owned; }
bool gameplay::Scene::gameplaySuspended() const { return !sceneControl.gameplayEnabled; }


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
    unsigned room, const roomtraversal::Bounds& bounds) {
    auto* context = gamerenderer::modelContext();
    gamerenderer::bindSceneTarget(false);
    const D3D11_RECT rectangle{bounds[0], bounds[1], bounds[2], bounds[3]};
    context->RSSetScissorRects(1, &rectangle);
    modellighting::State lighting;
    modellighting::initialize(lighting, view, {}, false, resources.rooms().entries().at(room).ambient, trig);
    modellighting::prepareModel(lighting, view, lightOrigin, lightTarget, false, trig);
    modeldraw::TraversalState state;
    state.flaggedOpacity = flaggedOpacity;
    state.limitedOpacity = limitedOpacity;
    modeldraw::traverse(model.drawing, pass, opacity, state, [&](const modeldraw::DrawCommand& command) {
        modelrenderer::VertexMatrices matrices;
        matrices.modelView = dooranimation::concatenate(view, matrix(command.node));
        if (!modeldraw::normalMatrix(matrices.modelView, matrices.normal)) throw std::runtime_error("Singular first-area model transform.");
        matrices.projection = projection;
        models.at(&model).draw(context, command, pass, 0, matrices, trig, lighting, pipeline, {}, retained, false);
    });
}

void gameplay::Scene::draw(bool present) {
    const bool gameplayDrawn = sceneControl.gameplayEnabled;
    if (gameplayDrawn) drawGameplay(false);
    if (presentationTick) {
        keyPresentation.advanceDraw(presentationButtons);
        presentationTick = false;
        presentationButtons = 0;
    }
    if (keyPresentation.state().owned && keyPresentation.state().updated) keyPresentation.draw(present);
    else {
        startupscene::Picture picture;
        picture.texture = gameplayDrawn ? textures[0] : 0;
        picture.width = 640;
        picture.height = 480;
        gamerenderer::drawInterface({picture}, present);
    }
}

void gameplay::Scene::drawGameplay(bool present) {
    if (textures.empty()) throw std::runtime_error("Gameplay graphics are not prepared.");
    if (!gamerenderer::useScene("gameplay")) throw std::runtime_error("Gameplay graphics ownership is unavailable.");
    gamerenderer::bindSceneTarget(true);
    modelrenderer::initializeModelState(pipeline);
    pipeline.scissor = true;
    const auto drawRooms = [&](unsigned stage) {
        for (const auto& submission : rooms) for (const auto& resident : resources.residentRooms().slots()) {
            if (resident.room != (submission.room & 255)) continue;
            for (unsigned pass = stage ? 2u : 0u; pass < (stage ? 3u : 2u); ++pass)
                drawModel(resident.drawing, [&](unsigned node) -> const dooranimation::Matrix& { return resident.drawing.initialMatrices.at(node); },
                    pass, static_cast<std::uint8_t>(submission.marker), submission.room >> 24, submission.bounds);
        }
    };
    const auto submitDoor = [&](const doors::ModelResource& model, const doors::PoseOwner& pose, unsigned modelPass,
        std::uint8_t opacity, bool shadow) {
        if (shadow) throw std::runtime_error("Door shadow submission is not available.");
        drawModel(model, [&](unsigned node) -> const dooranimation::Matrix& { return pose.matrix(node); },
            modelPass, opacity, rooms.front().room >> 24, fullBounds);
    };
    const auto drawFurniture = [&](unsigned stage) {
        for (const auto& object : resources.furniture().slots()) {
            if ((object.flags & 3) != 3 || !object.model || !resources.visibility().marked(object.room >> 24, 0, 0)) continue;
            const auto marker = resources.visibility().rawMarker(object.room >> 24, 0);
            if (!stage && (marker != -1 || object.opacity != 255)) continue;
            for (unsigned pass = stage ? 2u : 0u; pass < (stage ? 3u : 2u); ++pass)
                drawModel(*object.model, [&](unsigned node) -> const dooranimation::Matrix& { return object.matrices.at(node); },
                    pass, std::min(object.opacity, static_cast<std::uint8_t>(marker)), object.room >> 24, fullBounds);
        }
    };
    drawRooms(0);
    resources.managers().doors().submitOpaque(0, false, submitDoor);
    drawFurniture(0);
    drawActors(0);
    drawActors(1);
    drawFurniture(1);
    drawRooms(1);
    resources.managers().doors().submitTranslucent(0, false, submitDoor);
    startupscene::Picture picture;
    picture.texture = textures[0];
    picture.width = 640;
    picture.height = 480;
    gamerenderer::drawInterface({picture}, present);
}

void gameplay::Scene::drawActors(unsigned stage) {
    for (unsigned i = 0; i < actors.size(); ++i) {
        const auto& actor = *actors[i];
        if ((actor.flags & 0x6100) || !resources.visibility().marked(actor.room() >> 24, 0, 0)) continue;
        const auto* player = dynamic_cast<const playeractor::Player*>(&actor);
        auto world = player ? playerorientation::toMatrix(player->state.quaternion) :
            poseTables.world({}, {actor.rotation[0], actor.rotation[1], actor.rotation[2]}, {1, 1, 1});
        for (unsigned row = 0; row < 3; ++row) {
            for (unsigned column = 0; column < 3; ++column) world[row * 4 + column] *= actor.scale[column];
            world[row * 4 + 3] = actor.position[row];
        }
        const auto& surface = actor.surface();
        actorModels[i].materials = surface.materials;
        auto batches = actorModels[i].batches;
        for (auto& batch : batches) for (auto& vertex : batch.vertices) {
            const auto matrix = dooranimation::concatenate(world, actor.model->matrix(0, vertex.matrix));
            vertex.position = transform(matrix, surface.positions.at(vertex.positionIndex), true);
            dooranimation::Matrix normal;
            if (!modeldraw::normalMatrix(matrix, normal)) throw std::runtime_error("Singular first-area actor transform.");
            vertex.normal = transform(normal, vertex.normal, false);
        }
        s3denvironment::Lighting lighting;
        const auto lightMode = constant<std::uint16_t>(resources.executable(), 0x8033b598 + actor.type() * 0x1c) & 6;
        if (lightMode == 2) {
            for (unsigned channel = 0; channel < 3; ++channel) lighting.ambient[channel] = actor.fieldsE9[channel] / 255.0f;
            lighting.lights.push_back(actorLight);
        } else if (lightMode != 0) throw std::runtime_error("Actor requires its original secondary lighting path.");
        gamerenderer::drawPreview(i, actorModels[i], batches, lighting, &resources.camera(), false, 1U << stage);
    }
}

std::string gameplay::Scene::debugInfo() {
    unsigned ownedActors = 0, furniture = 0, residentRooms = 0, visibleDoors = 0;
    for (const auto* actor : resources.managers().actors().entries()) if (actor) ++ownedActors;
    for (const auto& object : resources.furniture().slots()) if (object.flags & 2) ++furniture;
    for (const auto& room : resources.residentRooms().slots()) if (room.room != 0xffffffff) ++residentRooms;
    auto& doors = resources.managers().doors();
    for (unsigned i = 0; i < doors.size(); ++i) if (doors.leaf(i).flags & 1) ++visibleDoors;
    return "Foyer | Registered actors: " + std::to_string(ownedActors) + " | Scene models: " + std::to_string(actors.size()) +
        " | Furniture: " + std::to_string(furniture) + " | Visible doors: " + std::to_string(visibleDoors) +
        " | Resident rooms: " + std::to_string(residentRooms) + "\nSimulation: 30 Hz | Tick: " + std::to_string(ticks);
}
