#include "resources/dolresources.h"
#include "game/world/dooranimationresource.h"
#include "game/world/dooranimationsampler.h"
#include "game/world/dooranimationpose.h"
#include "game/world/doormodelhierarchy.h"
#include "game/world/doorstate.h"
#include "game/world/doorpresentation.h"
#include "game/models/modeldrawtraversal.h"
#include "game/models/modelvertexstream.h"
#include "game/models/modeldrawmatrices.h"
#include "game/models/modeltexturestages.h"
#include "game/startup/startupresources.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"

#include <iostream>
#include <cmath>
#include <stdexcept>
#include <limits>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    void word(std::vector<std::uint8_t>& data, unsigned offset, unsigned value) {
        for (unsigned i = 0; i < 4; ++i) data[offset + i] = static_cast<std::uint8_t>(value >> (24 - 8 * i));
    }
}

int main(int argc, char** argv) {
    try {
        dooranimation::Resource resource;
        require(doors::distanceOpacity({0, 0, 0}, {0, 0, 0}) == 100 &&
            doors::distanceOpacity({300, 0, 0}, {0, 0, 0}) == 100 &&
            doors::distanceOpacity({305, 0, 0}, {0, 0, 0}) == 99 &&
            doors::distanceOpacity({550, 0, 0}, {0, 0, 0}) == 50 &&
            doors::distanceOpacity({800, 0, 0}, {0, 0, 0}) == 0,
            "Original door distance fade retains its threshold, scale and truncation.");
        require(doors::distanceOpacity({180, 240, 0}, {0, 0, 0}) == 100 &&
            doors::distanceOpacity({std::numeric_limits<float>::infinity(), 0, 0}, {0, 0, 0}) == 0,
            "Distance calculation retains three-axis magnitude and the original overflow result.");
        require(doors::advanceOpacity(250, false, 0) == 255 && doors::advanceOpacity(251, false, 0) == 255 &&
            doors::advanceOpacity(255, true, 100) == 250 && doors::advanceOpacity(98, true, 100) == 100 &&
            doors::advanceOpacity(103, true, 100) == 100 && doors::advanceOpacity(0, true, 100) == 5,
            "Five-unit fade steps approach their targets without overshoot and saturate at 255.");
        const dooranimation::Matrix parent{2, 0, 0, 10, 0, 3, 0, 20, 0, 0, 4, 30};
        const dooranimation::Matrix child{1, 0, 0, 5, 0, 1, 0, 6, 0, 0, 1, 7};
        require(dooranimation::concatenate(parent, child) ==
            dooranimation::Matrix{2, 0, 0, 20, 0, 3, 0, 38, 0, 0, 4, 58},
            "Original concatenation applies parent scale to local translation before parent translation.");
        std::string error;
        doormodel::Hierarchy hierarchy;
        std::vector<std::uint8_t> model(0x40 + 5 * 0x8c);
        word(model, 0x3c, 0x40);
        for (unsigned i = 0; i < 5; ++i) word(model, 0x40 + i * 0x8c + 2, 0xffffffff);
        word(model, 0x42, 0x00010002);
        word(model, 0x40 + 2 * 0x8c + 2, 0x0003ffff);
        require(hierarchy.read(model, error), error.c_str());
        const auto& visits = hierarchy.visits();
        require(visits.size() == 4 && visits[0].index == 0 && visits[1].index == 2 &&
            visits[1].parent == -1 && visits[2].index == 3 && visits[2].parent == 2 &&
            visits[3].index == 1 && visits[3].parent == 0,
            "Linked traversal visits sibling subtrees before children and ignores unreachable records.");
        word(model, 0x42, 0x00010000);
        require(!hierarchy.read(model, error) && hierarchy.visits().size() == 4,
            "Cyclic model data is rejected without replacing the loaded hierarchy.");
        require(!resource.loaded(), "New resource has no attachment.");
        std::vector<std::uint8_t> data(32);
        data[0] = 3; data[1] = 7;
        word(data, 4, 0x12345678);
        word(data, 8, 32); word(data, 12, 24); word(data, 16, 24); word(data, 20, 0);
        require(resource.read(data, error), error.c_str());
        require(resource.loaded() && resource.headerByte(0) == 3 && resource.headerByte(1) == 7 &&
            resource.headerValue() == 0x12345678 && resource.sectionOffset(0) == 32 && resource.sectionOffset(3) == 0,
            "Attachment preserves header values and offsets without inferred counts or sorting.");
        data[0] = 9;
        require(resource.bytes()[0] == 3, "Native resource owns its attached bytes.");
        word(data, 8, 33);
        require(!resource.read(data, error) && resource.headerByte(0) == 3,
            "Invalid section does not replace a valid attachment.");
        require(!resource.read(std::vector<std::uint8_t>(23), error), "Truncated header is rejected.");
        const dooranimation::Key start{0, 0, 100, 4}, end{60, 10, -4, 200};
        require(dooranimation::interpolate(0, start, end) == 0 && dooranimation::interpolate(60, start, end) == 10 &&
            dooranimation::interpolate(30, start, end) == 6, "Interpolation uses outgoing/incoming tangents and original time scaling.");
        data.assign(80, 0);
        word(data, 8, 32); word(data, 12, 32); word(data, 16, 32); word(data, 20, 24);
        data[25] = 2;
        word(data, 32, 0); word(data, 36, 0); word(data, 40, 0x40800000);
        word(data, 44, 0x42700000); word(data, 48, 0x41200000); word(data, 52, 0xc0800000);
        require(resource.read(data, error), error.c_str());
        require(dooranimation::sampleChannel(resource, 0, 0, 30) == 6, "Three-float keys share their tangent.");
        bool unsupported = false;
        try { dooranimation::sampleChannel(resource, 0, 0, 60); } catch (const std::runtime_error&) { unsupported = true; }
        require(unsupported, "Unresolved endpoint state is not silently clamped.");
        data[29] = 1;
        word(data, 40, 0x42c80000); word(data, 44, 0x40800000);
        word(data, 48, 0x42700000); word(data, 52, 0x41200000); word(data, 56, 0xc0800000); word(data, 60, 0x43480000);
        require(resource.read(data, error) && dooranimation::sampleChannel(resource, 0, 0, 30) == 6,
            "Four-float keys use separate incoming and outgoing tangents.");
        data[25] = 1;
        word(data, 32, 0x40e00000);
        require(resource.read(data, error) && dooranimation::sampleChannel(resource, 0, 0, -100) == 7,
            "Single-value channels bypass interpolation and frame checks.");
        word(data, 4, 60);
        require(resource.read(data, error), error.c_str());
        require(dooranimation::clampFrame(resource, 59) == 59 && dooranimation::clampFrame(resource, 60) == 59 &&
            dooranimation::clampFrame(resource, -2) == -2, "Original non-looping caller clamps only above frame count minus one.");
        float frame = 1000;
        dooranimation::updateFrame(resource, frame);
        require(frame == 59, "Non-looping mutable caller retains the last allowed frame.");
        data[1] = 2;
        require(resource.read(data, error), error.c_str());
        frame = 59;
        dooranimation::updateFrame(resource, frame);
        require(frame == 59, "Loop reset comparison is strict.");
        frame = 1000;
        dooranimation::updateFrame(resource, frame);
        require(frame == 0 && dooranimation::clampFrame(resource, 1000) == 59,
            "Looping mutable caller discards overshoot; the other caller always clamps regardless of loop attribute.");
        word(data, 4, 0);
        require(resource.read(data, error) && dooranimation::clampFrame(resource, 0) == -1,
            "Frame count subtraction retains original wrapping signed conversion.");
        data.assign(80, 0);
        word(data, 8, 32); word(data, 12, 32); word(data, 16, 32); word(data, 20, 24);
        data[25] = 2; data[27] = 3;
        word(data, 32, 0xc2700000); word(data, 36, 0xc1200000);
        word(data, 56, 0x42700000); word(data, 60, 0x41200000);
        require(resource.read(data, error) && dooranimation::sampleChannel(resource, 0, 0, -30) == -5,
            "Before-first-key sampling reads the preceding resource bytes with signed key addressing.");
        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> compressed, archive, executable;
            require(package.open(argv[1], error) && package.read("files/Game/game_usa.szp", compressed, error) &&
                roomarchive::decompress(compressed, archive, error), error.c_str());
            dooranimation::PoseTables tables;
            require(package.read("sys/main.dol", executable, error) && tables.read(executable, error), error.c_str());
            const dooranimation::Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
            auto doorResources = std::make_shared<doors::Resources>();
            require(doorResources->load(archive, executable, error), error.c_str());
            const auto zeroAngles = doorResources->floatTrig().sinCos(0);
            require(zeroAngles[0] == 0 && zeroAngles[1] == 1, "Original initialized trigonometric constants preserve zero-angle results.");
            for (int i = -400; i <= 400; ++i) {
                const float angle = i * 0.03125f;
                const auto angles = doorResources->floatTrig().sinCos(angle);
                require(std::fabs(angles[0] - std::sin(angle)) < 0.000002f && std::fabs(angles[1] - std::cos(angle)) < 0.000002f,
                    "Recovered trig range reduction and polynomial remain consistent across positive and negative quadrants.");
            }
            const dooranimation::Matrix textureBasis{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
            modelmaterial::TextureTransform textureTransform;
            textureTransform.flags = 3;
            textureTransform.values = {2, 3, 0, 4, 5, 0.5f, 0.25f};
            require(modelmaterial::evaluateTransform(textureTransform, textureBasis, doorResources->floatTrig()) ==
                dooranimation::Matrix{2, 0, 0, 3.5f, 0, 3, 0, 4.5f, 0, 0, 1, 0},
                "Generated texture scaling preserves original pivot and translation order.");
            std::size_t decodedBatches = 0, decodedVertices = 0;
            for (unsigned modelIndex = 0; modelIndex < 15; ++modelIndex) {
                const auto& modelResource = doorResources->model(modelIndex);
                for (const auto& entry : modelResource.drawing.nodes()) {
                    for (const auto& binding : entry.second.bindings) {
                        std::vector<modeldraw::Primitive> primitives;
                        require(modeldraw::decodeVertices(modelResource.bytes, modelResource.drawing.batch(binding.batch), primitives, error), error.c_str());
                        ++decodedBatches;
                        for (const auto& primitive : primitives) decodedVertices += primitive.vertices.size();
                    }
                }
                for (unsigned pass = 0; pass < 3; ++pass) {
                    modeldraw::TraversalState drawState;
                    modeldraw::traverse(modelResource.drawing, pass, 255, drawState, [&](const modeldraw::DrawCommand& command) {
                        require(static_cast<unsigned>(command.node) < modelResource.initialMatrices.size(),
                            "Owned draw node resolves its original initial pose matrix.");
                        require(command.opacity != 255 || modelResource.drawing.materialPass(command.material) == pass,
                            "Opaque owned batches select their original material pass.");
                    });
                }
            }
            std::cout << "Decoded " << decodedBatches << " door batch bindings and " << decodedVertices << " indexed vertices.\n";
            doors::PoseOwner poseOwner;
            poseOwner.update(0, identity, 0);
            require(poseOwner.flags() == 0, "Unbound controller leaves initialization pending.");
            poseOwner.bind(doorResources);
            poseOwner.selectModel(1);
            poseOwner.update(0, identity, 0);
            const auto* matrixAddress = &poseOwner.matrix(0);
            require(poseOwner.previousFrame() == 0 && poseOwner.frameDelta() == 1.234f,
                "First controller delta uses the original -1.234 sentinel.");
            poseOwner.update(1000, identity, 0);
            require(poseOwner.previousFrame() == 62 && &poseOwner.matrix(0) == matrixAddress,
                "Controller clamps sampled frame and retains matrix storage across updates.");
            poseOwner.update(1, identity, 0);
            require(poseOwner.frameDelta() == -61, "Door controller retains negative deltas on reversal.");
            float loopFrame = 0;
            poseOwner.updateLooping(loopFrame, identity, 0);
            require(poseOwner.frameDelta() == 1, "Mutable-frame caller replaces negative deltas with one.");
            const auto dolWord = [&](std::uint32_t address) {
                std::vector<std::uint8_t> bytes;
                require(dolresources::read(executable, address, 4, bytes, error), error.c_str());
                return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
            };
            doordata::Table descriptors;
            require(doordata::read(executable, dolWord(dolWord(0x802ff218 + 2 * 4) + 0x18), descriptors, error), error.c_str());
            doors::SavedFlags saved{};
            doors::State doorState;
            require(doorState.initialize(descriptors, doorResources, executable, saved, error), error.c_str());
            require(doorState.size() == descriptors.liveCount && doorState.size() != 0,
                "Map2 door descriptors initialize all allocated leaf controllers.");
            std::array<std::uint32_t, 256> interactionRooms;
            for (unsigned room = 0; room < interactionRooms.size(); ++room) interactionRooms[room] = (room << 24) | 0x1234;
            for (unsigned index = 0; index < doorState.size(); ++index) {
                const auto& leaf = doorState.leaf(index);
                std::uint32_t destination = 0;
                const auto firstRoom = interactionRooms[leaf.rooms[0]], secondRoom = interactionRooms[leaf.rooms[1]];
                require(doorState.entrySide(index, firstRoom) == 1 &&
                    doorState.destinationRoom(index, firstRoom, interactionRooms, destination) && destination == secondRoom,
                    "Door interaction uses the original source-side room byte and destination identifier.");
                if (leaf.rooms[0] != leaf.rooms[1]) require(doorState.entrySide(index, secondRoom) == 0 &&
                    !doorState.destinationRoom(index, secondRoom, interactionRooms, destination) && destination == firstRoom,
                    "Reverse entry returns the opposite room without swapping the retained leaf.");
                require(doorState.pairedEntry(index, 40, true) == -1 && doorState.pairedEntry(index, firstRoom, false) == -1,
                    "Disabled or unrelated entries do not select the special paired door.");
                if (leaf.savedId == 40) require(doorState.pairedEntry(index, 40, false) == doorState.savedPair(44)[0],
                    "The special paired entry uses the low room byte and original saved-door table.");
            }
            std::vector<unsigned> sounds;
            const auto emit = [&](unsigned id, const std::array<float, 3>&) { sounds.push_back(id); };
            for (int direction = 0; direction < 2; ++direction) {
                sounds.clear();
                doorState.action(0, direction, 1, true);
                const unsigned duration = doorResources->animation(direction ? 0 : 1).headerValue();
                for (unsigned tick = 0; tick <= duration; ++tick) {
                    doorState.advanceAnimation(0, emit);
                    doorState.refreshPose(0);
                }
                require((doorState.leaf(0).flags & 0xa28) == 8 && doorState.leaf(0).frame == 0,
                    "Full door cycle clears active/direction flags and resets the source frame.");
                require(sounds == std::vector<unsigned>{0x1809, direction ? 0x180bU : 0x1808U, 0x180a},
                    "Full door cycle emits original sound IDs in order.");
                sounds.clear();
                doorState.action(0, direction, 3, true);
                const unsigned holdDuration = doorResources->animation(direction ? 2 : 4).headerValue();
                for (unsigned tick = 0; tick <= holdDuration; ++tick) {
                    doorState.advanceAnimation(0, emit);
                    doorState.refreshPose(0);
                }
                require((doorState.leaf(0).flags & 0x258) == 0x210 && sounds == std::vector<unsigned>{0x181e},
                    "Open-and-hold keeps the original active flag and hold state.");
                const auto before = doorState.leaf(0).flags;
                require(doorState.action(0, direction, 4, false) == before,
                    "Non-forced action cannot replace an active controller.");
                doorState.action(0, direction, 4, true);
                sounds.clear();
                const unsigned closeDuration = doorResources->animation(direction ? 3 : 5).headerValue();
                for (unsigned tick = 0; tick <= closeDuration; ++tick) {
                    doorState.advanceAnimation(0, emit);
                    doorState.refreshPose(0);
                }
                require((doorState.leaf(0).flags & 0x1208) == 8 && sounds == std::vector<unsigned>{0x180a},
                    "Forward close returns the leaf to closed and emits its latch sound.");
            }
            const auto savedId = doorState.leaf(0).savedId;
            doorState.setSavedFlag(savedId, true, saved);
            require((saved[savedId >> 5] & (0x80000000U >> (savedId & 31))) != 0,
                "Door save flags retain their original most-significant-bit-first ordering.");
            for (auto index : doorState.savedPair(savedId))
                if (index != 0xff) require((doorState.leaf(index).flags & 0x4400) == 0x4400,
                    "Saved door state applies to both mapped leaves.");
            std::cout << "Map2 initialized " << doorState.size() << " door leaves; controller/action cycles verified.\n";
            doorState.action(0, 0, 3, true);
            for (unsigned tick = 0; tick <= doorResources->animation(4).headerValue(); ++tick) doorState.advanceAnimation(0, emit);
            doorState.action(0, 0, 2, true);
            sounds.clear();
            for (unsigned tick = 0; tick <= doorResources->animation(4).headerValue(); ++tick) {
                doorState.advanceAnimation(0, emit);
                doorState.refreshPose(0);
            }
            require(doorState.leaf(0).frame == -1 && (doorState.leaf(0).flags & 0x288) == 8 &&
                sounds == std::vector<unsigned>{0x180a}, "Reverse closing preserves the original negative terminal frame.");
            require(tables.local({1, 1, 1, std::numeric_limits<float>::quiet_NaN(), 0, 0, 0, 0, 0}) == identity,
                "Original invalid angle conversion selects lookup index zero.");
            for (unsigned leafIndex = 0; leafIndex < doorState.size(); ++leafIndex) {
                const auto& leaf = doorState.leaf(leafIndex);
                require(leaf.collision.has_value() == (leaf.type != 0) && leaf.collisionActive == 1,
                    "Original collision registration skips type zero and initializes activation bytes to one.");
                if (leaf.collision) {
                    for (float component : leaf.collision->center) require(std::isfinite(component), "Collision center is finite.");
                    require(leaf.collision->axes[0][0] == leaf.transform[2] &&
                        leaf.collision->axes[2][2] == leaf.transform[8], "Collision axes preserve original Z/Y/X storage order.");
                }
                for (int direction = 0; direction < 2; ++direction) {
                    std::vector<std::pair<int, unsigned>> events;
                    const auto record = [&](unsigned id, const std::array<float, 3>& position) {
                        require(position == leaf.position, "Sound retains its owning door position.");
                        events.emplace_back(static_cast<int>(leaf.frame), id);
                    };
                    doorState.action(leafIndex, direction, 1, true);
                    const auto end = doorResources->animation(direction ? 0 : 1).headerValue();
                    for (unsigned tick = 0; tick <= end; ++tick) {
                        doorState.advanceAnimation(leafIndex, record);
                        doorState.refreshPose(leafIndex);
                    }
                    require(events == std::vector<std::pair<int, unsigned>>{{12, 0x1809},
                        {direction ? 35 : 25, direction ? 0x180bU : 0x1808U}, {static_cast<int>(end) - 2, 0x180a}},
                        "Every owned door retains exact forward-cycle sound frame ordering.");
                    doorState.action(leafIndex, direction, 2, true);
                    const auto reverseEnd = doorResources->animation(direction ? 2 : 4).headerValue();
                    for (unsigned tick = 0; tick <= reverseEnd; ++tick) {
                        doorState.advanceAnimation(leafIndex, record);
                        doorState.refreshPose(leafIndex);
                    }
                    require(leaf.frame == -1 && (leaf.flags & 0x288) == 8,
                        "Every owned door completes both reverse paths without replacing the negative frame.");
                }
            }
            require(doorState.initialize(descriptors, doorResources, executable, saved, error), error.c_str());
            std::array<std::uint32_t, 256> roomIds{};
            doors::RoomVisit roomVisit;
            roomVisit.marker = 7;
            for (const auto& descriptor : descriptors.entries) roomVisit.doorIds.push_back(descriptor.identity);
            doorState.updateVisibility(2, {roomVisit}, roomIds, 0);
            unsigned queries = 0, influenceSounds = 0;
            int playerInfluence = 0;
            std::vector<unsigned> globalSounds;
            doors::UpdateServices services;
            services.positionalSound = emit;
            services.globalSound = [&](unsigned id) { globalSounds.push_back(id); };
            services.playerInfluence = [&](float radius, const std::array<float, 3>&) {
                require(radius == 200, "Door shake queries the original influence radius.");
                ++queries;
                return playerInfluence;
            };
            services.influenceSound = [&](float magnitude, std::uint8_t kind, const std::array<float, 3>&) {
                require(magnitude == 10 && kind == 2, "Influence audio receives original scaled strength and category.");
                ++influenceSounds;
            };
            gameflags::State runtimeFlags;
            doorState.action(0, 1, 1, true);
            doorState.update(1, 0, runtimeFlags, saved, services);
            require(doorState.leaf(0).frame == 1 && doorState.activeDoor() == 0 && queries != 0,
                "Visible doors advance through the connected update and retain the active-door index.");
            const unsigned visibleQueries = queries;
            playerInfluence = 2;
            doorState.update(1, 4, runtimeFlags, saved, services);
            require(influenceSounds == visibleQueries && queries == visibleQueries * 2,
                "Each visible nonzero-type door runs its world query and periodic influence sound.");
            doorState.updateVisibility(2, {}, roomIds, 0);
            const auto pausedFrame = doorState.leaf(0).frame;
            doorState.update(1, 5, runtimeFlags, saved, services);
            require(queries == visibleQueries * 2 && doorState.leaf(0).frame == pausedFrame && doorState.activeDoor() == -1 &&
                doorState.leaf(0).references[2] == 7, "Invisible doors pause; visibility clearing retains old marker bytes.");
            const auto usedPair = doorState.savedPair(0x22);
            require(usedPair[0] != 0xff, "Owned map contains the original door-triggered flag dependency.");
            std::uint8_t usedId = 0, usedType = 0;
            doorState.markUsed(usedPair[0], usedId, usedType);
            doorState.update(2, 6, runtimeFlags, saved, services);
            doorState.update(2, 7, runtimeFlags, saved, services);
            require(runtimeFlags.value(3) && globalSounds == std::vector<unsigned>{0x4872},
                "Map2 used-door event sets flag three and emits its global sound once, even outside visibility.");
            doorState.setCollisionActive(savedId, false);
            doorState.setEventBlocked(savedId, true);
            for (auto index : doorState.savedPair(savedId)) if (index != 0xff)
                require(doorState.leaf(index).collisionActive == 0 && (doorState.leaf(index).flags & 0x10000),
                    "Event collision and blocked flags reach both saved-ID leaves.");
            unsigned barrierMessages = 0;
            doorState.setGlobalBarrier(true, [&](unsigned actor, unsigned message, unsigned field) {
                require(actor == 0 && message == 0x12 && field == 1, "Barrier message retains original actor, message and changed field.");
                ++barrierMessages;
            });
            require(barrierMessages == 1 && doorState.eventBlocked(0xff) == 1 && doorState.barrierValue() == 0xff,
                "Global barrier takes precedence over the missing-door sentinel.");
            doorState.updateVisibility(2, {roomVisit}, roomIds, 0);
            playerInfluence = 0;
            sounds.clear();
            doorState.update(1, 8, runtimeFlags, saved, services);
            require(sounds == std::vector<unsigned>{0x187e}, "Shared five-step shake emits its first-door sound once.");
            doorState.resetGlobalBarrier();
            require(!doorState.globalBarrier() && doorState.barrierValue() == 0 && doorState.eventBlocked(0xff) == 0,
                "Barrier reset restores the missing-door query and does not dispatch another actor message.");
            std::cout << "All 57 leaf forward/reverse cycles, exact sound frames, visibility, world queries and map events verified.\n";
            require(doorState.initialize(descriptors, doorResources, executable, saved, error), error.c_str());
            struct Submission {
                const doors::ModelResource* model;
                const doors::PoseOwner* pose;
                unsigned pass;
                std::uint8_t opacity;
                bool lowOpacity;
            };
            std::vector<Submission> submissions;
            const auto submit = [&](const doors::ModelResource& resource, const doors::PoseOwner& pose,
                unsigned pass, std::uint8_t opacity, bool lowOpacity) {
                submissions.push_back({&resource, &pose, pass, opacity, lowOpacity});
                modeldraw::TraversalState drawState;
                drawState.lightingFlags = lowOpacity ? 1 : 0;
                std::int16_t prepared = -1;
                modeldraw::traverse(resource.drawing, pass, opacity, drawState,
                    [&](const modeldraw::DrawCommand& command) {
                        require(prepared == command.node, "Door batch uses the node matrices prepared immediately before its material draws.");
                        require(resource.geometry.count(command.batch) != 0, "Door draw consumes geometry cached by its resource owner.");
                        const auto& material = resource.materials.at(command.material);
                        const auto& batch = resource.drawing.batch(command.batch);
                        const auto plan = modelmaterial::texturePlan(material, batch.textureCount, batch.normalMode);
                        for (const auto& step : plan.steps)
                            require(material.samplers.at(step.slot).present, "Owned door texture steps resolve loaded samplers.");
                        modelmaterial::LightingState lighting;
                        lighting.lightingFlags = command.lightingFlags;
                        const bool specular = modelmaterial::configureChannels(material, command.opacity, batch.lighting, lighting,
                            [](float shininess) { require(std::isfinite(shininess), "Owned specular light input is finite."); });
                        const auto stages = modelmaterial::buildStages(material, batch.textureCount, batch.normalMode, specular, false);
                        for (const auto& texture : stages.textures) {
                            const auto& images = resource.textureImages.at(command.material).at(texture.slot);
                            require(!images.levels.empty() && images.levels[0].width == material.samplers[texture.slot].width,
                                "Owned material texture bindings resolve decoded images loaded by the resource owner.");
                        }
                        require(stages.stageCount > 0 && stages.stageCount <= 16 && stages.coordinateCount <= 8,
                            "Owned door materials produce complete stage descriptions within original hardware capacity.");
                        for (const auto& coordinate : stages.coordinates) {
                            if (coordinate.generatedMatrix < 0) continue;
                            const auto basis = modeldraw::textureMatrix(pose.matrix(command.node));
                            const auto matrix = modelmaterial::evaluateTransform(resource.textureTransforms.at(coordinate.generatedMatrix),
                                basis, doorResources->floatTrig());
                            for (float value : matrix) require(std::isfinite(value), "Owned generated texture matrix is finite.");
                        }
                    }, 0, false, [&](std::int16_t node) {
                        prepared = node;
                        const auto& world = pose.matrix(node);
                        dooranimation::Matrix normal;
                        require(modeldraw::normalMatrix(world, normal), "Owned door draw pose has a nonsingular normal matrix.");
                    });
            };
            roomVisit.marker = 255;
            doorState.updatePresentation(1, {roomVisit}, roomIds, 0, {0, 0, 0});
            doorState.submitOpaque(1, false, submit);
            require(submissions.size() == doorState.size() * 2, "Opaque doors submit both original model passes.");
            for (unsigned index = 0; index < doorState.size(); ++index) {
                const auto& first = submissions[index * 2];
                const auto& second = submissions[index * 2 + 1];
                require(first.pass == 0 && second.pass == 1 && first.opacity == 255 && !first.lowOpacity &&
                    first.pose == &doorState.leaf(index).pose && second.pose == first.pose && second.model == first.model,
                    "Opaque submission preserves leaf order, pass order, matrices and opacity.");
            }
            submissions.clear();
            doorState.submitTranslucent(1, false, submit);
            require(submissions.empty(), "Fully opaque leaves are excluded from the translucent pass.");
            roomVisit.marker = 100;
            doorState.updateVisibility(1, {roomVisit}, roomIds, 0x100);
            doorState.updatePresentation(1, {roomVisit}, roomIds, 0x100, {0, 0, 0});
            bool distanceFaded = false;
            for (unsigned index = 0; index < doorState.size(); ++index)
                if (doorState.leaf(index).flags & 2) {
                    require(doorState.leaf(index).references[1] == 250, "Distance-controlled opacity advances by five on the selected buffer.");
                    require(doorState.leaf(index).references[0] == 255, "Other presentation buffers retain their markers.");
                    distanceFaded = true;
                }
            require(distanceFaded, "Owned door fixture reaches the distance-controlled branch.");
            roomVisit.marker = 7;
            doorState.updatePresentation(1, {roomVisit}, roomIds, 0, {0, 0, 0});
            doorState.submitTranslucent(1, false, submit);
            require(submissions.size() == doorState.size(), "Every marked leaf submits one translucent pass.");
            for (const auto& command : submissions)
                require(command.pass == 2 && command.opacity == 12 && command.lowOpacity,
                    "Marker assignment precedes the five-unit fade and the low-opacity threshold.");
            doorState.setGlobalBarrier(true, [](unsigned, unsigned, unsigned) {});
            submissions.clear();
            doorState.submitTranslucent(1, false, submit);
            require(submissions.size() == doorState.size() * 2, "Barrier adds one model-zero draw for each translucent leaf.");
            for (unsigned index = 0; index < doorState.size(); ++index) {
                const auto& base = submissions[index * 2];
                const auto& barrier = submissions[index * 2 + 1];
                require(barrier.model == &doorResources->model(0) && barrier.pose == base.pose &&
                    barrier.opacity == base.opacity && barrier.lowOpacity == base.lowOpacity,
                    "Barrier model uses the original door matrices and opacity rather than a new pose.");
            }
            submissions.clear();
            doorState.submitTranslucent(1, true, submit);
            require(submissions.empty(), "Original override sends zero opacity, which suppresses translucent model drawing.");
            doorState.submitOpaque(1, true, submit);
            require(submissions.size() == doorState.size() * 4, "Override draws base and barrier with both opaque passes.");
            roomVisit.marker = 0;
            doorState.updateVisibility(1, {roomVisit}, roomIds, 0);
            submissions.clear();
            doorState.submitTranslucent(1, false, submit);
            require(submissions.empty(), "Zero opacity bypasses original translucent model calls, including barriers.");
            doorState.updatePresentation(1, {}, roomIds, 0, {0, 0, 0});
            doorState.submitOpaque(1, true, submit);
            doorState.submitTranslucent(1, false, submit);
            require(submissions.empty() && doorState.leaf(0).references[1] == 5,
                "Invisible doors continue opacity stepping but produce no model submissions.");
            roomVisit.marker = 100;
            for (unsigned step = 0; step < 20; ++step)
                doorState.updatePresentation(1, {roomVisit}, roomIds, 0, {0, 0, 0});
            doorState.submitTranslucent(1, false, submit);
            require(!submissions.empty(), "Opacity threshold fixture submits visible doors.");
            for (const auto& command : submissions)
                require(command.opacity == 105 && command.lowOpacity, "Opacity below 110 selects the original low-opacity state.");
            submissions.clear();
            doorState.updatePresentation(1, {roomVisit}, roomIds, 0, {0, 0, 0});
            doorState.submitTranslucent(1, false, submit);
            for (const auto& command : submissions)
                require(command.opacity == 110 && !command.lowOpacity, "Opacity exactly 110 clears the original low-opacity state.");
            std::cout << "Door distance fading, buffer retention, opaque/translucent ordering and barrier submissions verified.\n";
            word(model, 0x42, 0x00010002);
            for (unsigned i = 0; i < 5; ++i) {
                for (unsigned axis = 0; axis < 3; ++axis) word(model, 0x40 + i * 0x8c + 0xc + axis * 4, 0x3f800000);
                word(model, 0x40 + i * 0x8c + 0x24, 0x3f800000);
            }
            require(hierarchy.read(model, error), error.c_str());
            const auto staticMatrices = hierarchy.staticPose(tables, parent);
            require(staticMatrices.size() == 4 && staticMatrices[0][3] == 12 && staticMatrices[2][3] == 12 &&
                staticMatrices[1][3] == 14 && staticMatrices[3][3] == 14 && staticMatrices[3][5] == 3,
                "Static poses inherit parent scale and translation; siblings retain their common parent.");
            word(model, 0x42, 0x00010000);
            require(!hierarchy.read(model, error) && hierarchy.staticPose(tables, parent) == staticMatrices,
                "Failed attachment preserves static transform data as well as topology.");
            const auto staticQuarterTurn = tables.staticLocal({1, 1, 1, 0, 0, 90, 0, 0, 0});
            require(staticQuarterTurn[0] == -0.000008f && staticQuarterTurn[1] == -1 &&
                staticQuarterTurn[4] == 1 && staticQuarterTurn[5] == -0.000008f,
                "Static angle conversion retains the original quarter-turn table residue.");
            std::vector<doormodel::Hierarchy> doorHierarchies;
            for (unsigned i = 0; i < 15; ++i) {
                const std::string name = i == 0 ? "saku" : "door_" + std::string(i < 10 ? "0" : "") + std::to_string(i);
                require(roomarchive::read(archive, "iwamoto/door/" + name + ".bin", model, error) &&
                    hierarchy.read(model, error), error.c_str());
                require(!hierarchy.visits().empty(), "Owned door model has a linked root.");
                for (const auto& visit : hierarchy.visits())
                    require(static_cast<std::size_t>(visit.index) < hierarchy.visits().size(),
                        "Owned node indices fit the original allocation by visited count.");
                for (const auto& pose : hierarchy.staticPose(tables, identity))
                    for (float component : pose)
                        require(std::isfinite(component), "Owned static model poses remain finite.");
                doorHierarchies.push_back(hierarchy);
            }
            require(tables.local({2, 3, 4, 0, 0, 0, 10, 20, 30}) ==
                dooranimation::Matrix{2, 0, 0, 10, 0, 3, 0, 20, 0, 0, 4, 30},
                "Owned angle table gives original zero-angle scale/translation pose.");
            const auto quarterTurn = tables.local({1, 1, 1, 0, 0, 90, 0, 0, 0});
            require(quarterTurn[0] == -0.000008f && quarterTurn[1] == -1 &&
                quarterTurn[4] == 1 && quarterTurn[5] == -0.000008f,
                "Original table indices and rotation orientation produce the quarter-turn pose.");
            const char* names[]{"pull", "pull_h1", "pull_h2", "push", "push_h1", "push_h2"};
            const unsigned values[]{63, 36, 60, 44, 36, 60};
            const unsigned ends[]{332, 296, 320, 308, 284, 320};
            for (unsigned i = 0; i < 6; ++i) {
                require(roomarchive::read(archive, std::string("iwamoto/door/") + names[i] + ".anm", data, error) &&
                    resource.read(data, error), error.c_str());
                require(resource.headerByte(0) == 1 && resource.headerByte(1) == 0 && resource.headerValue() == values[i] &&
                    resource.sectionOffset(0) == 216 && resource.sectionOffset(1) == 220 &&
                    resource.sectionOffset(2) == ends[i] && resource.sectionOffset(3) == 24,
                    "Owned door animation headers match independent binary inspection.");
                for (unsigned frameIndex = 0; frameIndex < values[i] * 2; ++frameIndex) {
                    const float sampleFrame = static_cast<float>(dooranimation::clampFrame(resource, frameIndex * 0.5));
                    for (unsigned channel = 0; channel < 9; ++channel)
                        require(std::isfinite(dooranimation::sampleChannel(resource, 0, channel, sampleFrame)),
                            "Owned root channels sample at original and interpolated presentation frames through the caller limit.");
                    for (float component : dooranimation::samplePose(resource, tables, 0, sampleFrame, identity))
                        require(std::isfinite(component), "Owned root pose matrices remain finite across animation frames.");
                    for (const auto& doorHierarchy : doorHierarchies)
                        for (const auto& pose : doorHierarchy.sample(resource, tables, sampleFrame, identity))
                            for (float component : pose)
                                require(std::isfinite(component), "Owned linked door poses remain finite.");
                }
            }
            std::cout << "All six owned door animation resources attached and verified.\n";
        }
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
