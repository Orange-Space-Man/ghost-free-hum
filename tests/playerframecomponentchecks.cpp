#include "game/scene/gameplayresourceowner.h"
#include "game/player/playerheadtracking.h"
#include "game/player/playerreactioneffects.h"
#include "game/player/playerbreathframe.h"
#include "game/player/playercontactreactions.h"
#include "game/world/dooranimationpose.h"
#include "../recovered/playerfurnitureframe.h"
#include "../recovered/roomfurnitureresponse.h"

#include <cmath>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }
}

void checkPlayerFrameComponents(const std::filesystem::path& package, const gamesave::State& saves) {
    gameentry::State entry;
    std::string error;
    if (!gameentry::selectNewFile(saves, 0, entry, error)) throw std::runtime_error(error);
    gameplay::ResourceOwner resources(package, entry);
    auto& managers = resources.managers();
    auto& player = *managers.playerEvents().player(0);
    auto& state = player.state;
    auto& actions = *player.components->actions;
    const auto services = managers.preparePlayerDoor(0);
    const actormodel::InitialPose pose(resources.executable());
    require(state.camera && state.camera->position == resources.camera().position,
        "The registered player must retain the actual scene camera owner.");
    auto& part = *player.components->weapon->first;
    const auto checkSelection = [&](bool joints) {
        auto& primary = joints ? part.second : part.first;
        auto& upper = joints ? part.field598 : part.field574;
        auto& mode = joints ? part.field5E0 : part.field5BC;
        const auto fallback = joints ? part.parameters[7] : part.parameters[6];
        const auto select = [&](unsigned upperMode, bool alternate) {
            return joints ? part.joint(4, 2, upperMode, alternate) : part.animation(4, 2, upperMode, alternate);
        };
        primary[4] = 81;
        mode[3] = 82;
        upper[2] = 83;
        require(select(3, true) == 81, "Action-specific equipment overrides must take precedence.");
        primary[4] = fallback;
        require(select(3, true) == 82, "Attached-state equipment overrides must precede alternate selection.");
        mode[3] = fallback;
        require(select(3, false) == fallback, "Nonzero attachment mode must not use the upper-animation override.");
        mode[0] = fallback;
        require(select(0, true) == 83, "Idle attachment mode must use its upper-animation override.");
        upper[2] = fallback;
        require(select(0, true) == part.parameters[joints ? 9 : 8], "Alternate equipment fallback was lost.");
        require(select(0, false) == fallback, "Ordinary equipment fallback was lost.");
    };
    checkSelection(false);
    checkSelection(true);
    auto& head = *player.components->single;
    auto target = player.position;
    target[0] += 400;
    target[1] += 180;
    target[2] += 400;
    head.update(3, target, 0.25, 1, 1, pose);
    require(head.field28 && head.field3C && (head.effect.flags & 8), "Head tracking must enable and initialize its retained rotation.");
    const std::array<float, 4> initial{head.field2C, head.field30, head.field34, head.field38};
    target[0] -= 800;
    head.update(4, target, 0, 1, 1, pose);
    require(initial == std::array<float, 4>{head.field2C, head.field30, head.field34, head.field38},
        "Zero blend must retain an initialized head rotation.");
    head.update(0, target, 1, 1, 1, pose);
    require(!head.field28 && !(head.effect.flags & 8) && head.field3C,
        "Disabling head tracking must preserve its interpolation history.");
    head.update(4, target, 1, 1, 1, pose);
    require(initial != std::array<float, 4>{head.field2C, head.field30, head.field34, head.field38},
        "Reenabled head tracking must accept its new target.");
    for (float value : {head.effect.field34, head.effect.field38, head.effect.field3C, head.effect.field40})
        require(std::isfinite(value), "Head rotation must remain finite with the imported Luigi joint basis.");
    const playercomponents::HeadFrame frame(resources.executable());
    state.field10D1 = 1;
    state.field10D4 = target;
    state.field1324 |= 4;
    state.field10D0 = 0;
    player.flags |= 0x1000;
    frame.run(player, managers.actors(), managers.enemies(), services.catalog, pose);
    require(head.field28 != 0, "The player head stage must select its retained explicit target.");
    state.flags1140[2] = 1;
    frame.run(player, managers.actors(), managers.enemies(), services.catalog, pose);
    require(head.field28 == 0, "The original scanner guard must suppress the head override.");
    state.flags1140[2] = 0;
    unsigned releasedIce = 0;
    const auto effects = [&] {
        playeractor::updateReactionEffects(player, services.catalog, services.models, managers.particles().effects(),
            managers.modelEffects().factory, [&](unsigned id, const auto& position) {
                require(id == 0x813 && position == player.position, "Ice release must submit its original positional sound.");
                ++releasedIce;
            });
    };
    actions.fields31C[4] = 10;
    effects();
    const auto burning = state.burningEffects;
    for (auto* emitter : burning) require(emitter != nullptr, "Burning must acquire all four original effects.");
    effects();
    require(state.burningEffects == burning, "An existing burn must retain its emitter ownership.");
    actions.fields31C[4] = 0;
    actions.field310 = 2;
    actions.field314 = 0x16;
    player.animationFrame = 24;
    effects();
    require(!state.burningEffects[0] && !state.burningEffects[1] && state.burningEffects[2] && state.burningEffects[3],
        "The recovery action must retain its second burn pair before frame 25.");
    player.animationFrame = 25;
    effects();
    require(!state.burningEffects[2] && !state.burningEffects[3], "Burn recovery must release the second pair at frame 25.");
    for (auto* emitter : burning) require((emitter->flags & 0x24) == 0x24, "Burn release must use immediate retirement.");
    actions.fields31C[4] = 13;
    effects();
    require(state.iceEffect != nullptr, "Freeze must acquire the original model effect.");
    actions.fields31C[4] = 0;
    actions.field314 = 0x1c;
    player.animationFrame = 2;
    effects();
    require(state.iceEffect && !releasedIce, "Ice must survive the first two recovery frames.");
    player.animationFrame = 3;
    effects();
    require(!state.iceEffect && releasedIce == 1, "Ice must release once at recovery frame three.");
    effects();
    require(releasedIce == 1, "Ice release sound must not repeat on later frames.");
    player.components->reactions->state = 4;
    effects();
    require(state.poisonEffect != nullptr, "Poison reaction must acquire its original effect.");
    player.components->reactions->state = 1;
    effects();
    require(!state.poisonEffect, "Leaving poison must release the retained effect.");
    const playeractor::BreathFrame breath(resources.executable());
    breath.run(player, services.catalog, services.models, false);
    const auto mouth = dooranimation::concatenate(player.matrix, player.model->matrix(0, 0x33));
    float directionLength = 0;
    for (unsigned i = 0; i < 3; ++i) {
        require(std::isfinite(state.fieldFC4[i]) && std::isfinite(state.fieldFD0[i]),
            "Breathing must use a finite world-space mouth transform.");
        directionLength += state.fieldFD0[i] * state.fieldFD0[i];
        require(std::abs(state.fieldFC4[i] - (mouth[i * 4 + 3] + mouth[i * 4] * 7.156f + mouth[i * 4 + 2] * 22.367f)) < 0.01f,
            "Mouth offset must preserve the original local X and Z axes.");
    }
    require(std::abs(directionLength - 1) < 0.001f, "Breath direction must be normalized after the joint transform.");
    breath.run(player, services.catalog, services.models, true);
    require(state.fieldFE0 == 0, "Cleared rooms must suppress visible breath intensity.");
    const playeractions::ContactFrame contact(resources.executable());
    std::int32_t audioLife = 100;
    playerevents::LifeEnvironment life{audioLife, managers.particles().effects()};
    actions.field310 = 2;
    actions.field314 = 0x19;
    contact.run(player, managers.collision(), *state.camera, life, false, {});
    require(state.field1070 == 5 && state.field1074[0] == state.camera->position[0] &&
        state.field1074[1] == player.position[1] && state.field1074[2] == state.camera->position[2],
        "Item presentation contact reaction must turn toward the bound camera at the player's height.");
    playerscanner::FurnitureCache cache(resources.executable(), resources.furniture(), managers.flags());
    const actorcollision::Catalog collisionCatalog(resources.executable(), services.catalog);
    const actorcollision::Manager collision(collisionCatalog, services.catalog);
    const playerinteraction::FurnitureFrame furnitureFrame(resources.executable());
    unsigned initializedSheets = 0;
    playerinteraction::FurnitureServices furniture{cache, resources.furniture(), resources.rooms(), managers.enemies(),
        services.catalog, collision, managers.collision(), 2,
        [&](const playerscanner::SheetInput&) { ++initializedSheets; }, [] { return 0u; }};
    auto probe = player.position;
    probe[1] += 50;
    mapcollision::SphereContact wallHit;
    for (unsigned step = 0; step < 100 && wallHit.wall < 0; ++step) {
        probe[0] += 20;
        wallHit = managers.collision().sphereContact(probe, 40);
    }
    require(wallHit.wall >= 0, "The contact check must locate a real foyer wall.");
    player.fieldB0 = static_cast<std::int16_t>(wallHit.wall);
    std::array<float, 3> wallNormal;
    require(managers.collision().polygonNormal(player.fieldB0, wallNormal), "The wall must retain its collision normal.");
    for (unsigned i = 0; i < 3; ++i) state.direction[i] = -wallNormal[i];
    require(furnitureFrame.wall(player, managers.collision()) && player.sound.field64 == 0xff && state.field1070 == 7,
        "A facing wall within Luigi's reach must select the original knock action and sound context.");
    state.direction = wallNormal;
    require(!furnitureFrame.wall(player, managers.collision()), "A wall behind Luigi must not receive the interaction.");
    actions.field310 = 2;
    actions.field314 = 4;
    actions.fields31C[6] = 6;
    unsigned impactEffects = 0;
    contact.run(player, managers.collision(), *state.camera, life, false,
        [&](unsigned id, const auto& position, const auto& normal) {
            require(id == 0x3b && normal == wallNormal && position[1] > player.position[1],
                "The original wall recoil uses its raised impact point and actual contact normal.");
            ++impactEffects;
        });
    require(impactEffects == 1, "A qualifying wall reaction must emit its original impact once.");
    player.position = {100000, 100000, 100000};
    player.fieldB0 = -1;
    actions.field310 = 1;
    state.field11A0 = nullptr;
    state.field1130 = 0;
    state.field1170 = 0;
    state.field1338 = 1;
    state.flags1140[2] = 0;
    auto& controls = *player.components->controls;
    auto& attached = *player.components->attached;
    controls.flagsC[4] = 0;
    attached.field2C0 = attached.field2D0 = 0;
    furnitureFrame.run(player, furniture);
    require(initializedSheets == 1 && !attached.field2C0 && !attached.field2D0,
        "The idle interaction stage initializes the foyer sheet cache without inventing an input action.");
    controls.flagsC[4] = 1;
    furnitureFrame.run(player, furniture);
    require(initializedSheets == 1 && attached.field2C0 == 7 && attached.field2D0 == 7,
        "An unhandled first-area button press must select both original upper-body transitions.");
    attached.field2C0 = attached.field2D0 = 0;
    state.flags1140[2] = 1;
    furnitureFrame.run(player, furniture);
    require(!attached.field2C0 && !attached.field2D0, "Scanner ownership suppresses the ordinary interaction button.");
    state.flags1140[2] = 0;
    state.field1170 = 5;
    furnitureFrame.run(player, furniture);
    require(attached.field2C0 == 7 && attached.field2D0 == 7,
        "The original power-state guard retains the unhandled-button animation even while contact is disabled.");
    state.field1170 = 0;
    bool struck = false;
    unsigned struckRow = 0;
    for (const auto& bounds : cache.contacts()) {
        player.position = bounds.center;
        player.position[1] = bounds.minimumY;
        player.position[2] -= 10;
        state.direction = {0, 0, 1};
        actions.field310 = 1;
        furnitureFrame.run(player, furniture);
        for (unsigned i = 0; i < cache.targets().size(); ++i) {
            const auto& row = resources.furniture().rowState(cache.row(i));
            if (!(row.flags & 1)) continue;
            struck = true;
            struckRow = cache.row(i);
            require(state.field1070 == 7 && player.velocity == std::array<float, 3>{} &&
                player.sound.field64 == cache.contacts()[i].sound,
                "Furniture interaction must publish facing, stopped velocity and original animation-sound context.");
        }
        if (struck) break;
    }
    require(struck, "The player button stage must reach a real foyer furniture row.");
    enemytypes::Catalog types;
    require(types.read(resources.executable(), error), "Furniture response requires original item types.");
    const characteritems::Admission admission(resources.executable(), types);
    const roomfurniture::Response response(resources.executable(), admission);
    roomfurniture::ResponseServices responseServices;
    unsigned collections = 0, dust = 0;
    responseServices.roomCleared = [&](unsigned room) { return managers.roomCleared(room); };
    responseServices.selectItem = [&](unsigned) { ++collections; return jmp::ToolDataRef{}; };
    responseServices.effect = [&](unsigned effect, const auto&) {
        require(effect == 0x61, "Empty furniture response must emit the original dust effect.");
        ++dust;
    };
    responseServices.sound = [](unsigned, const auto&, const float*) {};
    resources.furniture().initializeObjects();
    const auto* ownedObject = resources.furniture().object(struckRow);
    require(ownedObject && (ownedObject->behavior == 0 || ownedObject->behavior == 22),
        "Player interaction fixture must reach the original basic furniture owner.");
    auto& visibility = resources.visibility();
    std::uint16_t savedFlags = 0;
    auto& roomFlags = resources.roomFlags().at(ownedObject->room >> 24);
    visibility.mark(0, 2, ownedObject->room, roomFlags, savedFlags, 0, -1, 0, {0, 0, 640, 480}, {0, 0, 640, 480});
    const auto startingUpdates = ownedObject->updates;
    for (unsigned frame = 0; frame < 20; ++frame) {
        resources.furniture().update(visibility, 0, [&](auto& object, auto& row, bool visible) {
            if (object.row != struckRow) return false;
            response.basic(object, row, visible, false, 0, frame, responseServices);
            return true;
        });
        require(bool(resources.furniture().rowState(struckRow).flags & 1) == (frame < 19),
            "The owned furniture row must retain the player's strike until its twentieth update.");
    }
    require(ownedObject == resources.furniture().object(struckRow) && ownedObject->updates == startingUpdates + 20 &&
        collections == 1 && dust == 1 && (ownedObject->collectionFlags & 1),
        "The manager-owned furniture object must consume the player interaction and retain completion state.");
    for (const auto& matrix : ownedObject->matrices) for (float value : matrix)
        require(std::isfinite(value), "Furniture owner must refresh finite render matrices after its strike response.");
}
