#include "game/scene/gameplayresourceowner.h"
#include "game/player/playerheadtracking.h"
#include "game/player/playerreactioneffects.h"
#include "game/player/playerbreathframe.h"
#include "game/player/playercontactreactions.h"
#include "game/world/dooranimationpose.h"
#include "../recovered/playerfurnitureframe.h"
#include "../recovered/roomfurnitureresponse.h"
#include "../recovered/playerenergyframe.h"
#include "game/player/playerlightframe.h"
#include "game/player/playerlightcontacts.h"
#include "../recovered/playerlatestate.h"

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
    {
        const playeractor::LateState late(resources.executable());
        const auto retained = state;
        const auto retainedCamera = *state.camera;
        const auto playerFlags = player.flags, firstFlags = state.field7A4->flags, secondFlags = state.field7A8->flags;
        const auto materialFlags = player.surface().materials.at(1).flags;
        unsigned cancelled = 0;
        playeractor::LateStateServices lateServices{false,
            [&](auto& owner, bool enabled) {
                require(&owner == &player && !enabled, "Late scanner cancellation must address its player owner.");
                owner.state.flags1140[2] = 0;
                ++cancelled;
            }};
        state.flags792 = 4;
        state.flags1330[2] = 0;
        state.flags1140[2] = 0;
        state.flags134C[0] = 0;
        player.flags &= ~0xc000u;
        state.camera->position = player.position;
        state.camera->target = player.position;
        state.camera->target[2] += 1;
        late.begin(player, lateServices);
        require(state.fieldF80 == std::array<float, 3>{0, 0, 1} && state.fieldF8C == 0 && (player.surface().materials.at(1).flags & 2) &&
            !(state.field7A4->flags & 0xc000) && (state.field7A8->flags & 0xc000) == 0xc000,
            "Late camera state and first-visit visibility must retain the held light and conceal the vacuum.");
        lateServices.hidePlayers = true;
        state.flags792 = 8;
        late.begin(player, lateServices);
        require((player.flags & 0x102) == 0x102 && (state.field7A4->flags & 0x102) == 0x102 && !(player.surface().materials.at(1).flags & 2) &&
            !(state.field7A8->flags & 0xc000), "Global player concealment must retain independent equipment ownership flags.");
        player.flags |= 0xc000;
        late.begin(player, lateServices);
        require((state.field7A4->flags & 0xc000) == 0xc000 && (state.field7A8->flags & 0xc000) == 0xc000,
            "Inactive player flags must conceal every held actor even when equipment is owned.");
        state.flags1140[2] = 1;
        state.field117C = 0;
        state.field1058 = 0;
        state.camera->flags = 9;
        late.begin(player, lateServices);
        require(cancelled == 0, "Camera flag eight must override scanner cancellation from flag one.");
        state.camera->flags = 1;
        late.begin(player, lateServices);
        require(cancelled == 1 && !state.flags1140[2], "Late camera restrictions must cancel the active scanner exactly once.");
        auto& scanner = *player.components->scanner;
        const auto retainedScannerState = scanner.state;
        const auto retainedFrame = scanner.field4, retainedDelay = scanner.field10;
        const auto retainedScannerMatrix = scanner.scanner->matrix, retainedWarpMatrix = scanner.warp->matrix;
        const auto retainedScannerScale = scanner.scanner->scale, retainedWarpScale = scanner.warp->scale;
        scanner.state = {1, 0, 1};
        scanner.field4 = 123;
        scanner.field10 = 4;
        late.scanner(scanner, *managers.players().models);
        require(managers.players().models->animation(4)->frame == managers.players().models->animation(4)->frameCount - 4 &&
            managers.players().models->animation(5)->frame == 123 && managers.players().models->animation(8)->frame == 123,
            "Scanner reverse texture timing and warp transform/color timing must retain their separate clocks.");
        require(scanner.scanner->matrix != retainedScannerMatrix && scanner.warp->matrix != retainedWarpMatrix,
            "Active scanner and warp models must update from their retained camera owner.");
        const auto stoppedMatrix = scanner.scanner->matrix;
        scanner.state = {};
        state.camera->position[0] += 500;
        late.scanner(scanner, *managers.players().models);
        require(scanner.scanner->matrix == stoppedMatrix, "Inactive scanner models must retain their last transforms.");
        scanner.state = retainedScannerState;
        scanner.field4 = retainedFrame;
        scanner.field10 = retainedDelay;
        scanner.scanner->matrix = retainedScannerMatrix;
        scanner.warp->matrix = retainedWarpMatrix;
        scanner.scanner->scale = retainedScannerScale;
        scanner.warp->scale = retainedWarpScale;
        state = retained;
        *state.camera = retainedCamera;
        player.flags = playerFlags;
        state.field7A4->flags = firstFlags;
        state.field7A8->flags = secondFlags;
        player.setMaterialFlags(1, 255, false);
        player.setMaterialFlags(1, materialFlags, true);
    }
    {
        const playeractor::LightFrame frame(resources.executable());
        const actormodel::InitialPose pose(resources.executable());
        auto& light = *player.components->light;
        auto& pulse = *managers.players().state;
        roomlighting::Pool lights;
        std::vector<unsigned> sounds;
        unsigned illuminated = 0;
        const playeractor::LightServices lightServices{lights, *managers.players().models, pulse,
            [&](unsigned sound, const auto& position) {
                require(position == player.position, "Flashlight switching sound must use the player position.");
                sounds.push_back(sound);
            },
            [&](auto& source, const auto& beam) {
                require(&source == &player && beam.owner == &player && beam.state == 1,
                    "Illumination must retain the player owner before dispatching contacts.");
                ++illuminated;
            }};
        gameplaycamera::CameraOwner camera;
        camera.position = {10, 20, 30};
        camera.target = {10, 20, 40};
        playeractor::LightInput input{true, false, true, false, {4, 5, 6}, {0, 0, 1},
            70, 1000, 1, 0x12345678, 123, camera};
        frame.run(player, input, lightServices, pose);
        require(sounds == std::vector<unsigned>{0x837} && illuminated == 1 && light.field50 == 1000000 &&
            light.field18 == input.position && light.field30 == input.direction && lights.count() == 1,
            "Enabling the flashlight must initialize its cone, room light, illumination and switch sound.");
        require(lights.entry(0).priority == 8 && lights.entry(0).light.color == std::array<std::uint8_t, 4>{0x12, 0x34, 0x56, 0x78} &&
            lights.entry(0).light.cutoff == 35 && lights.entry(0).light.referenceDistance == 1000,
            "Flashlight room lighting must retain original priority, packed color and half-angle.");
        const auto& animation = *managers.players().models->animation(2);
        require(animation.frame == float(123 % static_cast<std::uint16_t>(animation.frameCount)),
            "Cone texture animation must wrap on the supplied original frame clock.");
        for (unsigned row = 0; row < 3; ++row)
            require(light.instance->matrix[row * 4 + 3] == input.position[row],
                "Cone model translation must follow the actual flashlight endpoint.");
        const playeractor::LightContacts contacts(resources.executable(), services.catalog);
        require(contacts.contains(light, {4, 5, 506}, 0, nullptr) &&
            !contacts.contains(light, {4, 5, 1006}, 0, nullptr) &&
            !contacts.contains(light, {4, 5, -494}, 0, nullptr),
            "Flashlight contacts must reject the exact range boundary and points behind the beam.");
        require(!contacts.contains(light, {404, 5, 506}, 0, nullptr) &&
            contacts.contains(light, {404, 5, 506}, 100, nullptr) &&
            contacts.contains(light, {4, 5, -44}, 100, nullptr),
            "Collision radius must widen the beam and retain the original close-sphere exception.");
        camera.matrix = {0.001f, 0, 0, 0, 0, 0.001f, 0, 0, 0, 0, -0.001f, 0, 0, 0, 0, 1};
        require(contacts.contains(light, {4, 5, 506}, 0, &camera),
            "Original camera clip projection must admit a front-facing light contact.");
        camera.matrix[15] = 0;
        require(!contacts.contains(light, {4, 5, 506}, 0, &camera),
            "Zero homogeneous camera coordinate must reject a light contact.");
        lights.beginFrame();
        input.direction = {0, -0.6f, 0.8f};
        frame.run(player, input, lightServices, pose);
        require(light.field18[1] > input.position[1] && light.field30[1] < 0 && sounds.size() == 1,
            "Downward flashlight correction must raise the light origin without repeating its switch sound.");
        const auto& direction = light.field30;
        require(std::abs(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2] - 1) < 0.00001f,
            "Corrected flashlight direction must remain normalized.");
        lights.beginFrame();
        input.scanner = true;
        input.charged = true;
        frame.run(player, input, lightServices, pose);
        require(illuminated == 2 && light.field18 == camera.position && light.field30 == std::array<float, 3>{0, 0, 1} &&
            pulse.position == input.position && pulse.color == 0xff1f1fff && pulse.distance == 500 &&
            pulse.brightness == 0x5c && pulse.remaining == 10 && pulse.duration == 10,
            "Scanner lighting must use camera aim, suppress illumination and retain the original charged pulse.");
        for (unsigned tick = 0; tick < 10; ++tick) {
            lights.beginFrame();
            pulse.update(lights);
            require(lights.count() == 2 && lights.entry(1).priority == 1 &&
                lights.entry(1).light.referenceDistance == 500 - tick * 50 &&
                lights.entry(1).light.position == input.position && lights.entry(1).light.cutoff == 0 &&
                lights.entry(1).light.color == std::array<std::uint8_t, 4>{255, 31, 31, 255},
                "Owned charged pulse must shrink its radius at priority one before decrementing its lifetime.");
        }
        lights.beginFrame();
        pulse.update(lights);
        require(pulse.remaining == 0 && lights.count() == 1,
            "Expired shared pulse must stop submitting lights while retaining its stored parameters.");
        pulse.start({9, 8, 7}, 0x12345678, 1, 99, 10);
        pulse.remaining = 0.5f;
        pulse.update(lights);
        require(pulse.remaining == -0.5f && lights.count() == 1,
            "Sub-unit pulse radius must skip lighting without clamping the original lifetime undershoot.");
        pulse.start(input.position, 0, 0, 0, 0);
        const auto matrix = light.instance->matrix;
        input.enabled = false;
        frame.run(player, input, lightServices, pose);
        frame.run(player, input, lightServices, pose);
        require(sounds == std::vector<unsigned>{0x837, 0x84c} && !light.state && light.instance->matrix == matrix,
            "Disabling the flashlight must play one switch sound and preserve its inactive model state.");
        const auto retained = state;
        const auto retainedActions = actions;
        const auto retainedAttached = player.components->attached->field2C0;
        const playeractor::FinishFrame finish(resources.executable(), pose);
        state.field1180 = 5;
        state.flags1330.fill(0);
        state.flags1040.fill(0);
        state.field1058 = 0;
        state.flags792 = 4;
        state.field119E = 1;
        actions.field30D = 1;
        actions.field310 = 1;
        actions.fields31C[4] = 0;
        player.components->attached->field2C0 = 0;
        auto& part = *player.components->weapon->first;
        const auto retainedPosition = part.position, retainedDirection = part.field610;
        part.position = {4, 5, 6};
        part.field610 = {0, 0, 1};
        const auto checkGating = [&](bool cleared, bool expected) {
            lights.beginFrame();
            frame.update(player, cleared, 0, finish, lightServices, pose);
            require((light.state != 0) == expected, "Late flashlight admission must follow the original player state guards.");
        };
        checkGating(false, true);
        checkGating(true, false);
        for (auto* flag : {&state.flags1330[3], &state.flags1040[2], &state.field1058, &state.flags1330[2]}) {
            *flag = 1;
            checkGating(false, false);
            *flag = 0;
        }
        state.field119E = 0;
        checkGating(false, false);
        state.field119E = 1;
        state.flags792 = 0;
        checkGating(false, false);
        state.flags792 = 4;
        player.components->attached->field2C0 = 1;
        checkGating(false, false);
        player.components->attached->field2C0 = 0;
        actions.field30D = 0;
        checkGating(false, false);
        actions.field30D = 1;
        state.field1180 = 0;
        checkGating(false, false);
        state = retained;
        actions = retainedActions;
        player.components->attached->field2C0 = retainedAttached;
        part.position = retainedPosition;
        part.field610 = retainedDirection;
    }
    {
        const auto retained = state;
        const auto controlFlag = player.components->controls->flags24[1];
        auto& vacuum = *player.components->vacuum;
        const auto vacuumStrength = vacuum.field4;
        const auto vacuumFlags = vacuum.fields8;
        const playeractor::EnergyFrame energy(resources.executable());
        state.entryCount = 0;
        state.field1004 = 12;
        state.field1008 = 0.5f;
        vacuum.field4 = 123;
        vacuum.fields8 = {1, 2, 3};
        energy.run(player, services.catalog, false);
        const auto limit = state.field1004;
        require(limit == 300 && state.field1008 == -0.5f && vacuum.field4 != 123 &&
            vacuum.fields8 == std::array<std::uint8_t, 3>{0, 2, 3},
            "Unequipped update must reset only its own vacuum flags and retain the original delay undershoot.");
        player.components->controls->flags24[1] = 1;
        state.field1004 = -50;
        energy.run(player, services.catalog, false);
        require(state.field1004 == limit && state.field1008 == -0.5f,
            "Original unconditional capacity assignment must overwrite recharge instead of introducing gradual recovery.");
        unsigned cheap = 381, costly = 381;
        for (unsigned type = 0; type < 381; ++type) {
            if (services.catalog.type(type).field14 < 2) cheap = type;
            else costly = type;
        }
        require(cheap < 381 && costly < 381, "Original actor catalog must provide both energy categories.");
        state.entries[0] = cheap;
        state.entries[1] = costly;
        state.entryCount = 2;
        vacuum.field4 = 77;
        vacuum.fields8[0] = 1;
        state.field1008 = 10;
        energy.run(player, services.catalog, true);
        require(state.field1004 == 270 && state.field1008 == 10 && vacuum.field4 == 77 && vacuum.fields8[0] == 1,
            "Successful vacuum update must preserve its active state while applying original category costs.");
        state.entries.fill(costly);
        state.entryCount = 11;
        energy.run(player, services.catalog, false);
        require(state.field1004 == 0, "Repeated costly entries must retain their individual costs and clamp only after the loop.");
        state.fields11B0.fill(9);
        state.field11D4 = 7;
        state.field11D0 = 23;
        state.field11CC = 19;
        std::uint8_t previous = 0;
        auto& captures = *managers.players().entries;
        require(!energy.beginCaptureReaction(player, captures, previous) && previous == 7 &&
            state.fields11B0 == std::array<std::uint32_t, 7>{} && state.field11D4 == 0 && state.field11D0 == 0 && state.field11CC == 19,
            "Absent capture ownership must clear only the original reaction preamble fields.");
        auto& capture = *captures.entries[0];
        const auto captureState = capture.state;
        auto* source = capture.source;
        capture.state = 1;
        capture.source = &player;
        state.field11D0 = 23;
        state.field11D4 = 4;
        require(energy.beginCaptureReaction(player, captures, previous) && previous == 4 && state.field11D0 == 23,
            "Active capture ownership must preserve the reaction value for the original active branch.");
        capture.state = captureState;
        capture.source = source;
        state = retained;
        player.components->controls->flags24[1] = controlFlag;
        vacuum.field4 = vacuumStrength;
        vacuum.fields8 = vacuumFlags;
    }
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
