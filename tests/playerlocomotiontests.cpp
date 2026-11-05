#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/scene/gameplayresourceowner.h"
#include "game/player/playerinputrouting.h"
#include "game/player/playeractionframe.h"
#include "game/player/playerattachedframe.h"
#include "game/player/playervelocityframe.h"
#include "game/player/playerorientationframe.h"
#include "game/player/playerfinishframe.h"
#include "game/player/playerenvironmentframe.h"
#include "game/player/playerequipmentframe.h"
#include "game/player/playeraudioframe.h"
#include "game/player/playerbreathframe.h"
#include "game/actors/actortimingsampler.h"
#include "../recovered/playerenergyframe.h"
#include "../recovered/playerlatestate.h"
#include "game/player/playercaptureframe.h"
#include "game/player/playercontrolframe.h"
#include "game/player/playertimerframe.h"
#include "game/player/playerreactionframe.h"
#include "game/player/playercontactreactions.h"
#include "game/player/playerreactioneffects.h"
#include "game/player/playerheadtracking.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

void checkOwnedPlayerFinish(const std::filesystem::path& package, const gamesave::State& saves);
void checkPlayerFrameComponents(const std::filesystem::path& package, const gamesave::State& saves);

namespace {
    constexpr unsigned flashlightIdleAction = 0x2a;
    void require(bool condition, const std::string& message) {
        if (!condition) throw std::runtime_error(message);
    }

    float horizontalDistance(const std::array<float, 3>& a, const std::array<float, 3>& b) {
        const float x = a[0] - b[0], z = a[2] - b[2];
        return std::sqrt(x * x + z * z);
    }

