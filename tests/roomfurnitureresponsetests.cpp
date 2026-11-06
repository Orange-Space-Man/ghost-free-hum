#include "game/world/roomfurnitureresponse.h"
#include "../recovered/roomfurnitureswing.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"

#include <iostream>
#include <cmath>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "The imported package is required.");
        gameassets::Package package;
        std::string error;
        std::vector<std::uint8_t> executable, compressed, bytes;
        require(package.open(argv[1], error) && package.read("sys/main.dol", executable, error), error.c_str());
        require(package.read("files/Map/map2.szp", compressed, error) && roomarchive::decompress(compressed, bytes, error), error.c_str());
        mission::Resources mission;
        enemytypes::Catalog types;
        require(mission.read(bytes, error) && types.read(executable, error), error.c_str());
        const characteritems::Admission admission(executable, types);
        const auto* firstVisitChoices = mission.getJmpResource("itemappeartable");
        require(firstVisitChoices && firstVisitChoices->fieldCount(), "First-visit furniture requires the original item choices.");
        for (unsigned field = 0; field < firstVisitChoices->fieldCount(); ++field)
            require(admission.nothing(characteritems::find(mission, firstVisitChoices->fieldString(0, field))),
                "Every outcome in the ordinary foyer furniture table must be the original nothing item.");
        const roomfurniture::Response response(executable, admission);
        const dooranimation::PoseTables trig = [&] {
            dooranimation::PoseTables result;
            require(result.read(executable, error), error.c_str());
            return result;
        }();
        roomfurniture::Object object;
        object.row = 99;
        object.room = 0x02000102;
        object.position = {570, 0, -80};
        object.moveLevel = 3;
        roomfurniture::RowState row;
        auto selected = characteritems::find(mission, "nothing");
        unsigned selections = 0, creations = 0, releases = 0, saves = 0, animations = 0;
        unsigned result = 7;
        bool cleared = false;
        std::vector<unsigned> effects, sounds, scatter;
        std::string order;
        roomfurniture::ResponseServices services;
        services.roomCleared = [&](unsigned room) { require(room == 2, "Clear-state query must use the room index."); return cleared; };
        services.selectItem = [&](unsigned) { ++selections; order += 'q'; return selected; };
        services.createItem = [&](const auto&, const auto&) { ++creations; order += 'c'; return result; };
        services.createType = [&](unsigned type, const auto&) { scatter.push_back(type); };
        services.scatter = [&](bool bills, float speed, const auto& point, const auto& direction, unsigned count) {
            require(speed > 0 && point[0] == 570 && point[2] == -80 && direction[1] != 0 && count == 15,
                "Scatter must retain original launch parameters and truncate its count to one byte.");
            scatter.push_back(bills ? 2 : 1);
        };
        services.effect = [&](unsigned id, const auto&) { effects.push_back(id); order += 'e'; };
        services.sound = [&](unsigned id, const auto& point, const float* amplitude) {
            require(point == object.position, "Furniture audio must use the furniture position.");
            if (amplitude) require(*amplitude == 6, "Suction audio must retain strength times movement level.");
            sounds.push_back(id);
            order += 's';
        };
        services.releaseOccupant = [&](unsigned index, const auto&) { require(index == 99, "Release must use placement row."); ++releases; order += 'r'; };
        services.persistCollected = [&] { require(row.flags & 8, "Collection flag must precede persistence."); ++saves; order += 'p'; };
        services.animation = [&](auto& target, unsigned animation) { target.animation = static_cast<std::uint8_t>(animation); target.frame = 0; ++animations; order += 'a'; };

        row.flags = 1;
        for (unsigned tick = 0; tick < 19; ++tick) {
            object.updates = tick;
            require(response.strike(object, row, true, false, services) == 1 && selections == 0,
                "Strike must not collect before update twenty.");
        }
        require(response.strike(object, row, true, false, services) == 2 && !(row.flags & 1) && selections == 1 &&
            object.collectionFlags == 1 && effects == std::vector<unsigned>{0x61} && sounds == std::vector<unsigned>{0x1877} &&
            animations == 1 && order == "qesa", "Strike completion must collect before selecting the reaction animation.");
        require(response.strike(object, row, true, false, services) == 0 && object.strikeTicks == 0,
            "Inactive strike must reset its retained timer.");

        object.collectionFlags = 0;
        object.animation = 0;
        row.flags = 1;
        row.reactionTwo = true;
        sounds.clear();
        for (unsigned tick = 0; tick < 20; ++tick) {
            object.updates = tick + 2;
            response.strike(object, row, false, true, services);
            require(object.angles[2] == static_cast<int>(300.0f * trig.sineAngle(object.updates * 2000)),
                "Second reaction must use the original sine-table phase.");
        }
        require(sounds.size() == 2 && selections == 1 && object.collectionFlags == 0,
            "Suppressed strike collection still plays sounds at timer zero and sixteen.");
        sounds.clear();
        for (unsigned tick = 0; tick < 30; ++tick) {
            object.updates = tick;
            response.suction(object, 2, true, tick, services);
        }
        require(object.suctionTicks == 30 && object.animation == 1 && animations == 2 && sounds.size() == 8,
            "Suction must select animation on update thirty and sound on every fourth global update.");
        object.angles[2] = -600;
        response.suction(object, 0, true, 32, services);
        require(object.angles[2] == -344 && object.suctionTicks == 0, "Stopped suction must approach zero by the original short-angle step.");

        selected = characteritems::find(mission, "money");
        require(admission.money(selected), "Original money record must be available.");
        object.generate = 3;
        object.generateCount = 271;
        object.collectionFlags = 0;
        row.flags = 0;
        effects.clear();
        response.collect(object, row, true, services);
        require(scatter == std::vector<unsigned>({1, 2}) && effects == std::vector<unsigned>({0x54, 0x55}) &&
            (row.flags & 8) && object.collectionFlags == 1 && saves == 1, "Mixed loot must scatter coins before bills and persist once.");
        const auto previousSelections = selections;
        response.collect(object, row, true, services);
        require(selections == previousSelections && scatter.size() == 2 && effects.back() == 0x61,
            "Repeated ordinary collection must not select or duplicate loot.");
        object.collectionFlags = 0;
        response.collect(object, row, true, services);
        require(selections == previousSelections + 1 && scatter.size() == 2 && saves == 1,
            "Persisted money flag must prevent re-created furniture from duplicating money.");

        const auto* info = mission.getJmpResource("iteminfotable");
        bool checkedRepeatable = false;
        for (unsigned index = 0; index < info->count(); ++index) {
            const jmp::ToolDataRef item{info, static_cast<int>(index)};
            if (admission.nothing(item) || admission.money(item) || !admission.redirected(item)) continue;
            selected = item;
            object.collectionFlags = 0;
            response.collect(object, row, true, services);
            require(object.collectionFlags == 2 && creations == 1, "Redirected item must mark the visibility-local attempt without consuming ordinary loot.");
            response.collect(object, row, true, services);
            require(creations == 1 && effects.back() == 0x61, "Repeated redirected attempt must not create a duplicate.");
            object.collectionFlags &= ~2u;
            result = 0xffffffff;
            response.collect(object, row, true, services);
            require(creations == 2 && object.collectionFlags == 2 && effects.back() == 0x61,
                "Failed redirected creation must still mark its attempt.");
            checkedRepeatable = true;
            break;
        }
        require(checkedRepeatable, "Original map must provide a redirected item fixture.");
        row.occupant = 3;
        row.flags = 6;
        cleared = true;
        response.occupant(object, row, true, services);
        require(releases == 0 && row.flags == 6, "Covered occupant must remain owned despite a forced strike.");
        row.flags = 4;
        response.occupant(object, row, false, services);
        require(releases == 1 && !(row.flags & 4), "Uncovered occupant notification must consume its release flag.");
        object.suctionTicks = 30;
        response.occupant(object, row, false, services);
        require(releases == 1, "Suction occupant release uses a strict greater-than-thirty boundary.");
        object.suctionTicks = 31;
        response.occupant(object, row, false, services);
        require(releases == 2, "Suction must release the occupant after update thirty.");
        const auto beforeBlocked = selections;
        response.collect(object, row, true, services);
        require(selections == beforeBlocked, "Cleared-room occupied furniture must not select loot.");
        cleared = false;
        row.flags = 4;
        response.occupant(object, row, true, services);
        require(releases == 2 && row.flags == 0, "Uncleared room must clear its request without releasing the occupant.");
        row.occupant = -1;
        row.reactionTwo = true;
        row.flags = 1;
        object.strikeTicks = 0;
        object.collectionFlags = 3;
        object.behavior = 0;
        object.updates = 5;
        object.counter = 0x123;
        object.suctionTicks = 29;
        unsigned messages = 0;
        services.scanMessage = [&](unsigned message) {
            require(message == 0x23 && (row.flags & 0x20), "Scan callback must see its pending row and truncated counter.");
            ++messages;
        };
        response.basic(object, row, false, false, 0, 1, services);
        require(object.collectionFlags == 1 && object.strikeTicks == 0 && object.suctionTicks == 29,
            "Invisible basic furniture must only reset the repeat-attempt flag.");
        row.flags |= 0x20;
        response.basic(object, row, true, false, 0, 1, services);
        const auto strikeAngle = static_cast<int>(300.0f * trig.sineAngle(object.updates * 2000));
        const auto expected = strikeAngle > 256 ? strikeAngle - 256 : strikeAngle < -256 ? strikeAngle + 256 : 0;
        require(object.angles[2] == expected && object.suctionTicks == 0 && object.strikeTicks == 1 &&
            messages == 1 && !(row.flags & 0x20), "Basic handler must relax after its strike and consume a scanned message.");
        object.behavior = 15;
        object.strikeTicks = 0;
        object.frame = 5;
        response.basic(object, row, true, false, 0, 1, services);
        require(object.angles[2] == strikeAngle && object.frame == 6,
            "Animated handler must strike after relaxation and advance its animation frame.");
        object.behavior = 22;
        object.collectionFlags = 3;
        response.basic(object, row, false, false, 0, 1, services);
        require(object.collectionFlags == 3, "Mirror handler must preserve repeat flags while invisible.");
        row.flags = 0x20;
        response.basic(object, row, true, false, 0, 1, services);
        require(messages == 1 && (row.flags & 0x20), "Mirror handler must not consume the basic handler's scan-message request.");
        const roomfurniture::Swing swinging(executable);
        roomfurniture::SwingState swingState;
        roomfurniture::SwingServices swingServices;
        unsigned swingSounds = 0;
        swingServices.sound = [&](unsigned, const auto& point, const auto& velocity) {
            require(point == object.position && (std::abs(velocity[0]) > 1 || std::abs(velocity[1]) > 1),
                "Swing audio uses both retained angular velocities at the furniture position.");
            ++swingSounds;
        };
        object.behavior = 7;
        object.arguments[0] = 2;
        object.swing = {1800, 0};
        object.swingVelocity = {0, 0};
        object.collectionFlags = 0;
        row.flags = 0;
        selected = characteritems::find(mission, "nothing");
        const auto beforeSwing = selections;
        swinging.update(object, row, swingState, true, false, response, services, swingServices);
        require(selections == beforeSwing && swingState.collected == 0, "The swing collection threshold is strict.");
        object.swing[0] = 1801;
        swinging.update(object, row, swingState, true, false, response, services, swingServices);
        require(selections == beforeSwing + 1 && swingState.collected == 1,
            "Nonzero spring selectors use the original 1800-angle collection threshold even without a spring update.");
        swinging.update(object, row, swingState, true, false, response, services, swingServices);
        require(selections == beforeSwing + 1, "A sustained swing cannot repeat the collection callback.");
        object.swing = {100, -99};
        swinging.update(object, row, swingState, true, false, response, services, swingServices);
        require(swingState.collected == 1, "Exactly one hundred angle units must not rearm collection.");
        object.swing[0] = 99;
        swinging.update(object, row, swingState, true, false, response, services, swingServices);
        require(swingState.collected == 0, "Both small angles and small velocities rearm swing collection.");
        object.arguments[0] = 0;
        object.swing = {500, -500};
        object.swingVelocity = {0, 0};
        object.suctionTicks = 12;
        swingState.target = {100, 200};
        swinging.update(object, row, swingState, false, false, response, services, swingServices);
        require(object.swing[0] == 500 && object.suctionTicks == 12 && swingState.target[0] == 100,
            "Invisible swinging furniture retains its complete simulation state.");
        swinging.update(object, row, swingState, true, false, response, services, swingServices);
        require(object.swing[0] < 500 && object.swing[1] > -500 && object.suctionTicks == 0 &&
            swingState.target == std::array<float, 2>{0, 0} && swingState.previousVelocity == object.swingVelocity && swingSounds == 1,
            "Unequipped swing relaxes both springs and publishes the angular velocity used by collision.");
        object.itemOffset = 100;
        swingServices.suction = [&](float radius, const auto& point, auto& direction) {
            require(radius > 0 && point[1] == object.position[1] - 100,
                "Swing suction queries its original offset point.");
            direction = {1, 0, 0};
            return 1.0f;
        };
        swinging.update(object, row, swingState, true, true, response, services, swingServices);
        require(object.suctionTicks == 1 && swingState.target != std::array<float, 2>{0, 0},
            "Active suction converts the pulled direction through the original angle table.");
        require(swinging.sound({0, 0}, 1).stop, "A resting swing stops its retained sound.");
        const auto loud = swinging.sound({1000000, -2000000}, 0);
        const auto reversed = swinging.sound({-2000000, 1000000}, loud.pitch);
        require(!loud.stop && loud.changePitch && loud.volume > 0 && loud.pitch == reversed.pitch &&
            loud.volume == reversed.volume && !reversed.changePitch,
            "Swing sound saturates on the larger absolute velocity and retains an unchanged pitch.");
        roomfurniture::AnimatedSwingState animatedState;
        roomfurniture::AnimatedSwingServices animated;
        animated.playerPosition = object.position;
        unsigned queries = 0, randomCalls = 0, cameraStarts = 0, cameraEnds = 0, actions = 0, damages = 0, reactions = 0;
        animated.messageBusy = [&] { ++queries; return false; };
        animated.random = [&] { ++randomCalls; return 0.0f; };
        animated.camera = [&](bool start) { start ? ++cameraStarts : ++cameraEnds; };
        animated.playerAction = [&](unsigned action) { require(action == 0x55, "Animated furniture requests the original player action."); ++actions; };
        animated.playerDamage = [&](const auto& point, unsigned amount, unsigned reaction) {
            require(point == object.position && amount == 10 && reaction == 6, "Animated furniture retains its damage payload.");
            ++damages;
        };
        animated.jointPoint = [&](const auto&, const auto&) { return object.position; };
        animated.actorReaction = [&](const auto& point, const auto&, const auto&) {
            require(point == object.position, "Furniture reaction uses the transformed joint point."); ++reactions;
        };
        animated.animationEnd = [&](const auto&) { return 120u; };
        animated.globalSound = [&](unsigned id) { sounds.push_back(id); };
        object.behavior = 20;
        object.swing = object.swingVelocity = {0, 0};
        object.frame = 0;
        row.flags = 0;
        swinging.updateAnimated(object, row, animatedState, true, false, response, services, swingServices, animated);
        require(queries == 0 && randomCalls == 0 && actions == 0,
            "The disabled mission flag must leave first-visit animated furniture on its ordinary swing route.");
        animated.enabled = true;
        swinging.updateAnimated(object, row, animatedState, true, false, response, services, swingServices, animated);
        require(randomCalls == 1 && actions == 0 && animatedState.proximity == 1,
            "Proximity selection is retained until the following frame before starting the sequence.");
        swinging.updateAnimated(object, row, animatedState, true, false, response, services, swingServices, animated);
        require(cameraStarts == 1 && actions == 1 && animatedState.mode == 1 && animatedState.delay == 1,
            "Camera ownership and player action precede the first delay increment.");
        unsigned delayFrames = 0;
        while (animatedState.mode == 1 && delayFrames++ < 1000)
            swinging.updateAnimated(object, row, animatedState, true, false, response, services, swingServices, animated);
        require(delayFrames == 50 && animatedState.mode == 2 && object.animation == 1 && object.frame == 0 && animatedState.delay == 0,
            "The original delay selects the active animation and resets its frame before execution.");
        swinging.updateAnimated(object, row, animatedState, false, false, response, services, swingServices, animated);
        require(damages == 1 && object.frame == 1,
            "An active furniture animation continues its damage stage even outside visibility.");
        object.frame = 15;
        swinging.updateAnimated(object, row, animatedState, true, false, response, services, swingServices, animated);
        require(cameraEnds == 1, "Frame 15 releases the original camera mode.");
        object.frame = 75;
        swinging.updateAnimated(object, row, animatedState, true, false, response, services, swingServices, animated);
        require(effects.back() == 0x66 && reactions == 0, "The frame-75 effect still belongs to the player-damage stage.");
        object.frame = 80;
        const auto previousDamage = damages;
        swinging.updateAnimated(object, row, animatedState, true, false, response, services, swingServices, animated);
        require(reactions == 1 && damages == previousDamage, "Frame 80 switches from player damage to actor reaction.");
        object.frame = 110;
        swinging.updateAnimated(object, row, animatedState, true, false, response, services, swingServices, animated);
        require(sounds.back() == 0x4820 && animatedState.hit == 0, "Frame 110 selects the hit sound and consumes the hit marker.");
        object.frame = 119;
        swinging.updateAnimated(object, row, animatedState, true, false, response, services, swingServices, animated);
        require(animatedState.mode == 0 && object.animation == 0 && object.frame == 0 && actions == 1,
            "The terminal frame restores the idle animation without restarting the nearby trigger.");
        animatedState.mode = 2;
        object.frame = 110;
        swinging.updateAnimated(object, row, animatedState, true, false, response, services, swingServices, animated);
        require(sounds.back() == 0x4822, "An animation with no retained player hit uses the original miss sound.");
        animatedState.mode = 0;
        animated.playerPosition[0] += 100000;
        row.flags = 0x20;
        const auto previousMessages = messages;
        swinging.updateAnimated(object, row, animatedState, false, false, response, services, swingServices, animated);
        require(animatedState.proximity == 0 && messages == previousMessages + 1 && !(row.flags & 0x20),
            "Animated furniture rearms at distance and consumes scan requests even when its swing is not visible.");
        object = {};
        object.row = 99;
        object.room = 0x02000102;
        object.position = {570, 0, -80};
        object.behavior = 17;
        object.rotationAmplitude = 1000;
        object.angles[0] = 2000;
        object.updates = 2;
        row = {};
        unsigned rotationSounds = 0;
        services.sound = [&](unsigned id, const auto& point, const float* volume) {
            require(id == 0x1868 && point == object.position && volume && *volume > 0 && *volume <= 1,
                "Rotating furniture submits its own positional sound and capped volume.");
            ++rotationSounds;
        };
        response.rotating(object, row, false, false, services);
        require(object.rotationAmplitude == 1000 && object.angles[0] == 2000 && !rotationSounds,
            "Invisible rotating furniture must retain its angle, amplitude and audio state.");
        response.rotating(object, row, true, false, services);
        require(object.rotationAmplitude > 0 && object.rotationAmplitude < 1000 && object.previousRotation == 2000 &&
            object.angles[0] == static_cast<int>(object.rotationAmplitude) && rotationSounds == 1,
            "Rotation decays before computing its next angle and compares the previous angle before emitting sound.");
        object.updates = 6;
        response.rotating(object, row, true, false, services);
        require(object.angles[0] == (static_cast<int>(-object.rotationAmplitude) & 0xffff) && rotationSounds == 1,
            "Negative rotation keeps the original unsigned sixteen-bit mask and comparison behavior.");
        object.rotationAmplitude = 0;
        object.angles = {};
        object.suctionTicks = 0;
        services.suctionStrength = [](float, const auto&) { return 1.0f; };
        response.rotating(object, row, true, true, services);
        require(object.rotationAmplitude == 0 && object.suctionTicks == 1,
            "The first suction tick uses the old zero counter before incrementing it.");
        response.rotating(object, row, true, true, services);
        require(object.rotationAmplitude == 10 && object.suctionTicks == 2,
            "Subsequent rotation uses the retained counter for its original acceleration.");
        std::cout << "Furniture strike, suction, rotation, loot persistence, repeatable attempts and occupant release verified.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