    void checkFinish(const std::filesystem::path& package, const gamesave::State& saves) {
        gameentry::State entry;
        std::string error;
        require(gameentry::selectNewFile(saves, 0, entry, error), error);
        gameplay::ResourceOwner owner(package, entry);
        auto& managers = owner.managers();
        auto& player = *managers.playerEvents().player(0);
        auto& state = player.state;
        auto& actions = *player.components->actions;
        const auto resources = managers.preparePlayerDoor(0);
        const actormodel::InitialPose pose(owner.executable());
        const playeractor::FinishFrame finish(owner.executable(), pose);
        std::string calls;
        unsigned damageCalls = 0, damageKind = 0;
        playeractor::FinishServices services{resources.catalog, resources.models,
            [&](auto& recipient, const playerevents::Payload& payload) {
                require(&recipient == &player && payload.word(0x18) == 7 && state.field112C == 7,
                    "Deferred damage dispatch must see the pending amount before it is cleared.");
                require(payload.word(0x20) == damageKind && payload.word(0x14) == 0xffffffff,
                    "Deferred damage must retain original reaction selection and absent source.");
                ++damageCalls;
            },
            [&](bool active) {
                require(bool(state.flags1388[2]) == active, "Interaction ownership must be published before notification.");
                calls += active ? 'b' : 'e';
            },
            [&](auto& recipient) { require(&recipient == &player, "Interaction event must target its player."); calls += 'v'; },
            [&](auto& target) { require(&target == &player, "Interaction notification must use the retained actor."); calls += 'o'; }};
        state.field132C = (player.fieldB4 >> 24) ^ 1;
        state.fieldF68 = 0.375f;
        state.fieldF60 = 2;
        state.fieldF64 = 0.75f;
        state.flags1388[1] = 1;
        finish.run(player, services);
        require(state.flags1330[0] == 1 && state.field132C == player.fieldB4 >> 24 &&
            player.animationSpeed == 0.375f && player.scale == std::array<float, 3>{2, 1.5f, 2} && !state.flags1388[1],
            "Frame completion must publish room change, animation speed and scale, then clear input-edge suppression.");
        finish.run(player, services);
        require(!state.flags1330[0] && calls.empty(), "A stable room and inactive interaction must not produce repeated transitions.");
        state.flags10E8[0] = 1;
        state.field1064 = &player;
        finish.run(player, services);
        require(calls == "bvo", "Interaction entry must acquire ownership, send event 0x3a, then notify the target owner.");
        finish.run(player, services);
        require(calls == "bvo", "An active interaction must not reacquire ownership each frame.");
        state.flags10E8[0] = 0;
        actions.field310 = 2;
        actions.field314 = 0x22;
        finish.run(player, services);
        require(calls == "bvo", "Protected interaction action must retain ownership after the contact flag clears.");
        actions.field314 = 0;
        finish.run(player, services);
        finish.run(player, services);
        require(calls == "bvoe", "Interaction exit must release ownership exactly once.");
        state.field1064 = nullptr;
        std::vector<std::uint8_t> bytes;
        require(dolresources::read(owner.executable(), glme01::playerCaptureConcealFrames, 4, bytes, error), error);
        const auto conceal = static_cast<std::int32_t>((unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) |
            (unsigned(bytes[2]) << 8) | bytes[3]);
        state.field105C = float(conceal);
        require(finish.bodyScale(state) == 0, "Capture concealment starts at zero scale.");
        state.field105C *= 0.5f;
        require(finish.bodyScale(state) == 1, "Capture concealment recovers the original linear half scale.");
        state.field105C = float(conceal) * 2;
        require(finish.bodyScale(state) == -2, "Out-of-range original concealment must not be silently clamped.");
        state.field105C = 0;
        state.field1170 = 1;
        state.field1174 = 0;
        state.field1178 = 0.5f;
        require(finish.bodyScale(state) == 1, "Settled power scaling must retain the original size multiplier.");
        state.field1170 = 0;
        require(finish.bodyScale(state) == 2, "Expired power with no recovery timer must restore base scale.");
        state.field1170 = 1;
        state.field1174 = 20;
        require(std::abs(finish.bodyScale(state) - 2) < 0.00001f,
            "Original twenty-frame power transition starts at base scale.");
        state.field1174 = 10;
        require(std::abs(finish.bodyScale(state) - 1.5f) < 0.00001f,
            "Imported oscillation table must produce the half-transition scale.");
        state.field1174 = 8;
        require(std::abs(finish.bodyScale(state) - 1) < 0.00001f,
            "Power scaling retains its original intermediate oscillation rather than a linear blend.");
        state.field1174 = 4;
        require(std::abs(finish.bodyScale(state) - 2) < 0.00001f,
            "Power scaling must return to base scale on the next original oscillation.");
        state.field1170 = 0;
        state.field1174 = 20;
        require(std::abs(finish.bodyScale(state) - 1) < 0.00001f,
            "Recovery begins at the altered scale using its distinct interpolation formula.");
        state.field1174 = 8;
        require(std::abs(finish.bodyScale(state) - 2) < 0.00001f,
            "Recovery reverses the original oscillation endpoints.");
        state.field1174 = 0;
        const auto& type = resources.catalog.type(player.actorType);
        bool found = false;
        for (unsigned animation = 0; animation < type.animations.size() && !found; ++animation) {
            const auto timing = resources.models.timing(type.model, type.animations[animation][0]);
            if (actoranimation::sampleTiming(timing.get(), "Ldam_timing", 0) == -10000) continue;
            for (unsigned frame = 1; frame < 600 && !found; ++frame) {
                if (actoranimation::timingCrossing(timing.get(), "Ldam_timing", float(frame - 1), float(frame)) <= 0) continue;
                player.animationState = animation;
                player.animationFrame = float(frame);
                player.animationFrameStep = 1;
                actions.field310 = 2;
                actions.field314 = 0x20;
                state.field112C = 7;
                finish.run(player, services);
                require(damageCalls == 1 && state.field112C == 0, "The original damage marker must consume pending damage once.");
                finish.run(player, services);
                require(damageCalls == 1, "Revisiting the marker must not repeat consumed damage.");
                actions.field314 = 0;
                damageKind = 1;
                state.field112C = 7;
                finish.run(player, services);
                require(damageCalls == 2 && !state.field112C, "Ordinary deferred damage must select reaction kind one.");
                player.animationFrameStep = 0;
                state.field112C = 7;
                finish.run(player, services);
                require(damageCalls == 2 && state.field112C == 7, "Without a marker crossing, pending damage must remain queued.");
                found = true;
            }
        }
        require(found, "The imported Luigi animations must supply a real deferred-damage timing marker.");
        std::cout << "Player frame completion: room, scale, interaction lifecycle and original damage marker verified.\n";
    }

    void run(const std::filesystem::path& package, gamesave::State& saves, unsigned direction, unsigned idleFrames, std::ostream& trace) {
        std::string error;
        gameentry::State entry;
        require(gameentry::selectNewFile(saves, 0, entry, error), error);
        gameplay::ResourceOwner owner(package, entry);
        auto& managers = owner.managers();
        auto* player = managers.playerEvents().player(0);
        require(player != nullptr, "Locomotion requires the manager-owned player.");
        require(managers.enemies().playerCount() == 1, "Normal startup must not create a second collidable Luigi.");
        require((player->state.flags792 & 12) == 4, "The first visit must retain flashlight ownership without the Poltergust.");
        owner.publishCamera();
        const auto services = managers.preparePlayerDoor(0);
        const auto firstVisitIdleAnimation = player->components->actions->entries.at(flashlightIdleAction);
        const actormodel::InitialPose pose(owner.executable());
        const playeractions::Frame actions(owner.executable(), pose);
        const playercomponents::AttachedFrame attached(owner.executable());
        const playermotion::VelocityFrame velocity(owner.executable(), pose);
        const playerenvironment::Frame environment(owner.executable());
        const playeractor::EquipmentFrame equipment(owner.executable());
        const playeractor::EnergyFrame energy(owner.executable());
        const playeractor::CaptureFrame capture(owner.executable());
        const playercontrols::Frame controls(owner.executable());
        const playeractor::TimerFrame timers(owner.executable());
        const playeractions::ContactFrame contact(owner.executable());
        const playercomponents::HeadFrame head(owner.executable());
        const playeractor::LateState lateState(owner.executable());
        unsigned lightFrames = 0;
        std::uint8_t specialActive = 0;
        std::int32_t audioLife = player->life;
        playerevents::LifeEnvironment life{audioLife, managers.particles().effects(),
            [&](unsigned id, const auto& position) { managers.playback()->play(id, position); },
            [](const auto&) { throw std::runtime_error("Locomotion fixture unexpectedly requested death playback."); },
            [](const auto&, const auto&, auto, auto, auto, auto) {
                throw std::runtime_error("First-visit locomotion unexpectedly requested money scattering.");
            }};
        playeractor::AudioFrame audio(owner.executable());
        const playeractor::BreathFrame breath(owner.executable());
        unsigned footstepTicks = 0;
        unsigned roomNotifications = 0;
        const playerenvironment::Services environmentServices{managers.collision(), services.catalog, services.models,
            pose, managers.enemies(), *managers.players().entries,
            [](auto&) { throw std::runtime_error("First-visit locomotion unexpectedly requested a player-type change."); },
            [](const auto&) -> playeractor::Target* { throw std::runtime_error("First-visit locomotion unexpectedly acquired a vacuum target."); },
            [&](unsigned room) { return managers.roomCleared(room); },
            [](bool) { throw std::runtime_error("First-visit locomotion unexpectedly requested capture audio."); },
            [&](unsigned room) {
                require(room == player->fieldB4, "Environment audio notification must carry the actual packed room.");
                managers.music().notifyRoom(room, entry.map);
                require(managers.music().routing().room == (room & 255),
                    "Environment notification must reach the production music owner's room routing.");
                ++roomNotifications;
            }};
        const playeractor::FinishFrame finish(owner.executable(), pose);
        const playeractor::FinishServices finishServices{services.catalog, services.models};
        require(!player->components->quaternion->field10, "The constructor must retain the originally unwritten turning slot.");
        const playeractions::FrameServices actionServices{managers.flags(), services.catalog, services.models,
            [](unsigned id) { throw std::runtime_error("Locomotion fixture reached global sound " + std::to_string(id)); },
            [&](unsigned id, const auto& position) { managers.playback()->play(id, position); }};
        auto origin = player->position;
        const auto spawn = origin;
        unsigned grounded = 0, movingAnimations = 0, wallContacts = 0;
        bool returnedToIdle = false;
        std::array<float, 3> releasedPosition{}, wallPosition{};
        for (unsigned frame = 0; frame < 300; ++frame) {
            if (frame == idleFrames) {
                origin = player->position;
                require(horizontalDistance(spawn, origin) < 0.001f,
                    "Idle contact processing must not displace Luigi from his original spawn.");
            }
            playerinput::PadStatus pad;
            if (frame >= idleFrames && frame < 210) {
                if (direction == 0) pad.stickX = 87;
                else if (direction == 1) pad.stickX = -87;
                else if (direction == 2) pad.stickY = 87;
                else pad.stickY = -87;
            }
            managers.sampleInput(pad, 0);
            environment.run(*player, environmentServices);
            capture.run(*player, managers.playerEvents(), managers.particles().effects());
            if (frame == 0) require(player->state.field10E0 >= 0,
                "Before contact integration, the original forward probe must resolve the imported foyer floor.");
            managers.updatePlayerInput(0, true, {});
            equipment.run(*player, actionServices.positionalSound);
            require(player->state.field1180 != 1, "First-visit scheduler must not enter the active vacuum branch.");
            energy.run(*player, services.catalog, false);
            std::uint8_t previousReaction;
            require(!energy.beginCaptureReaction(*player, *managers.players().entries, previousReaction),
                "First-visit scheduler must follow the actual manager's absent-capture branch.");
            timers.run(*player, {life, false, frame,
                [](bool) { throw std::runtime_error("First-visit locomotion unexpectedly requested an audio-rate change."); },
                [](auto&, const auto&, unsigned) { throw std::runtime_error("Locomotion unexpectedly reached deferred attack damage."); },
                [](auto&, unsigned, unsigned, unsigned, unsigned) { throw std::runtime_error("Locomotion unexpectedly requested a weapon pulse."); },
                [](unsigned, const auto&) { throw std::runtime_error("Locomotion unexpectedly requested an attack matrix effect."); }});
            controls.charge(*player, life);
            controls.attack(*player);
            controls.blink(*player, frame);
            controls.special(*player, frame, false, specialActive, life);
            playeractions::updateState(*player, life);
            contact.run(*player, managers.collision(), *player->state.camera, life, false,
                [&](unsigned type, const auto& position, const auto& direction) {
                    managers.particles().effects().create(type, position, direction);
                });
            actions.run(*player, managers.collision(), actionServices);
            const auto attachment = attached.run(*player, services.catalog, services.models, pose);
            auto& weapon = *player->components->weapon;
            const auto& action = *player->components->actions;
            const auto& upper = *player->components->attached;
            weapon.update(owner.executable(), *player, player->state.field7A4, player->state.field7A8,
                action.field60, upper.field248, upper.field2C0, action.query74544(), player->view.field8C0, pose, {});
            velocity.run(*player, managers.collision(), {}, actionServices.positionalSound);
            velocity.constrain(*player);
            managers.updatePlayerOrientation(0, attachment, frame);
            playeractor::updateReactionEffects(*player, services.catalog, services.models,
                managers.particles().effects(), managers.modelEffects().factory, life.sound);
            if (frame == 0) require(player->components->quaternion->field10 == 0,
                "The initial direction lock must clear retained turning speed through the original update.");
            finish.run(*player, finishServices);
            require(player->view.room == player->fieldB4 && player->view.direction == &player->state.direction,
                "The completed player frame must publish the current room and direction to its manager view.");
            managers.updateContacts();
            managers.updatePoses();
            lateState.begin(*player, {false,
                [](auto&, bool) { throw std::runtime_error("First-visit late frame unexpectedly requested scanner cancellation."); }});
            require((player->surface().materials.at(1).flags & 2) && !(player->state.field7A4->flags & 0xc000) &&
                (player->state.field7A8->flags & 0xc000) == 0xc000,
                "First-visit late state must retain flashlight visibility and hide vacuum equipment.");
            capture.updateOpacity(*player);
            player->state.fieldF5C = 255;
            weapon.updateEndpoints(*player->state.field7A4, *player->state.field7A8);
            audio.run(*player, {services.catalog, services.models, managers.collision(), *managers.playback(), frame,
                [&](unsigned room, std::int16_t polygon) {
                    managers.playback()->setPlayerContext(managers.audioContext().parameters(room, polygon).packed);
                    managers.music().setRoom(room, polygon);
                }, [](unsigned) { return false; }, [](unsigned) -> unsigned {
                    throw std::runtime_error("Locomotion unexpectedly requested cutscene feedback.");
                }});
            footstepTicks += player->state.field1118 > 0;
            breath.run(*player, services.catalog, services.models, managers.roomCleared(player->room() >> 24));
            head.run(*player, managers.actors(), managers.enemies(), services.catalog, pose);
            managers.lights().beginFrame();
            require(managers.players().frame() == float(frame), "Shared player clock must retain one advance per original manager frame.");
            managers.updatePlayerLight(0);
            if (player->components->light->state) {
                ++lightFrames;
                require(managers.lights().count() == 2 && managers.lights().entry(1).priority == 8 && player->components->light->owner == player &&
                    player->components->light->fieldC == weapon.first->position,
                    "Late flashlight update must use the manager-owned player, endpoint and room-light pool.");
                const auto& cone = *managers.players().models->animation(2);
                require(cone.frame == float(frame % static_cast<unsigned>(cone.frameCount)),
                    "Manager-bound cone animation must consume the shared player clock rather than a separate light clock.");
            }
            player->components->attached->finish(player->state.fieldF6C, services.catalog, services.models,
                actionServices.positionalSound, managers.playback()->animationServices(player->position));
            lateState.scanner(*player->components->scanner, *managers.players().models);
            player->refreshView();
            managers.players().state->update(managers.lights());
            managers.players().advanceClock();
            owner.publishCamera();
            for (const auto value : weapon.first->position) require(std::isfinite(value), "Flashlight endpoint must remain finite.");
            require(weapon.firstType == 7 && player->state.field7A4->parent == player,
                "The weapon frame must retain the original flashlight child ownership.");
            for (const auto value : player->position) require(std::isfinite(value), "Locomotion produced a non-finite position.");
            for (const auto value : player->state.quaternion) require(std::isfinite(value), "Locomotion produced a non-finite orientation.");
            require(player->room() != 0xffffffff, "Locomotion lost its room owner.");
            grounded += player->fieldA0 >= 0 || player->fieldB2 >= 0;
            wallContacts += player->fieldB0 >= 0;
            if (frame >= idleFrames && frame < 210) movingAnimations += player->animationState != firstVisitIdleAnimation;
            if (frame >= 210 && player->animationState == firstVisitIdleAnimation) returnedToIdle = true;
            if (frame == 180) wallPosition = player->position;
            if (frame > 180 && frame < 210) require(horizontalDistance(wallPosition, player->position) < 0.01f,
                "Sustained input must not push Luigi through the imported foyer wall.");
            if (frame == 240) releasedPosition = player->position;
            trace << idleFrames << ',' << direction << ',' << frame << ',' << player->position[0] << ',' << player->position[1] << ',' <<
                player->position[2] << ',' << player->velocity[0] << ',' << player->velocity[1] << ',' << player->velocity[2] << ',' <<
                player->animationState << ',' << player->animationFrame << ',' << player->fieldB2 << ',' << player->fieldB0 << '\n';
        }
        require(horizontalDistance(origin, player->position) > 10, "Held controller input did not move Luigi through the imported foyer.");
        require(roomNotifications == 1, "Staying in the foyer must publish its audio room once, not every frame.");
        require(lightFrames > 250, "Original flashlight state gating must remain active during first-visit movement.");
        require(player->state.field1180 == 5 && player->state.field7A4->actorType == 7,
            "The uncleared foyer must select the real flashlight attachment without a vacuum.");
        require(grounded > 250, "Locomotion did not retain the imported foyer's floor contact.");
        require(wallContacts > 0, "The held-input path must exercise an actual map wall.");
        require(movingAnimations > 0, "Movement did not select an original locomotion animation.");
        require(footstepTicks > 0, "Movement must cross original footstep timing markers and submit real sound handles.");
        const auto& finalAction = *player->components->actions;
        require(returnedToIdle && finalAction.field310 == 1 && finalAction.field60 == 0x50 &&
            finalAction.fields31C[4] == 6 && player->animationState == finalAction.entries.at(0x50) && player->state.field116C == 90,
            "Released movement must return to idle, then enter the original flashlight-only idle-timeout animation.");
        require(horizontalDistance(releasedPosition, player->position) < 1, "Released input did not settle through original friction.");
        if (direction == 0 && idleFrames == 0) {
            const auto equipmentFlags = player->state.flags792;
            const auto apply = [&](const playerinput::PadStatus& pad) {
                managers.sampleInput(pad, 0);
                environment.run(*player, environmentServices);
                managers.updatePlayerInput(0, true, {});
                equipment.run(*player, actionServices.positionalSound);
                require(player->state.flags792 == equipmentFlags && player->state.field1180 == 5 && !player->state.flags1140[2],
                    "Pre-Poltergust buttons and triggers must not grant vacuum, elements or scanner access.");
            };
            for (unsigned button = 0; button < 16; ++button) {
                playerinput::PadStatus pad;
                pad.buttons = static_cast<std::uint16_t>(1u << button);
                apply(pad);
                apply({});
            }
            for (unsigned trigger = 0; trigger < 3; ++trigger) {
                playerinput::PadStatus pad;
                pad.triggerLeft = trigger != 1 ? 255 : 0;
                pad.triggerRight = trigger != 0 ? 255 : 0;
                apply(pad);
                apply({});
            }
            const auto room = player->fieldB4 >> 24;
            const auto originalRoomFlags = owner.roomFlags().at(room);
            owner.roomFlags().at(room) |= 0x8000;
            environment.run(*player, environmentServices);
            equipment.run(*player, actionServices.positionalSound);
            require(managers.roomCleared(room) && !player->state.field119D && !player->state.field1180,
                "The original cleared-room flag must put away the flashlight.");
            managers.flags().setOn(0x3d);
            environment.run(*player, environmentServices);
            equipment.run(*player, actionServices.positionalSound);
            require(!managers.roomCleared(room) && player->state.field119D && player->state.field1180 == 5,
                "The original map-two override must restore the uncleared-room flashlight behavior.");
            managers.flags().setOff(0x3d);
            owner.roomFlags().at(room) = originalRoomFlags;
        }
        std::cout << "Idle frames " << idleFrames << ", direction " << direction << ": distance " << horizontalDistance(origin, player->position) <<
            ", grounded " << grounded << ", wall contacts " << wallContacts << ", moving animation ticks " << movingAnimations << '\n';
    }
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) throw std::invalid_argument("Supply the imported asset package for the locomotion check.");
        gameassets::Package package;
        std::vector<std::uint8_t> executable, initialFlags;
        std::string error;
        require(package.open(argv[1], error) && package.read("sys/main.dol", executable, error) &&
            dolresources::read(executable, glme01::initialSaveFlags, 31, initialFlags, error), error);
        gamesave::State saves;
        require(gamesave::initialize(initialFlags, saves, error), error);
        checkFinish(argv[1], saves);
        checkOwnedPlayerFinish(argv[1], saves);
        checkPlayerFrameComponents(argv[1], saves);
        std::ofstream trace;
        if (argc > 2) {
            trace.open(argv[2]);
            require(bool(trace), "Cannot open the requested locomotion trace.");
            trace << "idle_frames,direction,frame,x,y,z,vx,vy,vz,animation,animation_frame,floor,wall\n";
        }
        for (unsigned idleFrames : {0u, 30u})
            for (unsigned direction = 0; direction < 4; ++direction) run(argv[1], saves, direction, idleFrames, trace);
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
