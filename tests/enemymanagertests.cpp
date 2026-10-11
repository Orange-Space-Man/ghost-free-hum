#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/enemies/enemymanager.h"
#include "game/enemies/enemybarrierstrategy.h"
#include "game/enemies/enemybatgeneratorstrategy.h"
#include "game/enemies/enemyfiregeneratorstrategy.h"
#include "game/enemies/enemygoldratgeneratorstrategy.h"
#include "game/enemies/enemyroomobserverstrategy.h"
#include "game/enemies/enemyfakedoorstrategy.h"
#include "game/scene/gameplaymanagerservices.h"
#include "game/effects/particlescene.h"
#include "game/actors/characterplacementruntime.h"
#include "game/player/playereventdispatch.h"
#include "game/world/doorstate.h"
#include "game/enemies/enemymanagerruntime.h"
#include "game/actors/characterowner.h"
#include "game/actors/actormodelarchive.h"
#include "game/actors/actormodelinstance.h"
#include "game/actors/actormodelcatalog.h"
#include "game/actors/actormodelresources.h"
#include "game/player/playerattachedstate.h"
#include "game/player/playerattachedframe.h"
#include "game/actors/actorposeupdate.h"
#include "game/actors/actorposeblend.h"
#include "game/player/playeractionstate.h"
#include "game/player/playeractionframe.h"
#include "game/player/playerenvironmentframe.h"
#include "game/player/playercontrolframe.h"
#include "game/player/playerreactionframe.h"
#include "game/player/playertimerframe.h"
#include "game/player/playerequipmentframe.h"
#include "game/player/playercontactreaction.h"
#include "game/player/playercontrolstate.h"
#include "game/actors/actorinitialpose.h"
#include "game/actors/moveactor.h"
#include "game/actors/actorcollisionowner.h"
#include "game/player/playerorientation.h"
#include "game/player/playerrecordwriteback.h"
#include "game/player/playeractorcomponents.h"
#include "game/actors/actorregistry.h"
#include "game/actors/characteractorlinks.h"
#include "game/effects/particleparameters.h"
#include "game/effects/particleemitterstate.h"
#include "game/effects/particleeffectfactory.h"
#include "game/effects/modeleffectpool.h"
#include "game/effects/sharedmodeleffect.h"
#include "game/effects/waterfalleffectpool.h"
#include "game/player/playericeeffectpool.h"
#include "game/effects/barriereffectpool.h"
#include "game/world/cobwebplacementmanager.h"
#include "game/effects/particlesuction.h"
#include "game/effects/particleforces.h"
#include "game/effects/particleadvancement.h"
#include "game/effects/particlesimulation.h"
#include "game/effects/crystaleffectpool.h"
#include "game/effects/rivereffectpool.h"
#include "game/effects/modeleffectmanager.h"
#include "game/effects/particlescenecontext.h"
#include "game/actors/actorcapturemotion.h"
#include "game/enemies/itemspawnadmission.h"
#include "game/enemies/enemyitemappearance.h"
#include "game/enemies/itemphysicsresponse.h"
#include "game/enemies/enemyitemphysics.h"
#include "game/enemies/enemyitemstrategy.h"
#include "game/enemies/enemykeystrategy.h"
#include "game/enemies/enemyheartstrategy.h"
#include "game/enemies/enemynegativestrategy.h"
#include "game/enemies/enemyjewelstrategy.h"
#include "game/enemies/enemygoldstrategy.h"
#include "game/enemies/enemypearlstrategy.h"
#include "game/enemies/enemycrownstrategy.h"
#include "game/enemies/enemyelementjewelstrategy.h"
#include "game/enemies/enemyheartescapestrategy.h"
#include "game/enemies/enemynegativerandomstrategy.h"
#include "game/enemies/enemybombstrategy.h"
#include "game/enemies/itemghostloot.h"
#include "game/actors/actortimingsampler.h"
#include "game/player/playerbombdamage.h"
#include "game/scene/gameplaycameraowner.h"
#include "game/audio/jaudiopositionowners.h"
#include "game/player/playershrinkevent.h"
#include "game/player/playerliferecovery.h"
#include "game/player/playerkeycollection.h"
#include "game/enemies/itemcreationroute.h"
#include "game/player/playercaptureflags.h"
#include "game/player/playeritemcollection.h"
#include "game/player/playerattractionevent.h"
#include "game/enemies/itemsuctioncontroller.h"
#include "game/player/playersuctioncompletion.h"
#include "game/player/playervacuummodel.h"
#include "game/actors/characterownervacuum.h"
#include "game/actors/characterownerdamage.h"
#include "game/actors/characterownermotion.h"
#include "game/scene/gamearchiveresources.h"
#include "game/actors/charactereffectresources.h"
#include "game/actors/charactercreationstate.h"
#include "game/actors/charactercreationruntime.h"
#include "game/world/mapcollisionstate.h"
#include "game/actors/characteritemselection.h"
#include "game/effects/bridgeeffectpool.h"
#include "game/effects/icejaileffectpool.h"
#include "game/effects/icicleeffectpool.h"
#include "game/enemies/enemynomovestrategy.h"
#include "game/enemies/enemystrategyfactory.h"
#include "game/enemies/enemypathcondition.h"
#include "game/actors/charactergenerator.h"
#include "game/enemies/enemygeneratormanager.h"
#include "game/actors/actorpathsampler.h"
#include "game/startup/startupresources.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"

#include <cstring>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
    void require(bool value, const std::string& message) {
        if (!value) throw std::runtime_error(message);
    }

    void word(std::vector<std::uint8_t>& data, unsigned offset, unsigned value) {
        for (unsigned i = 0; i < 4; ++i) data.at(offset + i) = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }

    void checkItemReflection() {
        mission::Resources resources;
        mapcollision::State collision(resources);
        std::vector<std::uint8_t> data(104);
        word(data, 0x28, 64);
        word(data, 0x2c, 80);
        word(data, 68, 0x3f800000);
        collision.load(data);
        std::array<float, 3> normal{7, 8, 9};
        require(!collision.polygonNormal(-1, normal) && normal == std::array<float, 3>{7, 8, 9},
            "Negative polygon lookup preserves the caller's output.");
        require(collision.polygonNormal(0x10000, normal) && normal == std::array<float, 3>{0, 1, 0},
            "Original normal lookup masks nonnegative polygon indices to sixteen bits.");
        const auto response = itemphysics::reflection(collision, 0, {4, -10, 2}, 0.5f, 0.25f);
        require(response == std::array<float, 3>{-1, 15, -0.5f},
            "Item bounce and tangent friction use the original independent response terms.");
        require(itemphysics::reflection(collision, 0, {4, 0, 2}, 0.5f, 0.25f) == std::array<float, 3>{} &&
            itemphysics::reflection(collision, -1, {4, -10, 2}, 0.5f, 0.25f) == std::array<float, 3>{},
            "Tangent motion and absent polygons produce no collision impulse.");
        const auto unordered = itemphysics::reflection(collision, 0,
            {0, std::numeric_limits<float>::quiet_NaN(), 0}, 0.5f, 0.25f);
        require(std::isnan(unordered[1]), "Original unordered normal velocity enters the reflection path.");
    }

    void checkAnimationBinding() {
        struct Handle : actoranimation::SoundHandle {
            unsigned stopped = 0;
            std::uint32_t currentFlags = 0xc00;
            void stop(unsigned frames) override { stopped = frames; }
            std::uint32_t flags() const override { return currentFlags; }
        } handle;
        actoranimation::SoundState state;
        auto resource = std::make_shared<actormodel::Resource>();
        resource->bytes.resize(64);
        resource->bytes[1] = 2;
        word(resource->bytes, 12, 0x3f800000);
        word(resource->bytes, 44, 0x40400000);
        state.bind(resource, 1, 2);
        require(state.forwardIndex == 1 && state.reverseIndex == 0 && state.frame == 0,
            "Animation sound binding advances past strictly earlier records only.");
        state.bind(resource, -1, 3);
        require(state.reverseIndex == 1 && state.forwardIndex == 0 && state.field78 == -1,
            "Reverse animation sound binding retains an event at the exact start frame.");
        std::array<std::uint8_t, 20> event{};
        event[19] = 4;
        state.slots[0].sound = &handle;
        state.slots[0].event = event.data();
        state.slots[0].state = 1;
        state.bind(nullptr, 1, 0);
        require(handle.stopped == 1 && state.slots[0].state == 0 && state.direction == -1,
            "Null animation binding stops flagged handles and preserves direction fields.");
        resource->bytes[1] = 0;
        state.slots[1].state = 9;
        state.bind(resource, 1, 0);
        require(!state.resource && state.slots[1].state == 9,
            "An empty original sound resource returns before clearing sound slots.");
        resource->bytes[0] = 1;
        state.bind(resource, 1, 100);
        require(state.resource == resource && state.forwardIndex == 0,
            "The original count helper treats a nonzero count with zero low byte as zero.");

        resource = std::make_shared<actormodel::Resource>();
        resource->bytes.resize(72);
        resource->bytes[1] = 2;
        for (unsigned i = 0; i < 2; ++i) {
            const auto offset = 8 + i * 32;
            word(resource->bytes, offset, 0x800 + i);
            word(resource->bytes, offset + 4, i ? 0x40400000 : 0x3f800000);
            word(resource->bytes, offset + 8, 0x40800000);
            word(resource->bytes, offset + 12, 0x3f800000);
            resource->bytes[offset + 20] = 127;
            resource->bytes[offset + 21] = 2;
            resource->bytes[offset + 23] = 64;
            resource->bytes[offset + 24] = 3;
        }
        actoranimation::SoundState playback;
        std::vector<unsigned> started;
        float currentPitch = 0, currentVolume = 0, auxiliary = 0;
        actoranimation::SoundServices services{
            [&](unsigned id, actoranimation::SoundHandle*& sound) { started.push_back(id); sound = &handle; },
            [&](actoranimation::SoundHandle&, float value) { currentPitch = value; },
            [&](actoranimation::SoundHandle&, float value) { currentVolume = value; },
            [&](actoranimation::SoundHandle&, float value) { auxiliary = value; }
        };
        playback.bind(resource, 1, 0);
        playback.update(0.5, 2, services);
        require(started.empty(), "Animation sound events wait for their original frame.");
        playback.update(1, 2, services);
        require(started == std::vector<unsigned>{0x800} && currentPitch == 1.0625f && currentVolume == 1 &&
            auxiliary == 64.0f / 127 && playback.slots[0].resource == resource,
            "Exact animation event boundaries start sounds with original speed pitch and retained resources.");
        playback.update(3, 2, services);
        require(started == std::vector<unsigned>({0x800, 0x801}) && playback.reverseIndex == 2,
            "One-shot sound IDs do not restart while traversing later animation markers.");
        playback.update(0, 2, services);
        require(playback.field78 == 1 && playback.reverseIndex == 0,
            "Forward animation wrap restarts the cursor and advances its loop count.");
        playback.update(0, 0, services);
        require(currentVolume == 121.0f / 127,
            "Retained animation sounds preserve signed speed-dependent volume adjustment.");
        playback.stop();
        require(handle.stopped == 0 && playback.slots[0].state == 1,
            "Original animation stop releases handles without clearing slot states.");

        actoranimation::SoundState reverse;
        started.clear();
        reverse.bind(resource, -1, 3);
        reverse.update(3, -1, services);
        require(started == std::vector<unsigned>{0x801} && reverse.field78 == 0 && reverse.reverseIndex == 0,
            "Reverse playback initializes its loop count and includes exact event frames.");
        reverse.update(1, -1, services);
        require(started == std::vector<unsigned>({0x801, 0x800}) && reverse.reverseIndex == 0xffffffff,
            "Reverse playback visits earlier markers and preserves the unsigned end sentinel.");
    }

    void checkPathSampling() {
        actormodel::Resource resource;
        auto& data = resource.bytes;
        data.resize(104);
        const auto half = [&](unsigned offset, unsigned value) {
            data[offset] = static_cast<std::uint8_t>(value >> 8);
            data[offset + 1] = static_cast<std::uint8_t>(value);
        };
        for (unsigned axis = 0; axis < 3; ++axis) {
            half(4 + axis * 6, 1); half(6 + axis * 6, axis); half(8 + axis * 6, 1);
            half(22 + axis * 6, 1); half(24 + axis * 6, axis); half(26 + axis * 6, 1);
        }
        word(data, 40, 48); word(data, 44, 96);
        const float values[]{0, 1, -2, 0, 0, 10, 30, 10, 10};
        for (unsigned i = 0; i < 9; ++i) {
            unsigned bits;
            std::memcpy(&bits, &values[i], 4);
            word(data, 48 + i * 4, bits);
        }
        half(96, static_cast<std::uint16_t>(-2049)); half(98, 200); half(100, 2);
        std::array<float, 3> position;
        std::array<std::int16_t, 3> rotation;
        actoranimation::samplePath(resource, 0, position, &rotation);
        require(position == std::array<float, 3>{0, 1, -2} && rotation == std::array<std::int16_t, 3>{32752, 3200, 32},
            "Original PTH constant tracks preserve signed rotation shift and low-halfword wrap.");
        half(4, 2); half(6, 3); half(8, 3);
        actoranimation::samplePath(resource, 15, position);
        require(std::abs(position[0] - 5) < 0.000001f && position[1] == 1 && position[2] == -2,
            "Original PTH Hermite tracks scale time by thirty and retain per-second tangents.");
        actoranimation::samplePath(resource, -5, position);
        require(position[0] == 0, "PTH sampling before the first key retains the first value.");
        actoranimation::samplePath(resource, 35, position);
        require(position[0] == 10, "PTH sampling after the last key retains the last value.");
        actoranimation::samplePath(resource, std::numeric_limits<double>::quiet_NaN(), position);
        require(position[0] == 10, "Original unordered path-time comparisons scan through to the last key.");
        data.resize(128);
        half(4, 2); half(6, 0); half(8, 4);
        const float splitTangents[]{0, 0, 900, 0, 30, 10, 0, -900};
        for (unsigned i = 0; i < 8; ++i) {
            unsigned bits;
            std::memcpy(&bits, &splitTangents[i], 4);
            word(data, 48 + i * 4, bits);
        }
        word(data, 44, 104);
        half(22, 2); half(24, 0); half(26, 4);
        const unsigned rotationKeys[]{0, 0, 900, 0, 30, 160, 0, 900};
        for (unsigned i = 0; i < 8; ++i) half(104 + i * 2, rotationKeys[i]);
        actoranimation::samplePath(resource, 7.5, position, &rotation);
        require(position[0] == 1.5625f && rotation[0] == 400,
            "Four-value PTH tracks select the left outgoing and right incoming tangents before integer truncation.");
    }

    void checkPlayerWriteback() {
        playerrecord::Record record;
        record.fill(0xa5);
        playerrecord::Values values{};
        values.life = 73;
        values.maximumLife = 100;
        values.flags = 12;
        values.entries[0] = 0xffff1234;
        values.entries[63] = 0x1234abcd;
        values.count = 2;
        values.fields12FC[6] = 0x12345678;
        values.fields1318[2] = 0x12345678;
        values.controlWord = 0x00e3abcd;
        require(!playerrecord::write(nullptr, values), "Player destruction without a bound record does not write save state.");
        require(playerrecord::write(&record, values), "Player teardown writes its bound record.");
        require(record[1] == 73 && record[3] == 100 && record[0x16a] == 3 &&
            record[0x32] == 0x12 && record[0x33] == 0x34 && record[0xb0] == 0xab && record[0xb1] == 0xcd &&
            record[0xc6] == 0x78 && record[0xcc] == 0x56 && record[0xcd] == 0x78 &&
            record[0xd0] == 0 && record[0xd1] == 0xe3 && record[0xd2] == 0xab && record[0xd3] == 0xcd,
            "Player record writeback preserves original widths, truncation and big-endian byte order.");
        for (unsigned i : {4U, 0x31U, 0xb2U, 0xb3U, 0xc7U, 0xceU, 0xcfU, 0xd4U, 0x169U, 0x16bU})
            require(record[i] == 0xa5, "Player writeback preserves bytes outside original stores.");
    }

    unsigned hash(const char* name) {
        unsigned value = 0;
        while (*name) value = (value * 256 + static_cast<unsigned char>(*name++)) % 33554393;
        return value;
    }

    std::vector<std::uint8_t> executable() {
        std::vector<std::uint8_t> data(0x61100);
        word(data, 0, 256);
        word(data, 0x48, 0x80330000);
        word(data, 0x90, 0x60000);
        word(data, 4, 0x60100);
        word(data, 0x4c, 0x800ec000);
        word(data, 0x94, 0x1000);
        auto put = [&](unsigned address, unsigned value) { word(data, address - 0x80330000 + 256, value); };
        for (unsigned i = 0; i < 381; ++i) {
            put(glme01::actorTypeTable + i * 28, i % 262);
            put(0x8033b598 + i * 28, i == 1 ? 0x00010000 : 0);
            put(0x8033b5a4 + i * 28, i == 0 ? 0xffff0000 : 0);
            put(0x8033b5a8 + i * 28, 0x80330000 + (i == 1 ? 6 : 0));
            put(glme01::enemyConstructors + i * 16, i ? glme01::ordinaryCharacterConstructor : glme01::nullCharacterConstructor);
            put(glme01::strategyTypes + i * 16, i == 2 ? 0x800ec200 : 0x800ec100);
            put(0x8036425c + i * 16, 0x800ec300);
        }
        for (unsigned i = 0; i < 187; ++i) {
            put(glme01::strategyFactories + i * 4, 0x800d0000 + i * 4);
        }
        put(glme01::strategyFactories + 0x9a * 4, glme01::immediateConditionFactory);
        const unsigned selector[]{0x7c0802a6, 0x38800001, 0x90010004, 0x9421fff8,
            0x48000001 | ((0x800db0d8U - 0x800ec110U) & 0x03fffffc),
            0x8001000c, 0x38210008, 0x7c0803a6, 0x4e800020};
        for (unsigned i = 0; i < 9; ++i) {
            word(data, 0x60200 + i * 4, selector[i]);
        }
        word(data, 0x60300, 0x38600025);
        word(data, 0x60304, 0x4e800020);
        for (unsigned i = 0; i < 9; ++i) word(data, 0x60400 + i * 4, selector[i]);
        word(data, 0x60404, 0x3880009a);
        word(data, 0x60410, 0x48000001 | ((0x800db168U - 0x800ec310U) & 0x03fffffc));
        std::memcpy(data.data() + 256, "luige\0boo\0", 10);
        put(0x8033b5a4 + 2 * 28, 0x00010000);
        put(0x8033b5a4 + 3 * 28, 0xfffe0000);
        const unsigned capacities[]{5, 32, 130, 318}, types[]{0x48, 0xde, 0x0b, 0xb1}, counts[]{58, 20, 24, 1};
        for (unsigned i = 0; i < 262; ++i) put(glme01::actorAuxiliaryTable + i * 64, 3);
        put(glme01::actorAuxiliaryTable, 120);
        for (unsigned i = 0; i < 4; ++i) {
            put(glme01::enemyCreationPools + i * 8, capacities[i]);
            put(0x80360db4 + i * 8, types[i]);
            put(glme01::enemyCreationCounts + i * 4, counts[i]);
            put(glme01::actorAuxiliaryTable + types[i] * 64, capacities[i]);
        }
        return data;
    }

    jmp::ToolData characterTable(const char* createName, bool percent = false, bool door = false) {
        const char* fields[]{"name", "create_name", "pos_x", "pos_y", "pos_z", "dir_y", "invisible", "appear_percent", "OpenDoorNo"};
        const unsigned offsets[]{0, 16, 32, 36, 40, 44, 48, 52, 56};
        const unsigned count = 7 + (percent ? 1 : 0) + (door ? 1 : 0), start = 16 + count * 12;
        std::vector<std::uint8_t> data(start + 2 * 64);
        word(data, 0, 2); word(data, 4, count); word(data, 8, start); word(data, 12, 64);
        for (unsigned i = 0; i < count; ++i) {
            const unsigned field = i == 7 && !percent ? 8 : i;
            word(data, 16 + i * 12, hash(fields[field]));
            word(data, 20 + i * 12, 0xffffffff);
            word(data, 24 + i * 12, offsets[field] << 16);
        }
        for (unsigned row = 0; row < 2; ++row) {
            const unsigned offset = start + row * 64;
            std::memcpy(data.data() + offset, row ? "boo" : "luige", row ? 4 : 6);
            std::memcpy(data.data() + offset + 16, createName, std::strlen(createName) + 1);
            word(data, offset + 32, 0x3fc00000);
            word(data, offset + 36, 0xc0200000);
            word(data, offset + 40, 0x40e00000);
            word(data, offset + 44, 0xc2b40000);
            word(data, offset + 48, row);
            word(data, offset + 52, 50);
            word(data, offset + 56, 0x123);
        }
        jmp::ToolData table;
        std::string error;
        require(table.attach(data, error), error);
        return table;
    }

    struct ActorProbe : enemies::Actor {
        unsigned index = 0, kind = 0, location = 2;
        std::array<float, 3> coordinates{};
        std::array<std::int16_t, 3> angles{};
        std::uint8_t primary = 0, secondary = 0;
        unsigned id() const override { return index; }
        unsigned type() const override { return kind; }
        unsigned room() const override { return location; }
        void setPosition(const std::array<float, 3>& value) override { coordinates = value; }
        void setRotation(const std::array<std::int16_t, 3>& value) override { angles = value; }
        void visibility(std::uint8_t first, std::uint8_t second) override { primary = first; secondary = second; }
    };

    struct OwnerProbe : enemies::Owner {
        ActorProbe* object = nullptr;
        enemies::Manager* manager = nullptr;
        std::vector<unsigned>* destroyed = nullptr;
        std::vector<unsigned> events;
        unsigned activations = 0, deactivations = 0, begins = 0, updates = 0, failures = 0, strategy = 0, state = 0;
        ~OwnerProbe() override { if (object && destroyed) destroyed->push_back(object->id()); }
        void attach(enemies::Actor& actor, enemies::Manager& value) override {
            object = &dynamic_cast<ActorProbe&>(actor);
            manager = &value;
        }
        enemies::Actor& actor() override { return *object; }
        bool activate(unsigned type) override {
            require(manager->slots()[object->id()].state == 2, "Active state precedes owner activation.");
            ++activations;
            if (failures) { --failures; return false; }
            object->kind = type;
            return true;
        }
        void deactivate() override {
            require(manager->slots()[object->id()].state == 1, "Dormant state precedes owner deactivation.");
            ++deactivations;
        }
        void beginStrategy() override { ++begins; }
        void replaceStrategy(unsigned type) override { strategy = type; }
        void nextStrategyState(std::uint16_t value) override { state = value; }
        void update() override { ++updates; }
        std::uint32_t event(characterevent::Event& event) override {
            const auto value = event.message;
            if (value == 10 || value == 11)
                require(manager->slots()[object->id()].visible == (value == 10), "Visibility event precedes flag update.");
            events.push_back(value);
            if (event.types) event.types->at(object->type()) = 1;
            return 1;
        }
    };

    struct RuntimeProbe : enemies::Runtime {
        std::vector<std::unique_ptr<ActorProbe>> actors;
        std::vector<unsigned> destroyed, capacities, constructors, kinds, calls;
        OwnerProbe sentinel;
        bool visible = false, opened = false, captured = false, cleared = false;
        unsigned resets = 0, randomCalls = 0, randomValue = 0, room = 2, playerPolls = 0;
        std::unique_ptr<enemies::Owner> constructOwner(unsigned address, unsigned type) override {
            constructors.push_back(address); kinds.push_back(type); calls.push_back(2);
            auto owner = std::make_unique<OwnerProbe>(); owner->destroyed = &destroyed; return owner;
        }
        void resetActorIndex() override { ++resets; calls.push_back(1); require(actors.empty(), "Reset before actor construction."); }
        enemies::Actor& constructActor(unsigned type, unsigned capacity) override {
            calls.push_back(3); capacities.push_back(capacity);
            auto actor = std::make_unique<ActorProbe>(); actor->index = static_cast<unsigned>(actors.size()); actor->kind = type;
            actors.push_back(std::move(actor)); return *actors.back();
        }
        enemies::Owner& absentOwner() override { return sentinel; }
        unsigned resolveRoom(const std::array<float, 3>&) override { calls.push_back(0); return room; }
        bool roomVisible(unsigned, unsigned buffer) override { require(buffer == 0, "Original visibility buffer."); return visible; }
        bool doorOpened(unsigned, unsigned door) override { require(door == 0x23, "Door argument truncates to byte."); return opened; }
        bool typeCaptured(unsigned player, unsigned) override { require(player == 0, "Capture suppression uses player zero."); return captured; }
        bool roomCleared(unsigned value) override { require(value == room >> 24, "Room group uses high byte."); return cleared; }
        std::uint32_t random() override { ++randomCalls; return randomValue; }
        std::array<float, 3> playerPosition(unsigned) override { return {8, 9, 10}; }
        unsigned playerRoom(unsigned) override { return 42; }
        bool playerSpecialState(unsigned player) override { ++playerPolls; return player == 0; }
    };

    struct StrategyProbe : enemystrategy::Strategy {
        std::vector<unsigned>& calls;
        explicit StrategyProbe(std::vector<unsigned>& calls) : calls(calls) {}
        ~StrategyProbe() override { calls.push_back(9); }
        void begin() override { calls.push_back(8); }
        void initializeBehavior() override { calls.push_back(10); }
        void behavior() override { calls.push_back(11); }
    };

    struct OwnerRuntime : characterowner::Runtime, characterownership::Resources, characterownership::ActorLinks {
        std::vector<unsigned> calls;
        unsigned constructor = 0, inactive = 0;
        characterowner::Owner* constructionOwner = nullptr;
        characterownership::Resources& resources() override { return *this; }
        characterownership::ActorLinks& links(enemies::Actor&) override { return *this; }
        characterownership::Resource* createPrimary(unsigned, const std::array<float, 3>&) override {
            throw std::runtime_error("Unexpected special-resource factory in ordinary-owner test.");
        }
        characterownership::Resource* createSecondary(unsigned, const std::array<float, 3>&, const std::array<float, 3>&) override {
            throw std::runtime_error("Unexpected special-resource factory in ordinary-owner test.");
        }
        std::int32_t playerValue() override { return 0; }
        unsigned gameVariant() override { return 0; }
        void setInactiveFlags() override { ++inactive; }
        void* child(unsigned) override { return nullptr; }
        void detach(void*) override { throw std::runtime_error("Unexpected child detach."); }
        void deactivate(void*) override { throw std::runtime_error("Unexpected child deactivation."); }
        void releaseAudio() override { calls.push_back(12); }
        void resetActor(enemies::Actor&, const jmp::ToolDataRef&) override { calls.push_back(1); }
        void clearInactiveFlags(enemies::Actor&) override { calls.push_back(2); }
        void initializeActor(enemies::Actor& actor, unsigned type) override {
            dynamic_cast<ActorProbe&>(actor).kind = type;
            calls.push_back(3);
        }
        std::unique_ptr<enemystrategy::Strategy> constructStrategy(std::uint32_t address, characterowner::Owner& owner, unsigned capacity) override {
            require(capacity == 0x800, "Original owner embeds 0x800 bytes of strategy storage.");
            calls.push_back(4);
            constructor = address;
            constructionOwner = &owner;
            return std::make_unique<StrategyProbe>(calls);
        }
        void updateEffects(characterowner::Owner&) override { calls.push_back(15); }
        void creationEvent(characterowner::Owner& owner) override {
            require(owner.ownership().strategy->owner == &owner, "Ordinary owner initializes strategy before creation event.");
            calls.push_back(5);
        }
        std::uint32_t color(unsigned) override { calls.push_back(6); return 0x12345678; }
        void writeColor(enemies::Actor&, std::uint32_t value) override {
            require(value == 0x12345678, "Creation color comes from type parameters.");
            calls.push_back(7);
        }
        void update(characterowner::Owner&) override { calls.push_back(13); }
        std::uint32_t event(characterowner::Owner& owner, characterevent::Event& event) override {
            if (event.message == 0x1d) {
                event.types->at(owner.actor().type()) = 1;
            }
            calls.push_back(14);
            return 1;
        }
    };

    struct ConcreteRuntime : RuntimeProbe {
        enemystrategy::Catalog& strategies;
        OwnerRuntime& ownerRuntime;
        ConcreteRuntime(enemystrategy::Catalog& strategies, OwnerRuntime& runtime) : strategies(strategies), ownerRuntime(runtime) {}
        std::unique_ptr<enemies::Owner> constructOwner(unsigned address, unsigned) override {
            return characterowner::construct(address, strategies, ownerRuntime);
        }
    };

    struct BindingOwnerRuntime : characterowner::CreationRuntime {
        using CreationRuntime::CreationRuntime;
        characterownership::Resources* itemResources = nullptr;
        characterownership::Resources& resources() override {
            return itemResources ? *itemResources : CreationRuntime::resources();
        }
    };

    void checkInitialization(enemies::Manager& manager, RuntimeProbe& runtime, const enemycreation::Catalog& creation, unsigned players) {
        manager.initializeActors();
        unsigned total = players;
        for (const auto& pool : creation.pools()) total += pool.count;
        require(runtime.resets == 1 && runtime.actors.size() == total, "Original complete preallocation count.");
        require(runtime.calls[players] == 1, "Resolve every player room before resetting actor index.");
        unsigned index = 0;
        for (; index < players; ++index) {
            require(runtime.kinds[index] == 0 && runtime.capacities[index] == creation.capacity(0), "Players precede pools.");
            require(manager.slots()[index].state == 2 && manager.appearPoint(index).actor == index, "Player ownership and ID.");
        }
        for (const auto& pool : creation.pools()) for (unsigned i = 0; i < pool.count; ++i, ++index) {
            const auto& slot = manager.slots()[index];
            require(runtime.kinds[index] == pool.type && runtime.capacities[index] == creation.capacity(pool.type), "Pool type and capacity order.");
            require(slot.state == 1 && slot.category == creation.category(pool.type), "Pool slots start dormant in their original category.");
            require(runtime.actors[index]->coordinates == std::array<float, 3>{-32000, -32000, -32000}, "Deactivation parks pooled actors.");
            require(dynamic_cast<OwnerProbe&>(*slot.owner).deactivations == 1, "Every preallocated pool owner is deactivated once.");
        }
        for (; index < 128; ++index) require(!manager.slots()[index].owner && manager.slots()[index].state == 0, "Unused slots remain unowned.");
        for (unsigned i = players + 1; i < runtime.calls.size(); i += 2)
            require(runtime.calls[i] == 2 && runtime.calls[i + 1] == 3, "Owner construction precedes actor construction.");
        require(&manager.owner(-1) == &runtime.sentinel && &manager.owner(127) == &runtime.sentinel,
            "Invalid and empty owner lookups use the original sentinel owner.");
    }
}

int main(int argc, char** argv) {
    try {
        {
            particles::GlowPool pool;
            std::array<float, 3> position{1, 2, 3};
            auto* first = pool.create(position, 2.6f, 1, {0xcf, 0x7e, 0x50, 0xff});
            for (unsigned i = 1; i < 45; ++i) require(pool.create(position, 1, 0, {}), "All original glow slots are available.");
            require(!pool.create(position, 1, 0, {}), "Glow exhaustion does not replace a live owner.");
            for (unsigned i = 0; i < 10; ++i) pool.update();
            require(first->state == 1 && first->intensity >= 1 && first->position == &position,
                "Glow fade-in retains the borrowed position and original unclamped accumulation.");
            require(pool.release(first) && !pool.release(nullptr), "Glow release preserves null handling.");
            require(!pool.create(position, 1, 0, {}), "A fading glow cannot be reused early.");
            for (unsigned i = 0; i < 11; ++i) pool.update();
            require(first->state == 0 && !first->position && pool.create(position, 1, 0, {}) == first,
                "Completed glow fade releases the original first-free slot.");
        }
        {
            std::array<std::uint8_t, 0x10c> retained;
            retained.fill(0xa5);
            playerevents::Payload payload(retained);
            require(payload.word(0) == 10 && payload.word(8) == 0xffffffff && payload.word(0x20) == 3 &&
                payload.word(0x48) == 100 && payload.word(0x50) == 0 && payload.number(0x54) == 1 &&
                payload.word(0x58) == 1 && payload.word(0x5c) == 0 && payload.word(0xb4) == 0x8021da40 &&
                payload.word(0xbc) == 0x8021da40 && payload.number(0xe4) == -1 && payload.number(0xfc) == 0.3f,
                "Player event constructor preserves original scalar and address defaults.");
            for (unsigned offset : {0x1du, 0x1eu, 0x1fu, 0x88u, 0x89u, 0x8au, 0x8bu, 0xa9u, 0xaau, 0xabu, 0xc6u, 0xc7u})
                require(payload.bytes[offset] == 0xa5, "Player event construction preserves bytes the original does not write.");
            require(payload.bytes[0x1c] == 0 && payload.bytes[0xa8] == 0 && payload.bytes[0xc4] == 0 && payload.bytes[0xc5] == 1,
                "Original byte writes do not overwrite neighboring retained padding.");
            payload.word(0x50, 3);
            payload.reset();
            require(payload.word(0x50) == 0 && payload.word(0x88) == 0xa5a5a5a5, "Payload reset repeats only original constructor writes.");
        }
        checkAnimationBinding();
        checkItemReflection();
        checkPathSampling();
        checkPlayerWriteback();
        roomlocation::Rooms testRooms({
            {0x00010203, {0, 0, 0, 10, 10, 10}, {}},
            {0x01020304, {0, 0, 0, 10, 10, 10}, {{0, 0, 0, 5, 5, 5}}}
        });
        require(testRooms.resolve({1, 1, 1}) == 0x00010203 && testRooms.resolve({1, 1, 1}, 0x01000000) == 0x01020304 &&
            testRooms.resolve({10, 1, 1}) == 0xffffffff && testRooms.contains(0, {0, 0, 0}) && !testRooms.contains(1, {5, 1, 1}),
            "Room lookup preserves hints, first-match order and half-open bounds.");
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        require(testRooms.contains(0, {nan, nan, nan}) && !testRooms.contains(1, {nan, nan, nan}),
            "Original single-box and region-list NaN comparisons differ.");
        auto modelData = std::make_shared<actormodel::Resource>();
        modelData->bytes.resize(16);
        modelData->bytes[9] = 2;
        modelData->bytes[13] = 1;
        actormodel::Instance automaticModel(modelData, 0);
        actormodel::Instance pooledModel(nullptr, 5);
        require(automaticModel.capacity() == 3 && pooledModel.capacity() == 5 &&
            automaticModel.matrix(0, 2) == automaticModel.matrix(3, 2),
            "Model allocation uses declared joint and envelope counts, or explicit capacity without a model.");
        auto keyData = std::make_shared<actormodel::Resource>();
        automaticModel.attachment(0).field0 = 12;
        automaticModel.attachment(2).field0 = 15;
        automaticModel.attachment(0).object = &automaticModel;
        automaticModel.bindKey(keyData);
        require(automaticModel.attachment(0).field0 == 0 && automaticModel.attachment(2).field0 == 15 &&
            automaticModel.attachment(0).object == &automaticModel,
            "Key binding resets joints without resetting envelope slots or attached objects.");
        automaticModel.attachment(0).field0 = 7;
        automaticModel.bindKey(keyData);
        require(automaticModel.attachment(0).field0 == 7, "Identical key does not reset cursors.");
        bool rejectedModel = false;
        try {
            actormodel::Instance invalid(modelData, 2);
        } catch (const std::runtime_error&) {
            rejectedModel = true;
        }
        require(rejectedModel, "Explicit model capacity cannot be smaller than declared joint and envelope counts.");
        std::string error;
        auto dol = executable();
        enemytypes::Catalog types;
        enemycreation::Catalog creation;
        require(types.read(dol, error) && creation.read(dol, error), error);
        require(creation.capacity(0) == 130 && creation.category(1) == 0, "Model capacity determines pool classification.");
        require(!creation.suppressInClearedRoom(0) && creation.suppressInClearedRoom(1) &&
            !creation.countsForRoom(0) && creation.countsForRoom(1) && !creation.countsForRoom(2) && creation.countsForRoom(3) &&
            !creation.countsForRoom(0xef) && !creation.countsForRoom(0xfd) && creation.countsForRoom(0xfe),
            "Signed type groups and original excluded type range govern room-count eligibility.");
        auto damaged = dol; word(damaged, 0x90, 0x80000);
        require(!creation.read(damaged, error) && creation.capacity(0) == 130, "Failed catalog reads preserve the prior catalog.");
        auto table = characterTable("----");
        RuntimeProbe runtime;
        {
            enemies::Manager manager(types, creation, runtime, 1, 0);
            manager.attachCharacterInfo(table, 0);
            require(manager.playerPosition(0) == std::array<float, 3>{1.5f, -2.5f, 7}, "Preinitialization uses appearance coordinates.");
            require(jmp::sameEntry(manager.appearPoint(0).source, {&table, 0}), "Owned ToolData wrapper preserves resource identity.");
            checkInitialization(manager, runtime, creation, 1);
            require(runtime.actors[0]->angles[1] == -16384 && runtime.actors[0]->primary == 255, "Original placement rotation and visibility.");
            require(manager.playerPosition(0) == std::array<float, 3>{8, 9, 10} && manager.playerRoom(0) == 42, "Live player queries replace appearance queries.");
            jmp::ToolDataRef player{&table, 0}, entry{&table, 1};
            require(manager.create(player) && runtime.actors.size() == 104, "Player rows never allocate another player.");
            auto& first = dynamic_cast<OwnerProbe&>(manager.owner(1));
            auto& second = dynamic_cast<OwnerProbe&>(manager.owner(2));
            first.failures = 1;
            require(!manager.create(entry) && first.activations == 1 && second.activations == 0 && manager.slots()[1].state == 1,
                "Failed activation cleans the first matching slot without trying another slot.");
            require(manager.create(entry) && first.begins == 1 && runtime.actors[1]->primary == 0, "Successful activation places and begins strategy.");
            require(manager.message(entry, "start") && first.state == 0x104 && manager.message(entry, "stop") && first.state == 0x103,
                "Source identity routes the original strategy commands.");
            require(!manager.message(entry, "Start"), "Command matching is case sensitive.");
            manager.update(1); manager.update(2);
            require(first.updates == 1 && second.updates == 0, "Only active owners update.");
            runtime.visible = true; manager.updateVisibility(); manager.updateVisibility();
            runtime.visible = false; manager.updateVisibility();
            require(first.events == std::vector<unsigned>{11, 10} && runtime.playerPolls == 3, "Visibility transitions emit once and poll player state.");
            require(manager.countRoom(30) == 1 && manager.countRoom(0x1200001e) == 1, "Active room count preserves room 2/30 equivalence.");
            const auto mask = manager.resourceMask();
            require(mask[0] == 1 && mask[1] == 1, "Active owners contribute type-to-model resource marks.");
            require(manager.remove(entry) && !manager.remove(entry), "Only the first live source match is removed.");
            require(manager.createActor(1, 19) == 1 && first.strategy == 19, "Dynamic spawning retains actor ID and replaces strategy.");
            manager.deactivate(1);
            auto pendingTable = characterTable("group");
            entry = {&pendingTable, 1};
            require(manager.create(entry) && manager.create(entry) && manager.countNamed("group") == 2, "Named requests retain duplicates.");
            require(!manager.remove(entry) && manager.pending().size() == 1, "Pending removal alone still returns false.");
            first.failures = 1;
            const auto attempts = first.activations;
            manager.activateNamed("group");
            require(manager.pending().size() == 0 && first.activations == attempts + 2 && manager.countNamed("group") == 1,
                "Named activation retries the same request after failure, then removes it on success.");
            require(manager.remove(entry), "Remove named active owner.");
            auto randomTable = characterTable("group", true, true);
            entry = {&randomTable, 1};
            runtime.opened = true;
            require(!manager.create(entry) && runtime.randomCalls == 0, "Door suppression precedes random consumption.");
            runtime.opened = false; runtime.captured = true;
            require(!manager.create(entry) && runtime.randomCalls == 0, "Captured marked types are suppressed before random consumption.");
            runtime.captured = false; runtime.randomValue = 150;
            require(!manager.create(entry) && runtime.randomCalls == 1, "Appearance threshold is exclusive and uses modulo 100.");
            runtime.randomValue = 149;
            require(manager.create(entry) && runtime.randomCalls == 2, "Random values below the threshold create a pending request.");
        }
        require(runtime.destroyed.size() == 104, "Manager destroys all owners.");
        for (unsigned i = 0; i < 104; ++i) require(runtime.destroyed[i] == 103 - i, "Owners are destroyed in reverse slot order.");
        require(runtime.actors.size() == 104, "Manager destruction does not delete the separately registered actors.");
        RuntimeProbe multiplayer;
        {
            enemies::Manager manager(types, creation, multiplayer, 2, 2);
            manager.attachCharacterInfo(table, 0);
            checkInitialization(manager, multiplayer, creation, 2);
            require(manager.anyPlayerSpecialState() && multiplayer.playerPolls == 2, "Player polling does not short circuit.");
            jmp::ToolDataRef entry{&table, 1};
            multiplayer.room = 0x03000002; multiplayer.cleared = true;
            require(!manager.create(entry), "Map-mode suppression uses the resolved room group.");
            multiplayer.room = 0x46;
            require(manager.create(entry), "Original room 0x46 bypasses cleared-room suppression.");
        }
        enemystrategy::Catalog strategies;
        require(strategies.read(dol, error), error);
        require(strategies.select(1, {}) == 1 && strategies.select(2, {}) == 37 && strategies.constructor(186) == 0x800d02e8,
            "Original wrapper and fixed strategy selectors decode separately.");
        require(strategies.selectGenerator(0) == 0x9a && strategies.selectGenerator(380) == 0x9a,
            "Generator selectors decode their separate original constructor column.");
        auto badStrategy = dol;
        word(badStrategy, 0x60210, 0x48000001);
        require(!strategies.read(badStrategy, error) && strategies.select(1, {}) == 1, "Unsupported selector code preserves the loaded catalog.");
        OwnerRuntime ownerRuntime;
        ConcreteRuntime concrete(strategies, ownerRuntime);
        {
            enemies::Manager manager(types, creation, concrete, 1, 0);
            manager.attachCharacterInfo(table, 0);
            manager.initializeActors();
            require(concrete.actors.size() == 104 && ownerRuntime.inactive == 103, "All real owners construct and pooled owners use real resource teardown.");
            auto& player = dynamic_cast<characterowner::Owner&>(manager.owner(0));
            auto& ordinary = dynamic_cast<characterowner::Owner&>(manager.owner(1));
            require(player.kind() == characterowner::Kind::Null && ordinary.kind() == characterowner::Kind::Ordinary && ordinary.path().count() == 0,
                "Original factory selects null and ordinary owners with distinct path ownership.");
            ownerRuntime.calls.clear();
            jmp::ToolDataRef entry{&table, 1};
            require(manager.create(entry) && ownerRuntime.calls == std::vector<unsigned>{1, 2, 3, 4, 5, 6, 7, 8},
                "Concrete owner activation connects reset, actor initialization, strategy construction, event, color, and strategy begin.");
            require(ownerRuntime.constructor == strategies.constructor(1), "Type selector routes the strategy constructor.");
            require(ownerRuntime.constructionOwner == &ordinary, "Original activation constructs strategy storage inside its owning character.");
            player.replaceStrategy(0);
            ownerRuntime.calls.clear();
            player.beginStrategy();
            require(ownerRuntime.calls == std::vector<unsigned>{8}, "Activation calls strategy begin even for null owners.");
            ownerRuntime.calls.clear();
            player.updateStrategy();
            require(ownerRuntime.calls.empty(), "Null-owner per-frame strategy callback is empty.");
            ordinary.state().field954 = 41;
            ordinary.updateStrategy();
            require(ownerRuntime.calls == std::vector<unsigned>{10, 11, 15} && ordinary.state().field958 == 41,
                "Ordinary owner updates strategy before its per-frame creation-light callback.");
            ownerRuntime.calls.clear();
            ordinary.state().flags904[0] = true;
            ordinary.updateStrategy();
            ordinary.state().flags904[0] = false;
            ordinary.state().field91c = true;
            ordinary.updateStrategy();
            ordinary.state().field91c = false;
            require(ownerRuntime.calls.empty(), "Original owner gates suppress strategy and light together.");
            ordinary.replaceStrategy(1);
            require(ownerRuntime.constructionOwner == &ordinary, "Explicit strategy replacement reuses the same owning character storage.");
            require(manager.message(entry, "stop") && ordinary.ownership().strategy->nextState == 0x103, "Manager command reaches concrete strategy state.");
            ordinary.state().value = 0.25f;
            ordinary.state().field8d8 = {1, 2, 3};
            ordinary.state().embeddedStrategy.frames = 91;
            ordinary.state().fields93c = {4, 5, 6, 7};
            ordinary.state().fields908.fill(3);
            ordinary.state().actorUsers[0] = &manager;
            ordinary.state().life = 99;
            ordinary.reset();
            require(ordinary.state().value == 0.25f && ordinary.state().field8d8 == std::array<float, 3>{1, 2, 3} &&
                ordinary.state().embeddedStrategy.frames == 91 && ordinary.state().fields93c == std::array<float, 4>{4, 5, 6, 7} &&
                !ordinary.state().actorUsers[0] && ordinary.state().life == 0 && ordinary.state().fields908 == std::array<std::uint32_t, 5>{} &&
                ordinary.state().controlActor == &ordinary.actor(), "Owner reset preserves original untouched fields and clears only reset state.");
            require(jmp::sameEntry(ordinary.source, entry) && !ordinary.ownership().strategy, "Reset retains source identity while destroying strategy.");
            ordinary.replaceStrategy(37);
            {
                mission::Resources resources;
                gameflags::State flags;
                enemystrategy::Factory factory(concrete, flags, resources);
                charactergenerator::Manager generators(types, creation, strategies, factory, manager, concrete, resources);
                std::vector<std::uint8_t> data(116);
                word(data, 0, 1); word(data, 4, 5); word(data, 8, 76); word(data, 12, 40);
                const char* names[]{"name", "type", "pos_x", "pos_y", "pos_z"};
                const unsigned offsets[]{0, 16, 28, 32, 36};
                for (unsigned i = 0; i < 5; ++i) {
                    word(data, 16 + i * 12, hash(names[i]));
                    word(data, 20 + i * 12, 0xffffffff);
                    word(data, 24 + i * 12, offsets[i] << 16);
                }
                std::memcpy(data.data() + 76, "generator", 10);
                std::memcpy(data.data() + 92, "boo", 4);
                word(data, 104, 0x3f800000); word(data, 108, 0x40000000); word(data, 112, 0x40400000);
                jmp::ToolData table;
                require(table.attach(data, error), error);
                jmp::ToolDataRef entry{&table, 0};
                require(generators.create(entry), "Original generator records activate the first free owner.");
                auto& first = *generators.generators[0];
                require(first.strategy && first.strategy->context == &first && !first.strategy->owner && first.manager == &manager &&
                    first.type == 1 && first.position == std::array<float, 3>{1, 2, 3},
                    "Generator creation binds its context and source before reading placement and beginning strategy.");
                require(generators.message(entry, "start") && first.strategy->nextState == 0x104 &&
                    generators.message(entry, "stop") && first.strategy->nextState == 0x103 && !generators.message(entry, "other"),
                    "Generator commands preserve original state identifiers and reject unknown commands.");
                concrete.visible = false;
                first.total = 9;
                generators.enabled = false;
                generators.update();
                require(first.total == 9 && first.strategy->frames == 0, "Disabled generator managers do not reset totals or update strategies.");
                generators.enabled = true;
                generators.update();
                require(first.total == 0 && first.strategy->frames == 1, "Invisible generators reset lifetime totals before strategy update.");
                concrete.visible = true;
                first.total = 7;
                generators.update();
                require(first.total == 7, "Visible generators retain lifetime totals.");
                struct ModelObserver : enemystrategy::Strategy, charactergenerator::Observer {
                    void receive(charactergenerator::Event& event) override {
                        require(event.message == 1 && event.types, "Generator model requests carry the original event payload.");
                        (*event.types)[2] = 1;
                    }
                };
                first.strategy = std::make_unique<ModelObserver>();
                const auto models = generators.requiredModels();
                require(models[creation.model(1)] && models[creation.model(2)] && !models[creation.model(3)],
                    "Generator resource reporting combines the generated type and observer-requested types.");
                auto alias = table;
                jmp::ToolDataRef aliasEntry{&alias, 0};
                const auto childIndex = manager.createActor(1, -1);
                auto& generatedChild = dynamic_cast<characterowner::Owner&>(manager.owner(static_cast<int>(childIndex)));
                generatedChild.state().generator = &first;
                first.children[0] = &generatedChild;
                first.count = 1;
                require(generators.remove(aliasEntry) && !first.strategy && !first.manager && first.total == 0 && first.count == 0 &&
                    first.position == std::array<float, 3>{1, 2, 3}, "Generator removal matches shared source identity and retains original placement storage.");
                require(!generatedChild.state().generator && manager.slots()[childIndex].state == 1,
                    "Manager removal clears child generator links before deactivation without sending removal callbacks.");
                require(!generators.remove(entry), "Removed generator records no longer match.");
                for (unsigned i = 0; i < 20; ++i) require(generators.create(entry), "Fill original twenty-generator pool.");
                auto* last = generators.generators.back()->strategy.get();
                require(generators.create(entry) && generators.generators.back()->strategy.get() == last,
                    "A full original generator pool still reports the record as handled.");
                ordinary.replaceStrategy(37);
            }
            require(ownerRuntime.constructor == strategies.constructor(37) && ordinary.ownership().strategy->owner == &ordinary &&
                ordinary.state().fields93c == std::array<float, 4>{}, "Strategy replacement executes ordinary-owner initialization.");
            {
                gameflags::State flags;
                mission::Resources paths;
                enemystrategy::Factory factory(concrete, flags, paths);
                auto immediate = factory.construct(glme01::immediateConditionFactory);
                immediate->update();
                require(immediate->state == 0 && immediate->nextState == 0x100 && immediate->frames == 1,
                    "Immediate conditions request completion for the following original strategy update.");
                immediate->update();
                require(immediate->state == 0x100 && immediate->nextState == 0x100 && immediate->frames == 1,
                    "Immediate conditions continue requesting completion even after entering the completion state.");
                auto inert = factory.construct(glme01::spearFactory);
                inert->update();
                require(inert->state == 0 && inert->nextState == 0xffff && inert->frames == 1,
                    "The original inert strategy retains the base clock without inventing completion or movement.");
                auto condition = [&](unsigned address) {
                    auto result = factory.construct(address);
                    result->owner = &ordinary;
                    return result;
                };
                auto empty = condition(glme01::roomEmptyConditionFactory);
                empty->update();
                require(empty->nextState == 0xffff, "Room-empty conditions count the active original owner.");
                auto cleared = condition(glme01::roomClearedConditionFactory);
                cleared->update();
                require(cleared->nextState == 0xffff, "Uncleared room does not complete the clear condition.");
                concrete.cleared = true;
                cleared->update();
                require(cleared->nextState == 0x100, "Room-clear conditions use the original room-state service.");
                concrete.cleared = false;
                auto inRoom = condition(glme01::playerInRoomConditionFactory);
                inRoom->update();
                require(inRoom->nextState == 0xffff, "Player-room conditions require exact room equality.");
                concrete.actors[1]->location = 42;
                inRoom->update();
                require(inRoom->nextState == 0x100, "Player-room conditions inspect player zero's live room.");
                auto occupied = condition(glme01::occupiedUnclearedRoomConditionFactory);
                occupied->update();
                require(occupied->nextState == 0x100, "Occupied-room conditions combine player, owner count and clear-state checks.");
                concrete.cleared = true;
                occupied = condition(glme01::occupiedUnclearedRoomConditionFactory);
                occupied->update();
                require(occupied->nextState == 0xffff, "Cleared occupied rooms do not satisfy the original condition.");
                concrete.cleared = false;
                concrete.actors[1]->location = 2;
                std::vector<std::uint8_t> data(64);
                word(data, 0, 1); word(data, 4, 2); word(data, 8, 40); word(data, 12, 24);
                word(data, 16, hash("cond_arg0")); word(data, 20, 0xffffffff); word(data, 24, 0);
                word(data, 28, hash("cond_string_arg0")); word(data, 32, 0xffffffff); word(data, 36, 4 << 16);
                word(data, 40, 0x13d);
                std::memcpy(data.data() + 44, "group", 6);
                jmp::ToolData conditions;
                require(conditions.attach(data, error), error);
                const auto source = ordinary.source;
                ordinary.source = {&conditions, 0};
                auto flagOn = condition(glme01::flagOnConditionFactory), flagOff = condition(glme01::flagOffConditionFactory);
                flagOn->update();
                flagOff->update();
                require(flagOn->nextState == 0xffff && flagOff->nextState == 0x100,
                    "Opposite flag conditions retain the original source argument's low-byte conversion.");
                flags.setOn(0x3d);
                flagOn->update();
                require(flagOn->nextState == 0x100, "Condition flags are read live after initialization.");
                auto named = condition(glme01::namedAbsentConditionFactory);
                named->update();
                require(named->nextState == 0x100, "Named conditions complete when the referenced group is absent.");
                auto pending = characterTable("group");
                jmp::ToolDataRef pendingEntry{&pending, 1};
                require(manager.create(pendingEntry), "Queue a named actor for condition testing.");
                named = condition(glme01::namedAbsentConditionFactory);
                named->update();
                require(named->nextState == 0xffff, "Named conditions include original pending requests before activation.");
                manager.remove(pendingEntry);
                ordinary.source = source;
                named = condition(glme01::namedAbsentConditionFactory);
                named->update();
                require(named->nextState == 0xffff, "Missing condition names do not imply an empty group.");
                manager.deactivate(1);
                empty->update();
                require(empty->nextState == 0x100, "Room-empty conditions finish after the manager releases the final eligible owner.");
                bool rejected = false;
                try { factory.construct(0x800eebf0); } catch (const std::runtime_error&) { rejected = true; }
                require(rejected, "Unported derived strategies never fall back to a generic strategy.");
            }
            {
                auto source = [](unsigned maximum, unsigned simultaneous) {
                    std::vector<std::uint8_t> data(116);
                    word(data, 0, 1); word(data, 4, 6); word(data, 8, 88); word(data, 12, 28);
                    const char* names[]{"max_enemy", "max_enemy_once", "path_name", "pos_x", "pos_y", "pos_z"};
                    const unsigned offsets[]{0, 4, 8, 16, 20, 24};
                    for (unsigned i = 0; i < 6; ++i) {
                        word(data, 16 + i * 12, hash(names[i]));
                        word(data, 20 + i * 12, 0xffffffff);
                        word(data, 24 + i * 12, offsets[i] << 16);
                    }
                    word(data, 88, maximum);
                    word(data, 92, simultaneous);
                    std::memcpy(data.data() + 96, "absent", 7);
                    word(data, 104, 0x3f800000); word(data, 108, 0x40000000); word(data, 112, 0x40400000);
                    jmp::ToolData result;
                    std::string error;
                    require(result.attach(data, error), error);
                    return result;
                };
                mission::Resources resources;
                charactergenerator::Generator generator(types, concrete, resources);
                auto data = source(2, 2);
                generator.source = {&data, 0};
                generator.manager = &manager;
                generator.position = {1, 2, 3};
                require(!generator.path().attached() && generator.parameter("missing") == 0,
                    "Generator path and parameter lookup preserve missing-resource and missing-field results.");
                concrete.cleared = true;
                concrete.visible = true;
                require(!generator.create(1, -1) && generator.total == 0, "Cleared rooms suppress original generator creation.");
                generator.flags = 2;
                concrete.visible = false;
                require(!generator.create(1, -1), "Generator clear suppression and visibility suppression have separate override bits.");
                generator.flags = 3;
                concrete.captured = true;
                require(!generator.create(1, -1), "Generator overrides do not bypass original capture suppression.");
                concrete.captured = false;
                auto* first = generator.create(1, -1);
                auto* second = generator.create(1, -1);
                require(first && second && first != second && generator.count == 2 && generator.total == 2 &&
                    first->state().generator == &generator && second->state().generator == &generator &&
                    !generator.create(1, -1), "Generator creation retains ordered child ownership and original cumulative limits.");
                struct Observer : enemystrategy::Strategy, charactergenerator::Observer {
                    std::function<void()> callback;
                    unsigned notifications = 0;
                    void receive(charactergenerator::Event& event) override {
                        const auto message = event.message;
                        require(message == 0, "Original generator removal notification.");
                        ++notifications;
                        callback();
                    }
                };
                auto observer = std::make_unique<Observer>();
                auto* listener = observer.get();
                generator.strategy = std::move(observer);
                listener->callback = [&]() {
                    require(generator.count == 1 && generator.children[0] == second &&
                        manager.slots()[first->actor().id()].state == 2,
                        "Generator removal compacts children and notifies before EnManager deactivates the actor.");
                };
                first->state().field91c = true;
                first->state().field920 = 1;
                first->updateReleaseTimer();
                require(first->state().field920 == 0 && listener->notifications == 0,
                    "Original delayed removal waits through timer zero.");
                first->updateReleaseTimer();
                require(first->state().field920 == 0xffffffff && !first->state().field91c &&
                    listener->notifications == 1 && generator.total == 2 &&
                    manager.slots()[first->actor().id()].state == 1 && first->state().generator == &generator,
                    "Negative timer completion releases through the generator while retaining original counters and links.");
                listener->callback = [&]() {
                    require(generator.count == 0 && manager.slots()[second->actor().id()].state == 2,
                        "Final generator removal still precedes actor deactivation.");
                };
                second->state().field91c = true;
                second->state().field920 = 0x80000000;
                second->updateReleaseTimer();
                require(second->state().field920 == 0x7fffffff && listener->notifications == 1,
                    "Original release countdown preserves 32-bit signed overflow behavior.");
                second->state().field920 = 0;
                second->updateReleaseTimer();
                require(listener->notifications == 2 && generator.count == 0 && !generator.create(1, -1),
                    "Releasing all children does not reset the generator's lifetime spawn limit.");
                generator.strategy.reset();
                generator.children[0] = first;
                generator.children[1] = second;
                generator.count = 2;
                generator.remove(player);
                require(generator.count == 1 && generator.children[0] == first && generator.children[1] == second,
                    "Original missing-child removal still decrements the count and retains the final record.");
                generator.reset();
                data = source(0, 20);
                generator.source = {&data, 0};
                generator.manager = &manager;
                generator.flags = 3;
                for (unsigned i = 0; i < 15; ++i) require(generator.create(1, -1), "Original generator accepts fifteen children.");
                require(!generator.create(1, -1) && generator.count == 15 && generator.total == 15,
                    "The original fixed child capacity remains independent of source limits.");
                while (generator.count) generator.children[0]->releaseFromManager();
                require(generator.total == 15, "Generator lifetime count survives every child release.");
                gameflags::State flags;
                enemystrategy::Factory factory(concrete, flags, resources);
                generator.strategy = factory.construct(glme01::generatorDoorFactory);
                generator.strategy->context = &generator;
                generator.type = 1;
                generator.strategy->update();
                require(generator.strategy->state == 0 && generator.strategy->nextState == 1 && generator.count == 0,
                    "Original door generator initializes placement one update before spawning.");
                generator.strategy->update();
                require(generator.count == 1 && generator.total == 16 &&
                    dynamic_cast<ActorProbe&>(generator.children[0]->actor()).coordinates == std::array<float, 3>{1, 2, 3},
                    "Door generator creates through EnManager and places the generated actor from the original fields.");
                generator.strategy->setNextState(0x103);
                generator.strategy->update();
                require(generator.count == 1, "Unknown door-generator states retain the original empty dispatch.");
                generator.strategy->setNextState(1);
                generator.strategy->update();
                require(generator.count == 2, "Returning to the original active state resumes generator creation.");
                while (generator.count) generator.children[0]->releaseFromManager();
                concrete.cleared = false;
                concrete.visible = false;
            }
        }
        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> compressed, archive, characterInfo;
            require(package.open(argv[1], error) && package.read("sys/main.dol", dol, error), error);
            require(types.read(dol, error) && creation.read(dol, error), error);
            require(strategies.read(dol, error), error);
            actormodel::Catalog modelCatalog;
            require(modelCatalog.read(dol, error), error);
            require(modelCatalog.type(0).model == 0 && modelCatalog.type(0).animations.size() == 160 &&
                modelCatalog.type(0).animations[0][0] == 130 && modelCatalog.model(0).keyMetadata.size() == 141 &&
                modelCatalog.model(0).path == "/model/luige.szp", "Original actor and key catalogs resolve Luigi's initial state.");
            std::vector<std::uint8_t> commonBytes, luigiBytes, modelRecord, auxiliaryRecord;
            require(package.read("files/Game/game_usa.szp", compressed, error) &&
                roomarchive::decompress(compressed, commonBytes, error), error);
            gamearchive::Archive common;
            require(common.read(commonBytes, error), error);
            {
                const int particle = common.findTypeResource(0x4b485f45, "obk_sui_s_a.pcr");
                require(particle >= 0, "Original character-effect particle resource exists in the common archive.");
                auto data = std::make_shared<std::vector<std::uint8_t>>(common.resource(static_cast<unsigned>(particle)));
                const unsigned offset = (unsigned(data->at(4)) << 8) | data->at(5);
                particles::Parameters particleParameters;
                particleParameters.load(data, offset);
                const int textureIndex = common.findTypeResource(0x4b485f45, "hokori4.bti");
                require(textureIndex >= 0, "Original emitter texture exists in the common archive.");
                auto texture = std::make_shared<std::vector<std::uint8_t>>(common.resource(static_cast<unsigned>(textureIndex)));
                particles::EmitterPool emitterPool(128, 1024, 1024);
                auto* emitter = emitterPool.create(data, texture);
                require(emitter && emitter->flags == 0xe && emitter->draw->source == data &&
                    emitter->draw->texture == texture && emitterPool.emitters.free.count == 127,
                    "Imported PCR and BTI resources initialize the original emitter owner and draw interface.");
                gamearchive::Resources particleResources(common);
                require(particleResources.get(0x4b485f45, "obk_sui_s_a.pcr") == particleResources.get(0x4b485f45, "obk_sui_s_a.pcr"),
                    "Repeated typed archive lookups retain original resource identity.");
                particles::EffectFactory particleFactory(dol, particleResources, emitterPool);
                particles::Forces forces(dol);
                {
                    particles::Emission emission(dol);
                    particles::Emitter emitting;
                    particles::Pool<particles::Node> pool(8);
                    emitting.nodePool = &pool.free;
                    emitting.parameters.flags = 2;
                    emitting.parameters.fieldF4 = 0x3f800000;
                    emitting.parameters.field108 = 0x40000000;
                    emitting.parameters.field14C = 10;
                    emitting.parameters.fields10C[8] = 0x3f800000;
                    emitting.parameters.fieldsD0[5] = 0x3f800000;
                    emitting.position = {1, 2, 3};
                    std::uint32_t random = 1, expected = 1;
                    for (unsigned i = 0; i < 10; ++i) expected = expected * 0x5eedf715u + 0x1b0cb173u;
                    emission.emit(emitting, random);
                    auto* node = emitting.nodes.sentinel.next;
                    require(emitting.nodes.count == 1 && random == expected && emitting.fieldB8 == 0 &&
                        node->fields0 == std::array<float, 9>{0, 0, 0, 0, 0, 2, 0, 0, 0} &&
                        node->fields24 == std::array<std::uint32_t, 3>{0x3f800000, 0x40000000, 0x40400000} &&
                        node->fields30[0] == 10 && node->fields38[1] == 1,
                        "Box emission retains ten RNG draws at zero spread and separates local position from emitter offset.");
                    emitting.parameters.fieldF4 = 0;
                    expected = random * 0x5eedf715u + 0x1b0cb173u;
                    emission.emit(emitting, random);
                    require(random == expected && emitting.nodes.count == 1,
                        "Zero emission rate still consumes its rate-variation draw.");
                    emitting.parameters.fieldsD0[5] = 0;
                    for (unsigned i = 0; i < 3; ++i) expected = expected * 0x5eedf715u + 0x1b0cb173u;
                    emission.emit(emitting, random);
                    require(random == expected && emitting.nodes.count == 1,
                        "Zero direction consumes two table-angle draws even when no particles are emitted.");
                    emitting.parameters.fieldF4 = 0x3f800000;
                    emitting.parameters.fieldsD0[5] = 0x3f800000;
                    for (const unsigned shape : {1u, 5u, 9u, 0u, 8u, 3u}) {
                        emitting.parameters.flags = shape;
                        emission.emit(emitting, random);
                    }
                    require(emitting.nodes.count == 7 && pool.free.count == 1,
                        "Original disk, sphere, cone and cylinder branches allocate through the shared node pool.");
                }
                {
                    particles::Emitter advancing;
                    particles::Pool<particles::Node> pool(2);
                    advancing.nodePool = &pool.free;
                    advancing.parameters.fields10C[1] = 0x3f800000;
                    advancing.parameters.fields10C[5] = 0x3f800000;
                    advancing.parameters.fields10C[11] = 0x3f800000;
                    advancing.parameters.field204 = 2;
                    advancing.parameters.fields206[0] = 1;
                    advancing.flags = 6;
                    auto* living = pool.free.take();
                    auto* dying = pool.free.take();
                    living->fields30 = {4, 0};
                    living->fields0[3] = 2;
                    living->fields0[6] = 99;
                    living->fields44 = {65535, 2};
                    dying->fields30 = {1, 0};
                    dying->fields24 = {1, 2, 3};
                    advancing.nodes.append(*living);
                    advancing.nodes.append(*dying);
                    struct EmitterCallback : particles::EmitterCallback {
                        unsigned calls = 0;
                        void advance(particles::Emitter& emitter) override {
                            ++calls;
                            emitter.parameters.fields10C[1] = 0;
                        }
                    } emitterCallback;
                    struct NodeCallback : particles::NodeCallback {
                        unsigned calls = 0;
                        bool advance(particles::Emitter&, particles::Node& node) override {
                            ++calls;
                            require(node.fields30[1] == calls, "Particle callback follows motion and age advancement.");
                            node.fields0[6] = 3;
                            node.fields54[4] = 1;
                            return false;
                        }
                    } nodeCallback;
                    advancing.fieldC4 = &emitterCallback;
                    advancing.fieldC8 = &nodeCallback;
                    std::uint32_t random = 0, expected = 0;
                    for (unsigned i = 0; i < 2; ++i) expected = expected * 0x5eedf715u + 0x1b0cb173u;
                    require(particles::advance(advancing, forces, random) == 1 && emitterCallback.calls == 1 &&
                        nodeCallback.calls == 1 && living->fields0[0] == 2 && living->fields44[0] == 1 &&
                        dying->fields30[0] == 0 && dying->fields24 == std::array<std::uint32_t, 3>{1, 2, 3} &&
                        pool.free.count == 1 && random == expected && advancing.fieldBC == 1,
                        "Emitter advancement caches damping before its callback, retires terminal nodes and ignores callback return values.");
                    require(particles::advance(advancing, forces, random) == 1 && living->fields0[0] == 5 &&
                        living->fields0[3] == 3 && nodeCallback.calls == 2 && advancing.fieldBC == 0 &&
                        advancing.fieldC0 == 1 && advancing.flags == 0x14,
                        "Captured particles retain prior callback acceleration and emitter cycle completion uses deferred release.");
                }
                particles::Node forced;
                forced.fields0.fill(0);
                forced.fields0[0] = 2;
                forced.fields0[3] = 3;
                particles::Parameters forceParameters;
                forceParameters.flags = 0x10000 | 0x80000 | 0x100000 | 0x20000;
                forceParameters.fields168 = {0x3f800000, 0, 0};
                forceParameters.fields19C = {0x40c00000, 0, 0, 0x40000000};
                forceParameters.fields1AC = {0x40800000, 0, 0, 0x3f800000};
                forceParameters.fields174 = {0x41200000, 0, 0};
                std::uint32_t forceRandom = 7;
                forces.apply(forceParameters, forced, forceRandom);
                require(forced.fields0[0] == 12 && forced.fields0[6] == 8 && forceRandom == 7,
                    "Particle force ordering accumulates acceleration before applying direct translation.");
                forceParameters.flags = 0x800000;
                forceParameters.fields1D0 = {0, 0, 0x3f800000, 0x40000000, 0x3f800000};
                forces.apply(forceParameters, forced, forceRandom);
                require(forced.fields0[3] == 0 && forced.fields0[6] == -4 && forced.fields0[8] == 2,
                    "Axis force clears velocity and adds its axial and transverse acceleration in original order.");
                forceParameters.flags = 0x400000;
                forceParameters.field1CC = 0;
                auto expectedRandom = forceRandom;
                for (unsigned i = 0; i < 3; ++i) expectedRandom = expectedRandom * 0x5eedf715u + 0x1b0cb173u;
                forces.apply(forceParameters, forced, forceRandom);
                require(forceRandom == expectedRandom && forced.fields0[6] == -4,
                    "Zero-amplitude particle jitter still consumes three shared RNG updates.");
                forceParameters.flags = 0x40000;
                forceParameters.fields180 = {0, 0, 0x3f800000, 0, 0x3f800000, 0, 0};
                forced.fields0.fill(0);
                forced.fields0[0] = 2;
                forces.apply(forceParameters, forced, forceRandom);
                require(forced.fields0[0] == 4 && forced.fields0[1] == 0 && forced.fields0[2] == 0,
                    "Zero-angle vortex retains radial translation through the original matrix path.");
                forceParameters.flags = 0x200000;
                forceParameters.fields1BC = {0x3f800000, 0x3f800000, 0x3f800000};
                forceParameters.fields1C8 = {0, 0, 0};
                for (unsigned table = 0; table < 6; ++table) {
                    forceParameters.fields1C8[2] = static_cast<std::uint8_t>(table);
                    forced.fields0.fill(0);
                    forced.fields0[0] = -33;
                    forces.apply(forceParameters, forced, forceRandom);
                    require(forced.fields0[6] == 0 && forced.fields0[7] == 0 && forced.fields0[8] == 0,
                        "Original noise tables support wrapped negative coordinates and zero-offset gradients.");
                }
                auto* effect = particleFactory.create(0, {1, 2, 3}, {4, 5, 6});
                require(effect && effect->position == std::array<float, 3>{1, 2, 3} &&
                    effect->field220 == std::array<float, 3>{4, 5, 6} && effect->field22C == std::array<float, 3>{} &&
                    effect->fieldC4 == 0 && effect->fieldC8 == 0 && particleFactory.history[0] == 0,
                    "Original particle catalog selects archive resources and initializes owner vectors without treating them as scale.");
                playeractor::View suctionPlayer{};
                std::array<float, 3> suctionOrigin{}, suctionAxis{0, 0, 1};
                particles::SuctionContext suctionContext{&suctionOrigin, &suctionAxis, 1};
                particles::Suction suction(suctionPlayer, suctionContext, [] { return 0u; });
                particles::SuctionCallback damageCallback(suction);
                damageCallback.capture = 1;
                auto* damage = particleFactory.createDamage(0, {7, 8, 9}, damageCallback);
                require(damage && damage->fieldC8 == &damageCallback && damage->fieldC4 == 0 &&
                    damage->position == std::array<float, 3>{7, 8, 9} && damage->field220 == std::array<float, 3>{} &&
                    damage->field22C == std::array<float, 3>{} && damageCallback.capture == 0 &&
                    particleFactory.history[0] == 200 && particleFactory.history[1] == 0 &&
                    damage->draw->alternateTexture == particleResources.get(0x4b485f45, "ps_ball.bti"),
                    "Damage effects share the original history and callback while binding all three original resources.");
                particles::EmitterPool fullPool(0, 0, 0);
                particles::EffectFactory fullFactory(dol, particleResources, fullPool);
                damageCallback.capture = 1;
                require(!fullFactory.createDamage(3, {}, damageCallback) && damageCallback.capture == 0 &&
                    fullFactory.history[0] == 203 && fullPool.attempts == 1,
                    "Damage allocation failure still clears the shared callback capture mode and records the attempt.");
                std::uint32_t simulationRandom = 1;
                particles::NativeSimulation simulation(dol, simulationRandom);
                emitter->fieldC4 = nullptr;
                emitter->fieldC8 = nullptr;
                for (unsigned frame = 0; frame < 90; ++frame) emitterPool.update(simulation);
                require(simulationRandom != 1 && emitterPool.attemptHistory[0] == 0,
                    "Imported character and damage emitters execute native emission, forces, callbacks and retirement.");
                require(particleParameters.flags == ((std::uint32_t(data->at(offset)) << 24) |
                    (std::uint32_t(data->at(offset + 1)) << 16) | (std::uint32_t(data->at(offset + 2)) << 8) | data->at(offset + 3)),
                    "Original character-effect factory parameter offset feeds the recovered particle initializer.");
            }
            actormodel::Resources modelResources(modelCatalog);
            require(modelCatalog.type(1).animations.size() == 160 && modelCatalog.type(2).animations.size() == 8 &&
                modelCatalog.type(3).animations.size() == 6 && modelCatalog.type(4).animations.size() == 9,
                "Each actor reads its own preceding animation count rather than the next actor's count.");
            for (unsigned type = 0; type < 381; ++type) {
                const auto& names = modelCatalog.type(type).animationNames;
                require(names.size() == modelCatalog.type(type).animations.size(), "Declared animation counts bound original name decoding.");
                for (unsigned i = 0; i < names.size(); ++i) {
                    const auto first = std::find(names.begin(), names.end(), names[i]);
                    require(modelCatalog.findAnimation(type, names[i]) == std::distance(names.begin(), first),
                        "Original animation lookup returns the first exact name match.");
                }
                require(modelCatalog.findAnimation(type, "POLTERPORT_MISSING_ANIMATION") == -1,
                    "Missing original animation names retain the signed sentinel.");
            }
            modelResources.initializeEmbedded(common);
            unsigned timingCount = 0;
            for (unsigned model = 0; model < 262; ++model) {
                if (!modelResources.entry(model).archive) continue;
                const auto& keys = modelCatalog.model(model).keyMetadata;
                for (unsigned key = 0; key < keys.size(); ++key) {
                    const auto timing = modelResources.timing(model, key);
                    if (!timing) continue;
                    ++timingCount;
                    require(actoranimation::sampleTiming(timing.get(), "POLTERPORT_MISSING_TIMING", 0) == -10000,
                        "Imported timing resources preserve the missing-channel sentinel.");
                    for (double frame : {0.0, 0.5, 10.0, 30.0}) {
                        require(std::isfinite(actoranimation::sampleTiming(timing.get(), "alfa2_timing", frame)),
                            "Imported alpha timing channels sample through original model key metadata.");
                    }
                }
            }
            require(timingCount != 0, "Imported model archives supply original timing resources.");
            unsigned pathCount = 0;
            for (unsigned model = 0; model < 262; ++model) {
                const auto& entry = modelResources.entry(model);
                if (!entry.archive) continue;
                const auto& keys = modelCatalog.model(model).keyMetadata;
                for (unsigned key = 0; key < keys.size(); ++key) {
                    const auto path = modelResources.path(model, key);
                    if (!path) continue;
                    std::array<float, 3> position;
                    actoranimation::samplePath(*path, 0, position);
                    for (const auto value : position) require(std::isfinite(value), "Original embedded path begins with finite coordinates.");
                    std::array<float, 3> displacement{11, 12, 13};
                    require(actoranimation::pathDisplacement(modelResources, model, key, 0, 0, false, displacement) &&
                        displacement == std::array<float, 3>{}, "Original path lookup and equal-frame displacement use shared model ownership.");
                    const auto& keyBytes = entry.archive->keys.at(key)->bytes;
                    const unsigned endFrame = (unsigned(keyBytes.at(4)) << 8) | keyBytes.at(5);
                    std::array<float, 3> terminal, one;
                    actoranimation::samplePath(*path, endFrame, terminal);
                    actoranimation::samplePath(*path, 1, one);
                    displacement = {11, 12, 13};
                    require(actoranimation::pathDisplacement(modelResources, model, key, 1, 0, false, displacement),
                        "Original forward path wrap remains available.");
                    for (unsigned axis = 0; axis < 3; ++axis) require(displacement[axis] == (11.0f + axis) + terminal[axis],
                        "Original forward path wrap adds the terminal sample to incoming displacement without recomputing its delta.");
                    require(actoranimation::pathDisplacement(modelResources, model, key, 1, 0, true, displacement),
                        "Original reverse path wrap remains available.");
                    for (unsigned axis = 0; axis < 3; ++axis) require(displacement[axis] == (one[axis] - position[axis]) - terminal[axis],
                        "Original reverse path wrap subtracts the terminal sample after computing its delta.");
                    ++pathCount;
                }
            }
            require(pathCount != 0, "Imported models include original animation paths.");
            {
                enemytypes::Manager parameters;
                require(parameters.loadParameters(dol, commonBytes, error), error);
                actormodel::InitialPose pose(dol);
                {
                    unsigned draws = 0;
                    actorcapture::Motion motion(dol, pose, [&] { ++draws; return 0u; });
                    std::array<std::uint8_t, 0x430> retained{};
                    retained[0x42f] = 0xab;
                    for (unsigned type : {0u, 1u, 3u, 5u, 6u, 9u, 120u, 121u, 122u, 123u}) {
                        actorcapture::State state(retained);
                        state.number(0x40, 12);
                        state.number(0x48, 24);
                        draws = 0;
                        motion.initialize(state, {0, 0, 0}, {0, 0, 100}, static_cast<std::uint16_t>(type));
                        const unsigned expected = 1 + (state.byte(0x2bc) ? 0 : 1) + 3 +
                            (type >= 120 ? 3 : 0) + (type >= 120 && type <= 122 ? 1 : 0);
                        require(draws == expected && state.byte(0x42f) == 0xab && state.number(0x214) == 0.5f &&
                            state.byte(0x288) == 1 && !state.byte(0x289) && state.number(0x54) == 1 && state.number(0x21c) == -1,
                            "Capture initialization preserves retained bytes, pre-reset distance ratio and original random draw count.");
                        require(state.half(0x210) == (type == 6 ? 5 : type), "Original capture type six remaps only after its parameter lookup.");
                        if (type == 6) require(state.number(0x410) == 1 && state.number(0x40) == 0,
                            "Capture type six retains its remap flag and original zero denominator behavior.");
                        if (type == 3) require(state.number(0x44) == 1500 && state.number(0x19c) == 1500,
                            "Capture type three initializes both original vertical fields.");
                        if (type >= 120) require(state.number(0x40) == 300 && state.number(0x48) == 300 &&
                            state.number(0x41c) == (type == 123 ? 180 : 150), "Special capture variants retain distinct initialization parameters.");
                        if (type >= 120) {
                            for (unsigned tick = 0; tick < 382; ++tick) {
                                motion.advanceSpecial(state, type == 123);
                                const unsigned phase = tick < 31 ? 0 : tick < 101 ? 1 : tick < 231 ? 2 : tick < 381 ? 3 : 4;
                                require(state.half(0x3a0) == tick + 1 && state.word(0x39c) == phase,
                                    "Special capture uses original phase boundaries at ticks 31, 101, 231 and 381.");
                            }
                            require(draws == expected && state.number(0x60) == 0.7f && state.number(0x74) == 0.7f && state.number(0x88) == 0.7f,
                                "Special capture frame updates consume no random values and retain the original scale matrix.");
                        }
                    }
                    actorcapture::State state(retained);
                    state.vector(0x24, {1, 0, 0});
                    state.number(0x40, 3);
                    state.number(0x48, 6);
                    motion.endpoints(state, {1, 2, 3}, {1, 2, 3});
                    require(state.vector(0x24) == std::array<float, 3>{1, 0, 0} && state.number(0x3c) == 0 && state.number(0x214) == 0.5f,
                        "Coincident capture endpoints retain the previous normalized direction.");
                    std::array<float, 3> output{7, 8, 9};
                    require(!actorcapture::targetDirection({}, {0, 0, 1}, {0, 0, 100}, output) &&
                        output == std::array<float, 3>{7, 8, 9}, "Failed capture-axis queries retain their caller's previous output.");
                    require(actorcapture::targetDirection({}, {1, 1, 0}, {25, 0, 0}, output) && output[1] == 1,
                        "Capture-axis queries include the twenty-five-unit transverse boundary without normalizing the supplied axis.");
                    state.number(0x48, 100);
                    state.number(0x214, 0.5f);
                    state.number(0x2d8, 0.25f);
                    state.byte(0x2e0, 2);
                    require(actorcapture::Motion::scale(state) == 0.296875f, "Capture scale repeatedly squares the distance ratio.");
                    state.number(0x48, 250);
                    state.number(0x40, 250);
                    require(actorcapture::Motion::scale(state) == 1, "Capture scale retains the inclusive full-size boundary.");
                    state.matrix(0x28c, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0});
                    motion.initialize(state, {}, {0, 0, 100}, 0);
                    const auto remaining = state.number(0x40);
                    const auto expectedSpeed = state.number(0x188) +
                        (state.number(0x188) < state.number(0x2cc) ? state.number(0x2d0) : 0);
                    motion.advanceOrdinary(state, {}, {0, 0, 100}, nullptr, nullptr);
                    require(state.number(0x40) == remaining - expectedSpeed && state.number(0x12c) == 0 &&
                        state.number(0x13c) == 0 && state.number(0x14c) == 0,
                        "Ordinary capture advances acceleration before distance and clears blended matrix translation.");
                    motion.initialize(state, {}, {0, 0, 100}, 123);
                    state.half(0x3b0, 0x4000);
                    state.number(0x3a4, 1);
                    state.number(0x378, 1);
                    motion.advanceSpecial(state, true);
                    require(state.number(0x3c4) == -1, "Capture variant 123 negates the unsigned orbit angle before table lookup.");
                    const actormodel::Matrix matrix{2, 0, 0, 10, 0, 4, 0, 20, 0, 0, 8, 40};
                    actormodel::Matrix inverse{};
                    require(actorcapture::invert(matrix, inverse) && inverse == actormodel::Matrix{0.5f, 0, 0, -5, 0, 0.25f, 0, -5, 0, 0, 0.125f, -5},
                        "Capture matrix inverse preserves original scale and translated origin.");
                    const auto retainedInverse = inverse;
                    require(!actorcapture::invert({}, inverse) && inverse == retainedInverse,
                        "Singular capture matrices leave the previous inverse untouched.");
                    motion.initialize(state, {}, {0, 0, 100}, 0);
                    motion.advance(state, {}, {0, 0, 100}, {2, 4, 8}, matrix, nullptr, nullptr);
                    const auto firstRotation = state.matrix(0x28c);
                    require(firstRotation[3] == 0 && firstRotation[7] == 0 && firstRotation[11] == 0 &&
                        state.matrix(0x1b0) == retainedInverse && state.byte(0x289) == 1 && !state.byte(0x288),
                        "First capture frame latches actor rotation, writes its inverse and advances initialization flags.");
                    const actormodel::Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
                    motion.advance(state, {}, {0, 0, 100}, {1, 1, 1}, identity, nullptr, nullptr);
                    require(state.matrix(0x28c) == firstRotation && state.matrix(0x1e0) == identity,
                        "Later capture frames preserve latched rotation while replacing the current actor matrix.");
                    playermodels::VacuumPath path(2);
                    const std::array<float, 3> axis{0, 0, 1};
                    bool missingScratch = false;
                    try { motion.advance(state, {}, {0, 0, 100}, {1, 1, 1}, identity, &path, &axis); }
                    catch (const std::runtime_error&) { missingScratch = true; }
                    require(missingScratch, "Degenerate capture path cannot silently substitute a direction for unwritten original stack data.");
                    state.half(0x210, 0);
                    state.number(0x214, -0.1f);
                    auto deformation = identity;
                    require(motion.ordinaryJointTransform(state, {}, deformation) && deformation == actormodel::Matrix{},
                        "Ordinary capture completion zeroes the deformation matrix once its ratio becomes negative.");
                    state.half(0x210, 5);
                    state.number(0x3fc, 1);
                    state.byte(0x360, 2);
                    deformation = identity;
                    require(motion.ordinaryJointTransform(state, {}, deformation) && deformation == identity,
                        "Capture type five reports completion while retaining unwritten output for an unsupported substate.");
                    state.half(0x210, 10);
                    state.number(0x214, 0.5f);
                    state.number(0x40, 50);
                    state.number(0x48, 100);
                    state.byte(0x2e0, 0);
                    state.number(0x428, 0);
                    state.number(0x42c, 0);
                    state.byte(0x180, 0);
                    const float registerValue = 0.25f;
                    motion.ordinaryJointTransform(state, {}, deformation, &registerValue);
                    require(deformation[0] == 0.75f && deformation[5] == 0.75f && deformation[10] == 0.75f && deformation[11] == 50,
                        "Capture type ten preserves the original retained-register scale branch.");
                    for (unsigned type : {120u, 123u}) {
                        motion.initialize(state, {}, {0, 0, 100}, static_cast<std::uint16_t>(type));
                        motion.advance(state, {}, {0, 0, 100}, {1, 1, 1}, identity, nullptr, nullptr);
                        state.half(0x3a0, 439);
                        require(!motion.jointTransform(state, 1, {1, 2, 3}, deformation),
                            "Special capture joint deformation remains active through frame 439.");
                        state.half(0x3a0, 440);
                        require(motion.jointTransform(state, 1, {1, 2, 3}, deformation) && deformation == actormodel::Matrix{},
                            "Special capture joint deformation returns completion and zero scale starting at frame 440.");
                    }
                    state.half(0x210, 121);
                    deformation = identity;
                    require(!motion.jointTransform(state, 1, {}, deformation) && deformation == identity,
                        "Original capture transform dispatch retains unwritten output for type 121.");
                    state.half(0x210, 0);
                    state.number(0x214, -1);
                    state.number(0x34, 1);
                    state.matrix(0xf0, identity);
                    state.matrix(0x1b0, identity);
                    auto joint = identity;
                    require(motion.deform(state, 1, {}, joint, deformation) && joint == actormodel::Matrix{},
                        "Completed capture deformation propagates zero scale into the original joint matrix.");
                }
                const auto orientation = playerorientation::fromMatrix(pose.rotation({0, 0, 0}));
                require(orientation == playerorientation::Quaternion{0, 0, 0, 1} &&
                    playerorientation::direction(orientation) == std::array<float, 3>{0, 0, 1},
                    "Player constructor derives its original initial quaternion and facing vector.");
                actors::Globals globals;
                actorcollision::Catalog collisionCatalog(dol, modelCatalog);
                actorcollision::Manager collisionManager(collisionCatalog, modelCatalog);
                require(collisionManager.elements[0]->count == 2 && collisionManager.elements[0]->field8 == 1,
                    "Original Luigi collision owner copies its two declared records.");
                for (unsigned type = 0; type < 381; ++type) {
                    const auto& descriptor = collisionCatalog.type(type);
                    const auto& element = *collisionManager.elements[type];
                    require(element.count == descriptor.records.size() && element.model == modelCatalog.type(type).model && !element.field18,
                        "Collision owner initializes every original actor type in declaration order.");
                    if (!descriptor.extended) require(element.extended == collisionCatalog.initialExtended,
                        "Absent collision override preserves the constructor's original extended record.");
                }
                roomlocation::Rooms rooms(dol, 2);
                actors::Construction construction{modelCatalog, modelResources, parameters, pose, globals, rooms};
                actors::MoveActor actor(construction, 0, 0, 130);
                {
                    using Vector = itemsuction::Vector;
                    actors::MoveActor item(construction, 127, 0, 130);
                    roomlocation::Rooms suctionRooms({{0, {-1000, -1000, -1000, 1000, 1000, 1000}, {}}});
                    roomtraversal::State visibility;
                    visibility.initialize(1);
                    std::uint32_t roomFlags = 0;
                    std::uint16_t savedFlags = 0;
                    struct Volume : playermodels::VacuumVolume {
                        Vector vector{0, 0, -1};
                        bool hit = true;
                        mutable Vector queried{};
                        bool intersects(double, Vector& direction, const Vector& position) const override {
                            queried = position;
                            direction = vector;
                            return hit;
                        }
                    } volume;
                    Vector origin{}, axis{0, 0, 1};
                    particles::SuctionContext suction{&origin, &axis, 1};
                    playeractor::View player{};
                    player.field8A0 = 1;
                    player.field8B4 = &volume;
                    itemsuction::Force force(player, suction);
                    gamearchive::Resources resources(common);
                    particles::EmitterPool pool(32, 0, 0);
                    particles::EffectFactory effects(dol, resources, pool);
                    gameflags::State flags;
                    struct Events : itemsuction::Events {
                        std::vector<unsigned> calls;
                        itemsuction::Completion payload;
                        unsigned actor = 0;
                        particles::EmitterPool* pool = nullptr;
                        unsigned effectCount = 0;
                        void attraction(unsigned value) override { actor = value; calls.push_back(10); }
                        void completion(const itemsuction::Completion& value) override {
                            payload = value;
                            effectCount = pool->active[0].count;
                            calls.push_back(19);
                        }
                        void collect(actors::MoveActor& value) override { actor = value.id(); calls.push_back(99); }
                    } events;
                    events.pool = &pool;
                    itemsuction::Environment environment{suctionRooms, visibility, pose, suction, force, effects, flags, events};
                    itemsuction::Controller controller(environment, 73);
                    item.position = {0, 0, 100};
                    item.fieldB4 = 0;
                    controller.initialize(item, 0, 5);
                    require(controller.remaining == 73 && controller.duration == 255 && controller.parameters.durationBase == 5 &&
                        controller.minimum == Vector{-1000, -1000, -1000}, "Suction initialization reads room bounds and preserves unwritten countdown state.");
                    require(!controller.update(item, 0, 25, {}) && events.calls.empty() && item.position == Vector{0, 0, 100},
                        "Invisible suction must leave the actor and event stream untouched.");
                    visibility.mark(0, 2, 0, roomFlags, savedFlags, 0, 0, 0, {}, {});
                    require(controller.update(item, 0, 25, {}) && events.calls == std::vector<unsigned>{10} && events.actor == 127 &&
                        std::abs(item.velocity[2] + 30) < 0.0001f && item.position == Vector{0, 0, 100} && item.animationSpeed == 2,
                        "Suction updates native velocity at double rate without directly integrating position.");
                    controller.update(item, 0, 25, {});
                    require(events.calls.size() == 1 && item.scale == Vector{0.65f, 0.65f, 0.65f},
                        "Continued attraction emits one begin event and distance-scales the actor.");
                    controller.duration = 100;
                    controller.remaining = 100;
                    controller.scale = 1;
                    controller.updateScale(item);
                    require(controller.scale == 0.96f, "Early capture applies the original four-percent scale decay.");
                    controller.remaining = 70;
                    controller.updateScale(item);
                    require(controller.scale == 0.65f, "Thirty elapsed countdown ticks snap scale to its distance target.");
                    controller.maximum[1] = 5;
                    volume.vector = {0, 1, 0};
                    controller.update(item, 10, 25, {});
                    require(volume.queried[1] == 6.5f && item.velocity[1] == 0 && item.position[1] == 0,
                        "Scaled query offset and room-height cancellation do not clamp native position.");
                    volume.vector = {0, 0, -1};
                    Vector output{91, 92, 93};
                    item.setVelocity({7, 8, 9});
                    require(controller.updateVelocity(item, output, {}) && std::abs(output[2] + 15) < 0.0001f &&
                        item.velocity == Vector{7, 8, 9}, "Velocity-only suction returns undoubled movement without overwriting the actor.");
                    volume.hit = false;
                    controller.scale = 0.5f;
                    item.previousRotation[2] = 99;
                    require(!controller.update(item, 0, 25, {}) && controller.scale == 0.55f && item.animationSpeed == 1 &&
                        item.previousRotation[2] == 0 && !controller.attracting,
                        "Miss recovery restores scale and animation rate and clears the attraction latch.");
                    volume.hit = true;
                    item.position = {0, 0, 20};
                    item.setVelocity({3, 4, 5});
                    events.calls.clear();
                    controller.duration = 255;
                    require(controller.update(item, 0, 25, {}) && events.calls == std::vector<unsigned>{19, 99} &&
                        events.effectCount == 2 && effects.history[0] == 1 && effects.history[1] == 0 &&
                        events.payload.kind == 3 && events.payload.amount == 1 && events.payload.type == item.type() &&
                        events.payload.collectionBit == -1 && item.rotation[1] == static_cast<std::int16_t>(pose.angle(std::sqrt(34.0f), 4)),
                        "Arrival creates both original effects before player completion and collection, using pre-arrival actor velocity.");
                    for (unsigned mode : {1u, 2u, 3u, 4u}) {
                        events.calls.clear();
                        controller.completionKind = mode;
                        controller.completionValue = 0x107;
                        const auto count = pool.active[0].count;
                        controller.complete(item);
                        if (mode == 4) require(events.calls == std::vector<unsigned>{99} && pool.active[0].count == count,
                            "Completion mode four calls collection without events or effects.");
                        else {
                            require(events.calls == std::vector<unsigned>{19, 99} && events.effectCount == count + 2,
                                "Completion modes retain effect-event-collection ordering.");
                            if (mode == 1) require(events.payload.kind == 0 && events.payload.delayEnabled == 1 && events.payload.delay == 0x107,
                                "Delay completion preserves its explicit word value.");
                            if (mode == 2) require(flags.value(7) && events.payload.kind == 0 && events.payload.delay == 10,
                                "Flag completion truncates its flag ID and retains default event fields.");
                            if (mode == 3) require(events.payload.collectionBit == 0x107 && events.payload.kind == 3,
                                "Indexed completion preserves the full collection-bit word.");
                        }
                    }
                    controller.completionKind = 0;
                    controller.emitEffects = 0;
                    const auto count = pool.active[0].count;
                    events.calls.clear();
                    controller.complete(item);
                    require(pool.active[0].count == count && events.calls == std::vector<unsigned>{19, 99},
                        "Disabled completion effects must not suppress notification or collection.");
                    events.calls.clear();
                    output = {91, 92, 93};
                    controller.duration = 255;
                    require(!controller.updateVelocity(item, output, {}) && output == Vector{91, 92, 93} && events.calls.empty(),
                        "Velocity-only arrival takes recovery, leaves output untouched and never invokes collection.");
                    particles::EmitterPool exhausted(0, 0, 0);
                    particles::EffectFactory exhaustedEffects(dol, resources, exhausted);
                    itemsuction::Environment exhaustedEnvironment{suctionRooms, visibility, pose, suction, force, exhaustedEffects, flags, events};
                    itemsuction::Controller exhaustedController(exhaustedEnvironment, 0xa5);
                    exhaustedController.initialize(item, 0, 5);
                    events.calls.clear();
                    exhaustedController.complete(item);
                    require(events.calls == std::vector<unsigned>{19, 99} && exhaustedEffects.history[0] == 1 &&
                        exhaustedEffects.history[1] == 0 && exhaustedController.remaining == 0xa5,
                        "Failed effect allocation must still attempt both effects, notify the player and invoke collection.");
                }
                {
                    actors::MoveActor captured(construction, 1, 0, 130);
                    captured.position = {0, 0, 100};
                    captured.flags &= ~0x80000u;
                    captured.retainCapture(std::array<std::uint8_t, 0x430>{});
                    auto& state = *captured.captureState();
                    unsigned draws = 0;
                    actorcapture::Motion motion(dol, pose, [&] { ++draws; return 0u; });
                    captured.initializeCapture(motion, {}, 0);
                    const auto initializedDraws = draws;
                    captured.initializeCapture(motion, {50, 60, 70}, 1);
                    require((captured.flags & 0x80000) && captured.field100 == std::array<float, 3>{} && draws == initializedDraws,
                        "An already-captured actor retains its target and does not initialize motion twice.");
                    state.number(0x40, -1);
                    actormodel::Matrix retained{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
                    captured.updateCapture(motion, nullptr, nullptr, retained);
                    require(!(captured.flags & 0x80000) && captured.model->matrix(0, 0) == actormodel::Matrix{} &&
                        captured.field160 == state.number(0x54),
                        "Imported actor capture deforms original joint matrices and clears its flag from the final joint result.");
                    const auto model = captured.model->model;
                    captured.model->model.reset();
                    captured.initializeCapture(motion, {4, 5, 6}, 0);
                    require(captured.field100 == std::array<float, 3>{4, 5, 6} && !(captured.flags & 0x80000) && draws == initializedDraws,
                        "A model-less actor stores the original target without activating capture or consuming random values.");
                    captured.model->model = model;
                    const auto stored = state.bytes;
                    captured.resetOwner({});
                    captured.initializeType(0);
                    require(captured.captureState() == &state && state.bytes == stored,
                        "Owner reset and actor type initialization preserve embedded capture storage across pooled reuse.");
                    bool rejected = false;
                    try { captured.retainCapture({}); } catch (const std::runtime_error&) { rejected = true; }
                    require(rejected && state.bytes == stored, "Rebinding cannot discard an actor's retained capture state.");
                }
                {
                    const auto originalMatrix = actor.matrix;
                    const auto originalRoot = actor.model->matrix(0, 0);
                    const auto originalJoint = actor.model->matrix(0, 1);
                    const auto originalFlags = actor.flags;
                    const auto originalVisibility = actor.fieldE0;
                    actor.matrix = {1, 0, 0, 10, 0, 1, 0, 20, 0, 0, 1, 30};
                    actor.model->matrix(0, 0) = {1, 0, 0, 1, 0, 1, 0, 2, 0, 0, 1, 3};
                    actor.model->matrix(0, 1) = {1, 0, 0, 4, 0, 1, 0, 5, 0, 0, 1, 6};
                    actor.flags &= ~0xc000u;
                    actor.fieldE0 = 255;
                    particles::Emitter expired, first, second;
                    first.flags = second.flags = 0xe;
                    characterownership::Ownership ownership;
                    ownership.attachments[0] = {&expired, 0, {}};
                    ownership.attachments[1] = {&first, 1, {7, 8, 9}};
                    ownership.attachments[2] = {&second, 0, {}};
                    ownership.attachmentCount = 3;
                    ownership.updateAttachments(actor);
                    require(ownership.attachmentCount == 2 && ownership.attachments[0].resource == &first &&
                        ownership.attachments[1].resource == &second && first.position == std::array<float, 3>{22, 35, 48} &&
                        first.field220 == std::array<float, 3>{} && (first.flags & 8),
                        "Attachment update compacts finished entries and applies original joint/root transforms in order.");
                    actor.flags |= 0x4000;
                    ownership.updateAttachments(actor);
                    require(!(first.flags & 8) && !(second.flags & 8), "Inactive actor flags hide both remaining attachments.");
                    first.flags = second.flags = 0;
                    ownership.updateAttachments(actor);
                    require(ownership.attachmentCount == 0, "Consecutive finished attachments are removed without skipping shifted entries.");
                    actor.matrix = originalMatrix;
                    actor.model->matrix(0, 0) = originalRoot;
                    actor.model->matrix(0, 1) = originalJoint;
                    actor.flags = originalFlags;
                    actor.fieldE0 = originalVisibility;
                }
                charactercreation::Properties creationProperties(dol, creation, parameters);
                {
                    std::array<std::uint8_t, 48> captureBytes{}, seenBytes{};
                    std::array<std::uint8_t, 6> gradeBytes;
                    playercomponents::BitBuffer captures(captureBytes.data(), 384, 48, 1);
                    playercomponents::BitBuffer seen(seenBytes.data(), 384, 48, 1);
                    playercomponents::BitBuffer grades(gradeBytes.data(), 24, 6, 2);
                    playercomponents::BitBuffer absent(nullptr, 384, 48, 1);
                    require(!playerevents::registerCapture(absent, grades, creationProperties, 0x40, 100),
                        "Unbound capture storage rejects registration without touching grades.");
                    for (unsigned type : {0x40u, 0xd5u, 0xdcu, 0xddu}) {
                        const auto index = creationProperties.savedIndex(type);
                        require(index >= 0, "Original graded capture types have saved actor slots.");
                        for (unsigned life : {0u, 59u, 60u, 89u, 90u, 100u, 65535u}) {
                            captureBytes.fill(0);
                            gradeBytes.fill(0xa5);
                            auto expected = gradeBytes;
                            unsigned grade = life == 65535 || life < 60 ? 1 : life < 90 ? 2 : 3;
                            for (unsigned bit = unsigned(index) * 2 + 2; bit != unsigned(index) * 2;) {
                                --bit;
                                const auto mask = static_cast<std::uint8_t>(1u << (bit & 7));
                                expected[bit >> 3] = grade & 1 ? expected[bit >> 3] | mask : expected[bit >> 3] & ~mask;
                                grade >>= 1;
                            }
                            require(playerevents::registerCapture(captures, grades, creationProperties, type,
                                static_cast<std::uint16_t>(life)) && gradeBytes == expected,
                                "Capture grades preserve signed life boundaries and descending bit order.");
                            require(!playerevents::registerCapture(captures, grades, creationProperties, type, 0) && gradeBytes == expected,
                                "Repeated capture registration does not overwrite the original grade.");
                        }
                    }
                    captureBytes.fill(0);
                    gradeBytes.fill(0xa5);
                    require(playerevents::registerCapture(captures, grades, creationProperties, 1, 0) && gradeBytes[0] == 0xa5,
                        "Ordinary capture registration does not write a grade.");
                    require(playerevents::registerSeen(seen, 383) && !playerevents::registerSeen(seen, 383) && seenBytes[47] == 128,
                        "Seen registration writes its own buffer and reports only the first insertion.");
                    gradeBytes.fill(0);
                    playerevents::promoteGrade(grades, 3, 2);
                    require(gradeBytes[0] == 64, "Item grade promotion writes most significant grade bits at the first slot bit.");
                    playerevents::promoteGrade(grades, 3, 1);
                    require(gradeBytes[0] == 64, "A lower item grade cannot replace the saved grade.");
                    playerevents::promoteGrade(grades, 3, 3);
                    require(gradeBytes[0] == 192, "A higher item grade replaces only its own bit range.");
                    playerevents::promoteGrade(grades, -1, 3);
                    playerevents::promoteGrade(grades, 3, -1);
                    require(gradeBytes[0] == 192, "Negative item grade arguments leave storage unchanged.");
                    playerevents::promoteGrade(grades, 3, 4);
                    require(gradeBytes[0] == 0, "Original oversized grade values truncate on write rather than clamping.");
                }
                playerrecord::Record lifeRecord;
                lifeRecord.fill(0xff);
                unsigned savedTypes = 0;
                for (unsigned type = 0; type < 381; ++type) {
                    const bool savedLife = creation.group(type) == 3 && creationProperties.savedIndex(type) >= 0;
                    if (savedLife) ++savedTypes;
                    require(creationProperties.life(type, lifeRecord) ==
                        (savedLife ? 0xffffffff : parameters.parameters(type).entries()[0].bits),
                        "Creation event uses signed saved life for mapped group-three types and parameter life otherwise.");
                }
                require(savedTypes != 0, "Imported saved-life mapping covers original actors.");
                creationProperties.writeColor(actor, 0x12345678);
                require(actor.fieldsE9 == std::array<std::uint8_t, 3>{0x12, 0x34, 0x56} && actor.fieldEC == 0x78,
                    "Original creation color writes the packed ambient and opacity bytes.");
                {
                    roomlighting::Pool lights;
                    unsigned eligible = 0;
                    for (unsigned type = 0; type < 381; ++type) {
                        lights.beginFrame();
                        actor.actorType = type;
                        actor.fieldE0 = 128;
                        creationProperties.updateEffects(actor, lights);
                        require(lights.count() == (creation.group(type) == 3 ? 2U : 1U),
                            "Original creation light is emitted only for group-three actor types.");
                        if (creation.group(type) == 3) {
                            ++eligible;
                            const auto color = parameters.parameters(type).entries()[24].bits;
                            require(lights.entry(1).light.position == actor.position &&
                                lights.entry(1).light.color[0] == static_cast<unsigned>((color >> 24) * (128.0f / 255.0f)) &&
                                lights.entry(1).light.color[3] == 255 && lights.entry(1).light.referenceDistance == 40 &&
                                lights.entry(1).light.brightnessPercent == 80,
                                "Creation light preserves original visibility scaling, position and attenuation inputs.");
                        }
                        actor.fieldE0 = 0;
                        creationProperties.updateEffects(actor, lights);
                        require(lights.count() <= 2, "Invisible actors do not allocate creation lights.");
                    }
                    require(eligible != 0, "Imported type table includes creation-light actors.");
                    actor.actorType = 0;
                }
                {
                    enemies::Manager parent(types, creation, concrete, 1, 2);
                    require(parent.nearPlayer({199, 0, 0}, 200) == 0 && parent.nearPlayer({200, 0, 0}, 200) == -1,
                        "Original player proximity excludes the exact range boundary.");
                    characterowner::Owner owner(characterowner::Kind::Null, strategies, ownerRuntime);
                    owner.attach(actor, parent);
                    {
                        const auto primary = actor.fieldE0, secondary = actor.fieldE4;
                        const auto speed = actor.fieldE8;
                        actor.fieldE0 = 0x12345678;
                        actor.fieldE4 = 0xabcdef12;
                        enemystrategy::ItemFadeAppearance fade;
                        fade.owner = &owner;
                        fade.update();
                        require(fade.state == 0 && fade.nextState == 1 && actor.fieldE0 == 0x12345600 &&
                            actor.fieldE4 == 0xabcdefff && actor.fieldE8 == 4,
                            "Item appearance hides the actor and starts the original four-unit alpha transition.");
                        fade.update();
                        require(fade.state == 1 && fade.nextState == 0xffff,
                            "Item appearance waits for actor alpha updates rather than advancing on elapsed time.");
                        actor.visibility(254, 255);
                        fade.update();
                        require(fade.nextState == 0xffff, "Item appearance requires alpha exactly 255.");
                        actor.visibility(255, 255);
                        fade.update();
                        require(fade.state == 1 && fade.nextState == 0x100,
                            "Item appearance schedules completion for the next strategy update.");
                        fade.update();
                        require(fade.state == 0x100 && fade.nextState == 0xffff,
                            "Completed fade appearance has no additional state callback.");
                        enemystrategy::ItemImmediateAppearance immediate;
                        immediate.update();
                        require(immediate.state == 0 && immediate.nextState == 0x100,
                            "Immediate appearance still follows the original deferred state transition.");
                        actor.fieldE0 = primary;
                        actor.fieldE4 = secondary;
                        actor.fieldE8 = speed;
                    }
                    {
                        mission::Resources resources;
                        mapcollision::State collision(resources);
                        actors::RotatingSurfaces surfaces;
                        std::array<float, 3> surface{1, 2, 3};
                        playerorientation::Quaternion quaternion{4, 5, 6, 7};
                        require(!surfaces.velocity(pose, {}, surface, quaternion) && surface == std::array<float, 3>{1, 2, 3} &&
                            quaternion == playerorientation::Quaternion{4, 5, 6, 7},
                            "Inactive rotating surfaces preserve both output arguments.");
                        surfaces.entries[0] = {{0, 200, 0}, 10, 16384};
                        require(!surfaces.velocity(pose, {10, -500, 0}, surface, quaternion),
                            "Rotating surface radius is exclusive and ignores vertical displacement.");
                        require(surfaces.velocity(pose, {5, -500, 0}, surface, quaternion) &&
                            std::abs(surface[0] - 5) < 0.001f && std::abs(surface[2] + 5) < 0.001f,
                            "Rotating surface displacement follows the original quaternion sign and center-relative vector.");
                        surfaces.entries[0].radius = 0;
                        const auto savedVelocity = actor.velocity;
                        const auto savedFlags = actor.flags;
                        const auto savedFirst = actor.fieldB0, savedSecond = actor.fieldB2;
                        actor.flags &= ~0x10000u;
                        actor.fieldB0 = actor.fieldB2 = -1;
                        actor.setVelocity({4, 10, 2});
                        enemystrategy::ItemPhysics physics(collision, surfaces, pose);
                        physics.owner = &owner;
                        physics.parameters.setBits(1, 0);
                        physics.parameters.setBits(4, 0);
                        physics.update();
                        require(actor.velocity == std::array<float, 3>{4, 10, 2} && physics.nextState == 1,
                            "Item physics first captures launch velocity without integrating it.");
                        physics.update();
                        require(actor.velocity == std::array<float, 3>{8, 17, 4},
                            "Airborne item physics adds retained launch acceleration before decaying it.");
                        physics.update();
                        require(actor.velocity == std::array<float, 3>{10, 19, 5},
                            "Item launch acceleration decays on each original physics update.");
                        actor.flags |= 0x10000;
                        physics.update();
                        require(physics.state == 1 && physics.nextState == 2 && actor.velocity == std::array<float, 3>{11, 18.5f, 5.5f},
                            "Ground contact completes the airborne update before entering grounded state.");
                        physics.update();
                        require(actor.velocity == std::array<float, 3>{11, 15.5f, 5.5f},
                            "Grounded physics excludes retained launch acceleration.");
                        actor.flags &= ~0x10000u;
                        physics.update();
                        require(physics.nextState == 1 && actor.velocity == std::array<float, 3>{11, 15.5f, 5.5f},
                            "Leaving ground schedules airborne state without integrating that update.");
                        actor.setVelocity(savedVelocity);
                        actor.flags = savedFlags;
                        actor.fieldB0 = savedFirst;
                        actor.fieldB2 = savedSecond;
                    }
                    {
                        mission::Resources resources;
                        gameflags::State flags;
                        enemystrategy::Factory factory(concrete, flags, resources);
                        auto condition = factory.construct(glme01::visibleFlagConditionFactory);
                        condition->owner = &owner;
                        concrete.visible = true;
                        condition->update();
                        require(condition->nextState == 0xffff, "Visible-room condition waits for original flag 5B.");
                        flags.setOn(0x5b);
                        concrete.visible = false;
                        condition->update();
                        require(condition->nextState == 0xffff, "Flag 5B does not bypass room visibility.");
                        concrete.visible = true;
                        condition->update();
                        require(condition->nextState == 0x100, "Original visible-room flag condition completes only with both dependencies.");
                        concrete.visible = false;
                    }
                    {
                        mission::Resources resources;
                        characterowner::Owner pathOwner(characterowner::Kind::Ordinary, strategies, ownerRuntime);
                        pathOwner.attach(actor, parent);
                        enemystrategy::PathCondition condition(resources);
                        condition.owner = &pathOwner;
                        condition.update();
                        require(condition.nextState == 0xffff, "Missing path resources leave proximity conditions pending.");
                        std::vector<std::uint8_t> emptyPath(16);
                        word(emptyPath, 8, 16);
                        require(pathOwner.path().attach(emptyPath, error), error);
                        const auto position = actor.position;
                        actor.position = {0, 0, 0};
                        condition.behavior();
                        require(condition.nextState == 0xffff, "Empty paths preserve the original false result for nearby player zero.");
                        actor.position = {200, 0, 0};
                        condition.behavior();
                        require(condition.nextState == 0x100, "Empty paths preserve the original true result when no player is in range.");
                        condition.nextState = 0xffff;
                        std::vector<std::uint8_t> points(64);
                        word(points, 0, 1); word(points, 4, 3); word(points, 8, 52); word(points, 12, 12);
                        const char* names[]{"pnt0_x", "pnt0_y", "pnt0_z"};
                        for (unsigned i = 0; i < 3; ++i) {
                            word(points, 16 + i * 12, hash(names[i]));
                            word(points, 20 + i * 12, 0xffffffff);
                            word(points, 24 + i * 12, i * 4 << 16);
                        }
                        require(pathOwner.path().attach(points, error), error);
                        condition.behavior();
                        require(condition.nextState == 0x100, "Nonempty paths accept nearby player zero and use original pnt0 fields.");
                        word(points, 16, hash("missing"));
                        require(pathOwner.path().attach(points, error), error);
                        condition.nextState = 0xffff;
                        condition.behavior();
                        require(condition.nextState == 0xffff, "Missing path coordinates retain the original negative sentinel.");
                        require(!pathOwner.loadPath(resources) && pathOwner.path().attached(),
                            "Absent path-name fields retain the previously attached owner path.");
                        actor.position = position;
                    }
                    {
                        const auto flags = actor.flags;
                        owner.state().field8d0 = 0x80;
                        owner.state().field8cc = 2;
                        actor.flags = 0x60000;
                        owner.updateFlagTimer();
                        require(owner.state().field8d0 == 0x81 && owner.state().field8cc == 2,
                            "Immediate owner flags take precedence over the countdown and preserve other flag bits.");
                        actor.flags = 0x20000;
                        owner.state().field8d0 = 0x80;
                        owner.updateFlagTimer();
                        require(owner.state().field8cc == 1 && owner.state().field8d0 == 0x80,
                            "Owner countdown does not fire before its original zero transition.");
                        owner.updateFlagTimer();
                        owner.updateFlagTimer();
                        require(owner.state().field8cc == 0 && owner.state().field8d0 == 0x81,
                            "Owner countdown reaches zero once without unsigned underflow.");
                        owner.state().fields908 = {0, 1, 0x80000000, 0xffffffff, 7};
                        owner.updateEventTimers();
                        require(owner.state().fields908 == std::array<std::uint32_t, 5>{0, 0, 0x80000000, 0xffffffff, 6},
                            "Original event clocks decrement only signed-positive values.");
                        actor.flags = flags;
                        modeleffects::Owner effect({});
                        effect.model = std::make_unique<j3dmodel::ModelInstance>(std::make_shared<j3dmodel::ModelData>(), 0);
                        effect.flags = 3;
                        owner.state().field894 = &effect;
                        owner.state().field898 = actor.animationState;
                        const auto animationSpeed = actor.animationSpeed;
                        actor.animationSpeed = 0;
                        owner.updateAnimationEffect();
                        require(effect.flags == 5 && effect.model->matrix ==
                            dooranimation::concatenate(actor.matrix, actor.model->matrix(0, 0)),
                            "Animation effects receive the current root matrix before the zero-speed pause.");
                        const auto matrix = effect.model->matrix;
                        actor.matrix[3] += 9;
                        actor.animationSpeed = -1;
                        owner.updateAnimationEffect();
                        require(effect.flags == 7 && effect.model->matrix == matrix,
                            "A paused model effect retains its matrix for the first resume tick, preserving original call order.");
                        owner.updateAnimationEffect();
                        require(effect.model->matrix != matrix, "A resumed effect accepts its matrix on the next tick.");
                        actor.matrix[3] -= 9;
                        actor.animationSpeed = animationSpeed;
                        owner.state().field898 ^= 1;
                        owner.updateAnimationEffect();
                        require(!owner.state().field894 && owner.state().field898 == 0xffffffff && effect.flags == 14,
                            "Animation changes release the effect and clear the original owner link and selector.");
                        particles::Emitter emitter;
                        emitter.flags = 0x10e;
                        emitter.setRunning(false);
                        require(emitter.flags == 0x108, "Particle pause disables both emission and simulation while retaining other flags.");
                        emitter.setRunning(true);
                        require(emitter.flags == 0x10e, "Particle resume restores both original update bits.");
                    }
                    enemystrategy::NoMove noMove;
                    noMove.owner = &owner;
                    actor.setVelocity({3, 4, 0});
                    require(actor.speed == 5 && actor.direction[0] == 0.6f, "Actor velocity setter derives speed and direction.");
                    noMove.update();
                    require(actor.speed == 0 && actor.velocity == std::array<float, 3>{} && actor.direction == actor.velocity,
                        "Recovered EnNoMove behavior stops the owner's concrete actor through the original velocity setter.");
                    auto& replacement = owner.state().embeddedStrategy;
                    replacement.owner = &owner;
                    actor.visibility(255, 255);
                    replacement.update();
                    require((actor.flags & 0xc400) == 0xc400 && (actor.fieldE0 & 255) == 0 &&
                        (actor.fieldE4 & 255) == 0 && replacement.frames == 1 && replacement.nextState == 0xffff,
                        "Embedded EnReplace initializes invisibility and inactive flags before its first behavior tick.");
                    while (replacement.frames < 60) replacement.update();
                    require(replacement.nextState == 0xffff, "EnReplace waits through frame 59.");
                    const auto events = ownerRuntime.calls.size();
                    replacement.update();
                    require(replacement.nextState == 1 && ownerRuntime.calls.size() == events + 1 &&
                        ownerRuntime.calls.back() == 14,
                        "EnReplace requests removal when source placement is absent, then schedules state one at frame 60.");
                    concrete.visible = true;
                    replacement.update();
                    require(replacement.state == 1 && replacement.nextState == 0xffff,
                        "Visible replacement near the player remains in state one.");
                    concrete.visible = false;
                    replacement.update();
                    require(replacement.nextState == 0x100, "Replacement finishes when its resolved room is no longer visible.");
                    replacement.update();
                    require(replacement.state == 0x100 && replacement.frames == 1,
                        "Terminal replacement state retains the base strategy clock without further behavior.");
                    owner.ownership().strategy = std::make_unique<enemystrategy::NoMove>();
                    owner.ownership().strategy->owner = &owner;
                    owner.ownership().strategy->context = &parent;
                    require(owner.beginReplacement() && !owner.beginReplacement() && replacement.owner == &owner &&
                        replacement.context == &parent && replacement.nextState == 0,
                        "Replacement activation copies primary strategy context and rejects duplicate activation.");
                    replacement.setNextState(0x100);
                    owner.ownership().strategy->setNextState(0x103);
                    owner.updateStrategy();
                    require(!owner.state().field8ea && owner.ownership().strategy->nextState == 0,
                        "Embedded replacement completion schedules the primary strategy's original restart state.");
                }
                {
                    playeractor::Globals playerGlobals;
                    playeractor::State state(actor, playerGlobals, pose);
                    j3dmodel::MaterialDefaults defaults;
                    require(defaults.read(dol, error), error);
                    playerinput::WindowsMotorOutput output;
                    playerinput::MotorState motors(output);
                    players::Globals sharedGlobals;
                    players::Manager manager(common, defaults, 2, nullptr, sharedGlobals, motors);
                    manager.initializeEntries();
                    gameplaycamera::Owners cameras;
                    unsigned singleAttachment;
                    {
                        playeractor::Components components(dol, actor, state, manager, sharedGlobals, *cameras.cameras[0]);
                        singleAttachment = components.single->parameters[0];
                        require(components.scanner->camera == cameras.cameras[0].get() &&
                            components.quaternion->quaternion == state.quaternion && components.bits[4]->width == 2 &&
                            !components.bits[0]->data && components.bits[0]->count == 0x180,
                            "Player's complete component ownership retains original parent resources and buffer sizes.");
                        require(actor.model->attachment(singleAttachment).object == &components.single->effect,
                            "Assembled Player components attach to the concrete MoveActor model.");
                    }
                    require(!actor.model->attachment(singleAttachment).object,
                        "Player component teardown releases its model links before the base actor is destroyed.");
                    std::vector<std::uint32_t> roomFlags(74);
                    playerrecord::Record saved;
                    saved.fill(0xa5);
                    actors::Registry registry(dol, construction, manager, sharedGlobals, *cameras.cameras[0],
                        playerGlobals, collisionCatalog, roomFlags, 2, false);
                    auto& player = dynamic_cast<playeractor::Player&>(registry.create(0, 130));
                    require(player.life == 100 && !player.fieldE8 && player.view.position == &player.position &&
                        player.view.quaternion == &player.state.quaternion && player.view.life == 100 &&
                        player.view.field85E == 0 && player.view.field8D0 == 0xffff && !player.view.field8B8 &&
                        player.view.volume == &collisionCatalog.type(0),
                        "Concrete Player construction connects the original state view after all components.");
                    auto& alternate = dynamic_cast<playeractor::Alternate&>(registry.create(1, 130));
                    require(alternate.state.field100C == 1 && (alternate.components->controls->settings.flags & 4) &&
                        alternate.field13BC == std::array<float, 3>{0, 0, 1},
                        "Original alternate Player constructor adds its state after base construction.");
                    unsigned registered = 2;
                    for (const auto& pool : creation.pools()) {
                        for (unsigned i = 0; i < pool.count; ++i) {
                            auto& pooled = registry.create(pool.type, pool.capacity);
                            require(pooled.actorId == registered++ && registry.actor(pooled.actorId) == &pooled,
                                "Concrete registry retains original pool IDs and stable actor identity.");
                        }
                    }
                    require(registry.count() == 105, "Two concrete Players precede all original pooled actors.");
                    enemies::Actor& placed = player;
                    placed.setPosition({-7.640747f, 0, 145.1737f});
                    placed.setRotation({0, 0x2000, 0});
                    placed.visibility(17, 29);
                    require(player.position == player.previousPosition && player.position == player.field5C &&
                        player.fieldB4 == rooms.resolve(player.position, 0) &&
                        player.components->quaternion->quaternion == player.state.quaternion &&
                        player.rotation == player.previousRotation && (player.fieldE0 & 255) == 17 && (player.fieldE4 & 255) == 29,
                        "EnManager actor interface follows original placement, room resolution and Player rotation override.");
                    require(player.components->attached->actor == &player && player.components->single->actor == &player,
                        "Player attachments retain their owning actor rather than only its model.");
                    std::array<std::uint8_t, 48> playerBits{};
                    std::array<std::uint8_t, 8> countBits{};
                    playerBits[0x17] = 0xb0;
                    playerBits[0x18] = 1;
                    playerBits[0xe] = 0xa0;
                    countBits[0] = 3;
                    countBits[7] = 0xff;
                    player.components->bits[0]->data = playerBits.data();
                    player.components->bits[2]->data = countBits.data();
                    player.state.entryCount = 3;
                    player.state.entries[0] = 4;
                    player.state.entries[1] = 8;
                    player.state.entries[2] = 4;
                    player.components->reactions->state = 2;
                    player.components->reactions->field8 = alternate.actorId;
                    player.state.field1180 = 5;
                    player.fieldB4 = 0;
                    player.refreshView();
                    require(player.view.field880 == 6 && player.view.counts[4] == 2 && player.view.counts[8] == 1 &&
                        player.view.flagsF04 == std::array<std::uint8_t, 5>{1, 1, 1, 0, 1} &&
                        player.view.flagsF12 == std::array<std::uint8_t, 3>{1, 1, 0} &&
                        player.view.field890 == &alternate.position && player.view.field8B8 == player.components->light.get(),
                        "Player view counts only sixty flags, maps original flag bits and resolves registry references.");
                    roomFlags[0] = 0x8000;
                    alternate.flags |= 0xc000;
                    player.refreshView();
                    require(!player.view.field890 && !player.view.field8B8,
                        "Inactive reaction actors and the original room light-suppression flag remain effective.");
                    auto& child = *registry.actor(2);
                    const auto originalPosition = child.position;
                    player.model->matrix(0, 0) = {2, 0, 0, 10, 0, 3, 0, 20, 0, 0, 4, 30};
                    require(player.attachChild(child, 0, nullptr, 1) && child.parent == &player &&
                        !alternate.attachChild(child, 0, nullptr, 1) && !child.attachChild(alternate, 0, nullptr, 1),
                        "Original child ownership rejects an existing parent and nested attachments.");
                    require(player.detachChild(child) && !child.parent && !player.children[0].actor &&
                        child.scale == std::array<float, 3>{3, 3, 3} && child.rotation == std::array<std::int16_t, 3>{} &&
                        child.position == originalPosition && !player.detachChild(child),
                        "Original detach computes uniform scale and angles while leaving position untouched.");
                    player.components->bits[0]->data = nullptr;
                    player.components->bits[2]->data = nullptr;
                    player.state.record = &saved;
                    saved[0] = 0xff;
                    saved[1] = 0xff;
                    saved[2] = 0;
                    saved[3] = 100;
                    saved[0x16a] = 2;
                    saved[0xd0] = saved[0xd1] = saved[0xd2] = 0;
                    saved[0xd3] = 1;
                    player.state.flags792 = 0x17;
                    std::int32_t audioLife = -9;
                    require(player.loadRecord(audioLife) && player.life == 1 && audioLife == 1 &&
                        player.state.maximumLife == 100 && player.state.flags792 == 0x1b &&
                        player.state.entries[63] == 0xa5a5 && player.state.fields12F4[11] == 0xa5a5 &&
                        player.components->flags->data == saved.data() + 0x144 &&
                        player.components->bits[4]->data == saved.data() + 0x164 &&
                        player.components->bits[4]->width == 2 && player.components->bits[4]->count == 24 &&
                        player.components->controls->mode == 1 && player.components->controls->settings.fields[0x25] == 1,
                        "Player record loading preserves signed health, original flag masks and borrowed bit storage.");
                    player.state.record = nullptr;
                    audioLife = 29;
                    require(!player.loadRecord(audioLife) && audioLife == 29 &&
                        player.components->flags->data == saved.data() + 0x144,
                        "Null record loading preserves existing bindings and audio health.");
                    player.components->controls->setMode(7);
                    require(player.components->controls->mode == 7 && player.components->controls->settings.fields[0x25] == 1,
                        "Unsupported original control modes retain their mapping while storing the mode.");
                    player.state.record = &saved;
                    alternate.state.record = &saved;
                    {
                        enemies::Manager parent(types, creation, concrete, 1, 2);
                        characterowner::Owner owner(characterowner::Kind::Null, strategies, ownerRuntime);
                        owner.attach(player, parent);
                        saved[0xb4] = saved[0xb5] = saved[0xb6] = saved[0xb7] = 0;
                        player.bind({0, &saved, 0x13, nullptr, nullptr, nullptr, nullptr, nullptr, &child}, owner, audioLife);
                        require(player.state.field790 == 1 && player.state.fieldF34 == 0 && player.view.life == 1 &&
                            owner.state().life == 1 && child.parent == &player && player.children[0].joint == 0x24,
                            "Original Player binding attaches equipment, loads the record, refreshes the view and notifies its owner.");
                        owner.setLife(0xffffffff);
                        require(player.life == 0xffff && owner.state().life == 0xffffffff,
                            "Owner life events preserve the full value independently of the actor's halfword.");
                    }
                    player.life = 31;
                    alternate.life = 47;
                    alternate.components->controls->mode = 2;
                    alternate.state.entries[0] = 0x12345;
                    alternate.state.entryCount = 1;
                    registry.destroyActors();
                    require(saved[1] == 47 && saved[0x32] == 0x23 && saved[0x33] == 0x45 && saved[0xd3] == 2 &&
                        saved[0xd0] == 0 && saved[4] == 0xa5 && registry.count() == 105,
                        "Registry destruction runs Player writeback in forward order and retains the original index.");
                    registry.resetIndex();
                    require(registry.count() == 0 && registry.create(0, 130).actorId == 0,
                        "Explicit registry reset starts the next original actor lifecycle.");
                    auto& third = dynamic_cast<playeractor::Alternate&>(registry.create(0xb3, 130));
                    require(third.model->attachment(2).object == &third.effect && (third.effect.flags & 0x20) &&
                        third.effect.field28 == std::array<float, 3>{0.7f, 1, 1},
                        "Type B3 retains the original alternate Player attachment override.");
                    {
                        actors::Registry boundRegistry(dol, construction, manager, sharedGlobals, *cameras.cameras[0],
                            playerGlobals, collisionCatalog, roomFlags, 2, false);
                        modeleffects::Factory ownerModels;
                        gamearchive::Resources ownerArchive(common);
                        particles::EmitterPool ownerEmitters(0, 0, 0);
                        particles::EffectFactory ownerParticles(dol, ownerArchive, ownerEmitters);
                        unsigned ownerVariant = 0;
                        charactereffects::Resources ownerEffects(ownerModels, ownerParticles, ownerVariant);
                        playerrecord::Record firstRecord{}, secondRecord{};
                        roomlighting::Pool ownerLights;
                        jaudio::PositionOwners sounds;
                        BindingOwnerRuntime bindingOwners(ownerEffects, creationProperties, firstRecord, ownerLights, sounds);
                        roomlocation::Rooms rooms(dol, 2);
                        roomtraversal::State visibility;
                        visibility.initialize(static_cast<unsigned>(rooms.entries().size()));
                        gameflags::State flags;
                        unsigned mapMode = 2;
                        std::uint32_t randomState = 0;
                        enemies::ManagerRuntime bindingRuntime(strategies, bindingOwners, boundRegistry, rooms,
                            visibility, roomFlags, flags, mapMode, randomState);
                        enemies::Manager boundEnemies(types, creation, bindingRuntime, 2, 2);
                        std::vector<std::uint8_t> ownerCompressed, ownerMap;
                        mission::Resources ownerMission;
                        require(package.read("files/Map/map2.szp", ownerCompressed, error) &&
                            roomarchive::decompress(ownerCompressed, ownerMap, error) && ownerMission.read(ownerMap, error), error);
                        enemystrategy::Factory ownerStrategies(bindingRuntime, flags, ownerMission);
                        {
                            gamearchive::Archive serviceMap;
                            require(serviceMap.read(ownerMap, error), error);
                            jaudio::Resources serviceAudio;
                            require(jaudio::read(package, serviceAudio, error), error);
                            std::vector<std::uint8_t> initialFlags;
                            require(dolresources::read(dol, glme01::initialSaveFlags, 31, initialFlags, error), error);
                            gamesave::State selectedSave;
                            require(gamesave::initialize(initialFlags, selectedSave, error), error);
                            gameentry::State entry;
                            require(gameentry::selectNewFile(selectedSave, 0, entry, error), error);
                            playerrecord::Record secondRecord = entry.player;
                            playerinput::Pad servicePad;
                            gameflags::State serviceFlags;
                            serviceFlags.restore(entry.flags);
                            {
                                auto completedEntry = entry;
                                completedEntry.inventory[0x20 + 48] = 1;
                                gameevent::MissionActivation completedEvents(ownerMission, types, completedEntry, serviceFlags);
                                require(completedEvents.activation().value(48) == 0,
                                    "Original EventInfo load limits read the selected save's event-completion byte.");
                                auto hiddenEntry = entry;
                                hiddenEntry.flags[0] |= 1u << 3;
                                gameflags::State hiddenFlags;
                                hiddenFlags.restore(hiddenEntry.flags);
                                gameevent::MissionActivation hiddenEvents(ownerMission, types, hiddenEntry, hiddenFlags);
                                require(hiddenEvents.activation().value(48) == 0 && hiddenEvents.activation().value(7) == 0,
                                    "Restored disappearance flags disable the old event before the new appearance update.");
                                hiddenEvents.updateAppearance(0);
                                require(hiddenEvents.activation().value(7) == 1,
                                    "Original event seven is activated from the restored mission flag through its real EventInfo record.");
                            }
                            std::uint32_t serviceRandom = 0;
                            std::int32_t serviceLife = 0;
                            std::shared_ptr<jaudio::PositionalPlayback> retainedPlayback;
                            gameplay::ManagerDependencies dependencies{dol, package, serviceAudio, common, serviceMap, ownerMission,
                                defaults, construction, collisionCatalog, types, creation, strategies, *cameras.cameras[0],
                                roomFlags, visibility, entry, serviceFlags, motors, servicePad, {&entry.player, &secondRecord}, serviceRandom, serviceLife};
                            for (unsigned load = 0; load < 2; ++load) {
                                {
                                    gameplay::ManagerServices services(dependencies);
                                    auto& sceneEnemies = services.enemies();
                                    auto& sceneActors = services.actors();
                                    auto& scenePlayer = dynamic_cast<playeractor::Player&>(*sceneActors.actor(sceneEnemies.appearPoint(0).actor));
                                    require(sceneEnemies.actorsInitialized() && sceneActors.count() == 105 &&
                                        scenePlayer.state.record == &entry.player && scenePlayer.state.field7A0 == services.players().controllers[0].get(),
                                        "Production service assembly binds real player records, controllers and all original actor slots.");
                                    require(services.modelEffects().registered().size() == 44 && services.players().entries &&
                                        services.players().auxiliary && services.particles().pool().emitters.free.count == 98 &&
                                        services.particles().contexts().player.actorPosition == scenePlayer.view.position,
                                        "Production effect managers share native player, elemental and particle storage.");
                                    require(services.playerEvents().send(0, 0x12) == 1 &&
                                        services.players().controllers[0]->feedback.pulses.slots[0].active,
                                        "Production player events reach the manager-owned controller.");
                                    require(!serviceFlags.value(3) && !serviceFlags.value(4) &&
                                        services.missionEvents().conditions().size() == 95 && services.missionEvents().activation().value(48) == 1 &&
                                        services.missionEvents().activation().value(7) == 0,
                                        "Fresh-save flags and original EventInfo configure foyer events without test overrides.");
                                    require(services.placements().size() == 0,
                                        "Placement dispatch waits for the scene's activation step after manager construction.");
                                    services.updatePlacement(0);
                                    const auto initialPlacements = services.placements().size();
                                    std::cout << "Fresh mission placements: " << initialPlacements << '\n';
                                    const auto* actualCharacters = ownerMission.getJmpResource("CharacterInfo");
                                    require(services.placements().find({actualCharacters, 38}) >= 0 &&
                                        services.placements().find({actualCharacters, 50}) >= 0 &&
                                        services.placements().find({actualCharacters, 21}) < 0 &&
                                        services.placements().find({actualCharacters, 22}) < 0,
                                        "Full fresh-save activation preserves original player and demo-event gates.");
                                    actormodel::Matrix retained{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
                                    auto expectedRandom = serviceRandom;
                                    services.updateCaptures(nullptr, nullptr, retained);
                                    require(serviceRandom == expectedRandom,
                                        "Assembled inactive capture uses no retained-state allocation or random draws.");
                                    for (const auto* actor : sceneActors.entries()) {
                                        if (actor) require(!actor->captureState(), "Initial actors retain no invented capture state.");
                                    }
                                    for (unsigned frame = 0; frame < 120; ++frame) {
                                        services.updatePlacement(static_cast<std::int32_t>(frame / 30));
                                        services.updateGenerators();
                                        services.glows().update();
                                        services.updateCaptures(nullptr, nullptr, retained);
                                        for (unsigned slot = 0; slot < sceneEnemies.slots().size(); ++slot) services.updateOwner(slot);
                                        services.particles().update();
                                        services.particles().refresh(nullptr);
                                        services.particles().batch(0).update();
                                        services.particles().updateElements();
                                        services.modelEffects().update(construction.rooms, visibility);
                                        services.updateAudio(retained);
                                    }
                                    require(services.placements().size() == initialPlacements,
                                        "Repeated native placement scans do not duplicate admitted mission entries.");
                                    unsigned activeGenerators = 0;
                                    unsigned bats = 0, fires = 0, rats = 0;
                                    for (const auto& generator : services.generators().generators) {
                                        if (!generator->strategy) continue;
                                        ++activeGenerators;
                                        require(generator->count == 0, "Adjacent-room generators retain their original first-area creation gates.");
                                        if (auto* bat = dynamic_cast<enemystrategy::BatGenerator*>(generator->strategy.get())) {
                                            ++bats;
                                            require(bat->state == 2 && bat->radius == 200 && bat->initialDelay == 0,
                                                "Both original bat generators retain their waiting state and placement radius.");
                                        }
                                        if (auto* fire = dynamic_cast<enemystrategy::FireGenerator*>(generator->strategy.get())) {
                                            ++fires;
                                            require(fire->state == 1 && fire->angle == 180 && fire->appearTime == 10,
                                                "The original candle generator reaches its ambient update state.");
                                        }
                                        if (dynamic_cast<enemystrategy::GoldRatGenerator*>(generator->strategy.get())) {
                                            ++rats;
                                            require(generator->strategy->state == 0x103, "The gold rat waits for its original start command.");
                                        }
                                    }
                                    require(activeGenerators == 4, "The real scan owns all four initially eligible adjacent-room generators.");
                                    require(bats == 2 && fires == 1 && rats == 1, "All four original generator factories are selected by the imported records.");
                                    unsigned barriers = 0, observers = 0, fakeDoors = 0;
                                    for (const auto& slot : sceneEnemies.slots()) {
                                        auto* owner = dynamic_cast<characterowner::Owner*>(slot.owner.get());
                                        if (!owner) continue;
                                        auto* strategy = owner->ownership().strategy.get();
                                        if (auto* barrier = dynamic_cast<enemystrategy::Barrier*>(strategy)) {
                                            ++barriers;
                                            const auto pair = services.doors().savedPair(static_cast<std::uint8_t>(barrier->doorId()));
                                            require(services.doors().eventBlocked(pair[0]) != 0,
                                                "The barrier locks its actual saved door through the original door manager.");
                                        }
                                        if (dynamic_cast<enemystrategy::RoomObserver*>(strategy)) {
                                            ++observers;
                                            require(strategy->state == 1, "Original observers wait on their real first-area conditions.");
                                        }
                                        if (dynamic_cast<enemystrategy::FakeDoor*>(strategy)) ++fakeDoors;
                                    }
                                    require(barriers == 1 && observers == 2 && fakeDoors == 1 && services.doors().size() == 57,
                                        "The full scan owns the barrier, fake door and both observers alongside the original door leaves.");
                                    retainedPlayback = services.playback();
                                    jaudio::PositionalSound* binding = nullptr;
                                    retainedPlayback->play(0x181e, scenePlayer.position, &binding);
                                    require(binding && binding->ownerPosition == &scenePlayer.position,
                                        "Assembled positional playback retains the real actor position identity.");
                                    jaudio::DspMixer mixer;
                                    require(mixer.configure(serviceAudio.effects, error), error);
                                    bool audible = false;
                                    for (unsigned block = 0; block < 200; ++block) {
                                        mixer.begin();
                                        require(retainedPlayback->mix(mixer, error), error);
                                        std::array<std::int16_t, 160> pcm;
                                        mixer.end(pcm, 0x7fff);
                                        audible |= std::any_of(pcm.begin(), pcm.end(), [](auto value) { return value != 0; });
                                    }
                                    require(audible, "The scene playback binding generates original effect PCM through the DSP mixer.");
                                    sceneEnemies.deactivate(scenePlayer.actorId);
                                    require(!binding, "EnManager actor release reaches the assembled positional sound owner.");
                                    retainedPlayback->play(0x181e, scenePlayer.position);
                                }
                                require(retainedPlayback->finished(), "Scene teardown stops audio before destroying referenced actor positions.");
                                retainedPlayback->update({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0});
                                retainedPlayback.reset();
                            }
                            auto invalid = dependencies;
                            invalid.records[0] = nullptr;
                            bool rejected = false;
                            try { gameplay::ManagerServices services(invalid); }
                            catch (const std::invalid_argument&) { rejected = true; }
                            require(rejected, "Service assembly rejects missing persistent records before creating player actors.");
                            std::cout << "Production manager/service assembly, updates, playback and teardown verified.\n";
                        }
                        {
                            unsigned itemGhostTypes = 0;
                            for (unsigned type = 0; type < 381; ++type) {
                                if (strategies.constructor(strategies.select(type, {})) != 0x800f02a4) continue;
                                ++itemGhostTypes;
                                const auto source = itemghost::findLoot(ownerMission, types, type);
                                require(source.table && source.row >= 0, "Original item ghost selectors resolve their named IyapooTable rows.");
                                const auto loot = itemghost::readLoot(source);
                                require(!loot.drops.empty(), "Original item ghost loot preserves its table fields.");
                            }
                            require(itemGhostTypes != 0, "The imported executable contains item ghost selectors.");
                        }
                        {
                            RuntimeProbe selection;
                            mission::Resources absent;
                            require(!characteritems::select(absent, 2, 30, selection).table && !selection.randomCalls,
                                "Missing item choice tables do not consume the shared random sequence.");
                            require(characteritems::adjustTable(2, 30) == 12 && characteritems::adjustTable(3, 30) == 13 &&
                                characteritems::adjustTable(2, 31) == 2 && characteritems::adjustTable(3, -1) == 13 &&
                                characteritems::adjustTable(4, 0) == 4, "Original low-life item remapping uses signed life and a strict 31 threshold.");
                            const auto* choices = ownerMission.getJmpResource("itemappeartable");
                            require(choices && choices->fieldCount(), "Original map2 supplies the item choice table.");
                            const auto* info = ownerMission.getJmpResource("iteminfotable");
                            require(info, "Original map2 supplies item name records.");
                            characteritems::Admission admission(dol, types);
                            require(admission.nothing({}) && !admission.money({}) && !admission.key({}) &&
                                admission.nothing(characteritems::find(ownerMission, "nothing")) &&
                                admission.money(characteritems::find(ownerMission, "money")),
                                "Original item admission distinguishes missing/nothing records from money records.");
                            std::array<std::uint8_t, 32> doorBits{};
                            std::array<std::uint8_t, 48> capturedBits{};
                            playercomponents::FlagBuffer doorFlags(doorBits.data());
                            playercomponents::BitBuffer capturedFlags(capturedBits.data(), 381, 48, 1);
                            playeractor::View player{};
                            player.flags = &doorFlags;
                            player.bits[0] = &capturedFlags;
                            unsigned keys = 0;
                            for (unsigned row = 0; row < info->count(); ++row) {
                                const jmp::ToolDataRef item{info, static_cast<int>(row)};
                                if (admission.nothing(item) || admission.money(item)) continue;
                                const auto type = admission.type(item);
                                if (admission.key(item)) {
                                    ++keys;
                                    require(admission.redirected(item), "Original key item types always use redirected spawning.");
                                    std::uint32_t door;
                                    if (info->getValue(row, "OpenDoorNo", door)) {
                                        require(!admission.blocked(item, player), "Unowned key is admitted.");
                                        doorBits[(door & 255) >> 3] |= 1u << (door & 7);
                                        require(admission.blocked(item, player), "Owned key uses the original eight-bit door number mask.");
                                        doorBits.fill(0);
                                    }
                                } else if (type < 381) {
                                    require(!admission.blocked(item, player), "Uncaptured item is admitted.");
                                    capturedBits[type >> 3] |= 1u << (type & 7);
                                    require(admission.blocked(item, player) == ((types.flags(type) & 1) != 0),
                                        "Captured-type admission applies only to original single-instance item flags.");
                                    capturedBits.fill(0);
                                }
                            }
                            require(keys != 0, "Imported item records exercise original key admission.");
                            for (unsigned row = 0; row < choices->count(); ++row) {
                                for (unsigned field = 0; field < choices->fieldCount(); ++field) {
                                    selection.randomValue = field;
                                    const auto before = selection.randomCalls;
                                    const auto item = characteritems::select(ownerMission, row, 100, selection);
                                    const auto expected = info->findEntryByValue(info->searchItemInfo("name"), choices->fieldString(row, field));
                                    require(selection.randomCalls == before + 1 && item.row == expected &&
                                        item.table == (expected < 0 ? nullptr : info),
                                        "Every original item choice resolves through the first named mission record and consumes one random value.");
                                }
                            }
                            const auto before = selection.randomCalls;
                            require(!characteritems::select(ownerMission, choices->count(), 100, selection).table && selection.randomCalls == before,
                                "Out-of-range item tables retain the original invalid reference without advancing randomness.");
                            require(characteritems::tableId({}, 0, construction.parameters) == construction.parameters.parameters(0).entries()[21].bits,
                                "Missing character placement falls back to the original normal-item parameter.");
                        }
                        bindingOwners.bind(boundEnemies, ownerStrategies);
                        ownerEffects.bind(boundEnemies, boundRegistry);
                        bindingRuntime.bind(boundEnemies);
                        boundEnemies.initializeActors();
                        {
                            roomdependencies::Catalog dependencies(dol, 2);
                            characterplacement::Runtime placementRuntime(boundEnemies, bindingRuntime, dependencies);
                            auto& firstPlayer = dynamic_cast<playeractor::Player&>(*boundRegistry.actor(boundEnemies.appearPoint(0).actor));
                            auto& secondPlayer = dynamic_cast<playeractor::Player&>(*boundRegistry.actor(boundEnemies.appearPoint(1).actor));
                            const auto firstRoom = firstPlayer.view.previousRoom;
                            const auto secondRoom = secondPlayer.view.previousRoom;
                            firstPlayer.view.previousRoom = 0x02000102;
                            secondPlayer.view.previousRoom = 0x02000102;
                            {
                                const auto randomBefore = randomState;
                                particles::Scene scene(dol, ownerArchive, randomState);
                                require(scene.pool().emitters.free.count == 128 && scene.pool().nodes.free.count == 1024 &&
                                    scene.pool().shortNodes.free.count == 1024, "Original gameplay particle manager capacities.");
                                bool rejected = false;
                                try { scene.createDamage(0, {}); } catch (const std::runtime_error&) { rejected = true; }
                                require(rejected && scene.pool().emitters.free.count == 128,
                                    "Damage creation cannot consume resources before scene context binding.");
                                scene.bind(boundEnemies, boundRegistry, pose, visibility);
                                scene.refresh(nullptr);
                                require(scene.contexts().player.actorPosition == firstPlayer.view.position &&
                                    scene.contexts().player.suction.position == firstPlayer.view.field8A4 &&
                                    scene.contexts().player.suction.direction == firstPlayer.view.field8A8 &&
                                    scene.contexts().player.room == 0x02000102,
                                    "Particle scene obtains borrowed player pointers through real EnManager ownership.");
                                scene.update();
                                require(randomState == randomBefore && scene.pool().emitters.free.count == 128 &&
                                    scene.pool().active[0].count == 0, "Empty first-area particle update consumes neither random values nor pool slots.");
                                auto* damage = scene.createDamage(0, {7, 8, 9});
                                require(damage && damage->fieldC8 && scene.effects().history[0] == 200 &&
                                    scene.pool().emitters.free.count == 127,
                                    "Scene damage service loads original resources and retains its owned suction callback.");
                                auto* callback = damage->fieldC8;
                                for (unsigned frame = 0; frame < 4; ++frame) {
                                    scene.refresh(nullptr);
                                    scene.update();
                                }
                                require(damage->fieldC8 == callback, "Scene refresh preserves callback identity during emitter simulation.");
                                damage->release();
                                for (unsigned frame = 0; frame < 1024 && scene.pool().emitters.free.count != 128; ++frame) scene.update();
                                require(scene.pool().emitters.free.count == 128 && scene.pool().nodes.free.count == 1024 &&
                                    scene.pool().shortNodes.free.count == 1024,
                                    "Original deferred emitter retirement returns scene particle storage to its pools.");
                                rejected = false;
                                try { scene.bind(boundEnemies, boundRegistry, pose, visibility); }
                                catch (const std::logic_error&) { rejected = true; }
                                require(rejected, "Rebinding cannot invalidate existing scene callback references.");
                                scene.initializeEffectBatches();
                                scene.initializeElements();
                                auto& elemental = scene.elemental();
                                require(elemental.pool.active.count == 9 && elemental.pool.emitters.free.count == 6 &&
                                    elemental.pool.particles.free.count == 1500 && !elemental.ice.active && !elemental.water.active,
                                    "Original first-area elemental initialization reserves nine emitters and inactive projectiles.");
                                const std::array<int, 9> types{0, 10, 9, 11, 2, 8, 8, 8, 8};
                                unsigned index = 0;
                                for (auto* emitter = elemental.pool.active.sentinel.next;
                                    emitter != &elemental.pool.active.sentinel; emitter = emitter->next, ++index) {
                                    require(emitter == &elemental.pool.emitters.at(14 - index) && emitter->type == types[index],
                                        "Elemental startup consumes the free-list tail and preserves original active order.");
                                    if (index == 2 || index == 4) require(emitter->room == 0x40ff013d &&
                                        emitter->position == std::array<float, 3>{1327, -470, -358},
                                        "Original basement emitter position and room resolution.");
                                    if (index >= 5) require(emitter->room == 0x49ff0546 &&
                                        emitter->direction == std::array<float, 3>{0, 1, 0},
                                        "Original boiler-room ambient emitters retain their upward axis.");
                                }
                                require(scene.pool().emitters.free.count == 98 && scene.pool().active[0].count == 30,
                                    "Original effect services reserve three ten-emitter batches from the scene's general pool.");
                                const auto visibilityBefore = visibility;
                                for (unsigned room : {0x02000102u, 0x1e01021du, 0x06000206u}) {
                                    std::uint32_t roomFlags = 0;
                                    std::uint16_t savedFlags = 0;
                                    visibility.mark(0, 2, room, roomFlags, savedFlags, 0, 0, 0, {}, {});
                                }
                                auto expectedRandom = randomState;
                                for (unsigned frame = 0; frame < 120; ++frame) {
                                    scene.update();
                                    for (unsigned i = 0; i < 30; ++i) expectedRandom = expectedRandom * 0x5eedf715u + 0x1b0cb173u;
                                    scene.batch(0).update();
                                    scene.refresh(nullptr);
                                    scene.updateElements();
                                }
                                require(randomState == expectedRandom, "Reserved general emitters preserve thirty original random draws per tick, even while empty.");
                                require(elemental.pool.active.count == 9 &&
                                    elemental.pool.particles.free.count == 1500 && scene.pool().emitters.free.count == 98 &&
                                    scene.pool().nodes.free.count == 1024 && scene.pool().shortNodes.free.count == 1024,
                                    "First-area effects retain their reserved pools over 120 original simulation ticks without spurious emission.");
                                scene.batch(1).cursor = 7;
                                scene.batch(1).entries()[0]->flags = 0xe;
                                scene.updateElements();
                                require(scene.batch(1).cursor == 0 && scene.batch(1).entries()[0]->flags == 0xc,
                                    "Elemental update resets the original batch cursor and emission flags before general simulation.");
                                elemental.paused = true;
                                const auto contextDistance = scene.contexts().player.distance;
                                scene.contexts().player.suction.strength = 2;
                                scene.updateElements();
                                require(scene.contexts().player.distance == contextDistance,
                                    "Elemental pause retains context scaling while still resetting its general batches.");
                                elemental.paused = false;
                                scene.updateElements();
                                require(scene.contexts().player.distance == 100,
                                    "Original static initialization supplies the elemental context's fifty-unit scale.");
                                visibility = visibilityBefore;
                                randomState = randomBefore;
                            }
                            {
                                const auto firstController = firstPlayer.state.field7A0;
                                const auto secondController = secondPlayer.state.field7A0;
                                firstPlayer.state.field7A0 = manager.controllers[0].get();
                                secondPlayer.state.field7A0 = manager.controllers[1].get();
                                auto& feedback = firstPlayer.state.field7A0->feedback;
                                feedback.reset();
                                secondPlayer.state.field7A0->feedback.reset();
                                playerevents::Dispatcher events(boundEnemies, boundRegistry);
                                require(events.send(0, 0x12) == 1 && feedback.pulses.slots[0].active &&
                                    feedback.pulses.slots[0].amplitude == 1 && feedback.pulses.slots[0].remaining == 1,
                                    "Null player-event payload uses the original single-frame feedback defaults.");
                                playerevents::Payload payload;
                                payload.word(0x50, 0xffffffff);
                                payload.word(0x54, 0x3f000000);
                                payload.word(0x58, 7);
                                payload.word(0x5c, 2);
                                feedback.reset();
                                require(events.send(0, 0x12, &payload) == 1 && feedback.pulses.slots[0].amplitude == 0.5f &&
                                    feedback.pulses.slots[0].remaining == 7 && feedback.pulses.slots[0].fade == 2,
                                    "Negative feedback kind uses the original explicit pulse branch and full payload fields.");
                                for (unsigned kind = 1; kind <= 3; ++kind) {
                                    feedback.reset();
                                    payload.word(0x50, kind);
                                    require(events.send(0, 0x12, &payload) == 1 && feedback.scripts.slots[0].active,
                                        "Player event feedback kinds select imported original controller scripts.");
                                    const auto actual = feedback.scripts.slots[0].script;
                                    feedback.reset();
                                    firstPlayer.playFeedback(0x11 + kind);
                                    require(feedback.scripts.slots[0].script.identity == actual.identity &&
                                        feedback.scripts.slots[0].script.commands == actual.commands,
                                        "Player event selects the exact original feedback preset.");
                                }
                                feedback.reset();
                                doors::State doorState;
                                doorState.setGlobalBarrier(true, events);
                                require(doorState.barrierValue() == 255 && feedback.scripts.slots[0].active &&
                                    !secondPlayer.state.field7A0->feedback.scripts.slots[0].active,
                                    "Door barrier constructs a native payload and routes feedback to EnManager player zero only.");
                                feedback.reset();
                                doorState.setGlobalBarrier(false, events);
                                require(doorState.barrierValue() == 0 && feedback.scripts.slots[0].active,
                                    "Disabling the original door barrier also sends feedback.");
                                require(events.send(0x100, 0x12) == 0 && events.send(0, 0) == 0 && events.send(0, 0x41) == 0,
                                    "Absent alternate targets and original unhandled message IDs return zero.");
                                playerevents::Dispatcher alternateEvents(boundEnemies, boundRegistry, &secondPlayer);
                                require(alternateEvents.send(0x100, 0x12) == 1 && secondPlayer.state.field7A0->feedback.pulses.slots[0].active,
                                    "Alternate target routing retains its separate player object.");
                                bool unsupported = false;
                                try { events.send(0, 2); } catch (const std::runtime_error&) { unsupported = true; }
                                require(unsupported, "Unported valid gameplay messages cannot silently succeed.");
                                feedback.reset();
                                secondPlayer.state.field7A0->feedback.reset();
                                firstPlayer.state.field7A0 = firstController;
                                secondPlayer.state.field7A0 = secondController;
                            }
                            jmp::MessageSender sender;
                            sender.addReceiver(&boundEnemies);
                            gameevent::ActivationState events;
                            gamearchive::Archive updateMap;
                            require(updateMap.read(ownerMap, error), error);
                            gamearchive::Resources updateResources(updateMap);
                            modeleffects::SharedPool updateShared;
                            updateShared.load(updateResources, defaults);
                            modeleffects::Factory updateModels;
                            updateModels.add(updateShared);
                            mapcollision::State updateCollision(ownerMission);
                            std::vector<std::uint8_t> updateCollisionBytes;
                            require(roomarchive::read(ownerMap, "col.mp", updateCollisionBytes, error), error);
                            updateCollision.load(updateCollisionBytes);
                            particles::Scene updateParticles(dol, ownerArchive, randomState);
                            updateParticles.bind(boundEnemies, boundRegistry, pose, visibility);
                            updateParticles.refresh(nullptr);
                            characterowner::UpdateServices ownerUpdates{modelCatalog, modelResources, pose, parameters,
                                creation, updateCollision, boundRegistry, manager, bindingRuntime, updateModels, updateParticles, {}};
                            bindingOwners.bindUpdates(&ownerUpdates);
                            jmp::EntryActivation placement(sender, placementRuntime, flags, events);
                            const auto* characters = ownerMission.getJmpResource("characterinfo");
                            placement.add(characters);
                            placement.update();
                            require(placement.entries().size() == 2, "Original first-area sender admits Luigi records without creating duplicate players.");
                            flags.setOn(3);
                            placement.update();
                            require(placement.entries().size() == 4, "Original demo flag admits the foyer's two ordinary actors.");
                            unsigned captureDraws = 0;
                            actorcapture::Motion captureMotion(dol, pose, [&] { ++captureDraws; return 0u; });
                            actormodel::Matrix captureTransform{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
                            boundRegistry.updateCaptures(captureMotion, nullptr, nullptr, captureTransform);
                            require(captureDraws == 0, "First-area actor capture stage does not consume random values while inactive.");
                            for (int row : {21, 22}) {
                                const jmp::ToolDataRef source{characters, row};
                                unsigned found = 0;
                                for (const auto& slot : boundEnemies.slots()) {
                                    if (slot.state != 2 || !jmp::sameEntry(slot.owner->source, source)) continue;
                                    ++found;
                                    auto& owner = dynamic_cast<characterowner::Owner&>(*slot.owner);
                                    require(dynamic_cast<enemystrategy::NoMove*>(owner.ownership().strategy.get()),
                                        "Foyer placements use the original EnNoMove selector through manager allocation.");
                                    auto& actor = dynamic_cast<actors::MoveActor&>(owner.actor());
                                    require(!actor.captureState(), "Inactive foyer actors require no fabricated retained capture bytes.");
                                    const auto beforeCapture = actor.flags;
                                    bindingOwners.bindUpdates(nullptr);
                                    bool rejected = false;
                                    try { boundEnemies.update(actor.id()); } catch (const std::runtime_error&) { rejected = true; }
                                    require(rejected && actor.flags == beforeCapture,
                                        "Missing production update services fail before mutating the owner.");
                                    bindingOwners.bindUpdates(&ownerUpdates);
                                    owner.state().field8d4 = 1;
                                    owner.state().field8d8 = actor.position;
                                    owner.state().field8d8[0] += 5;
                                    owner.state().field8e4 = 1;
                                    owner.state().fields908 = {3, 2, 1, 0, 0xffffffff};
                                    owner.state().field8cc = 2;
                                    actor.flags |= 0x20000;
                                    for (unsigned frame = 0; frame < 120; ++frame) {
                                        actor.setVelocity({1, 2, 3});
                                        boundEnemies.update(actor.id());
                                        require(actor.velocity == std::array<float, 3>{} && slot.state == 2,
                                            "EnManager runs owner motion before EnNoMove clears velocity on each native tick.");
                                    }
                                    require((actor.flags & ~0x20000u) == (beforeCapture & ~0x20000u) &&
                                        !owner.state().field91c && owner.state().field8cc == 0 && (owner.state().field8d0 & 1) &&
                                        owner.state().fields908 == std::array<std::uint32_t, 5>{0, 0, 0, 0, 0xffffffff},
                                        "Production updates preserve inactive capture and original flag and signed event timer behavior.");
                                    owner.state().field8d4 = 0;
                                    owner.state().field91c = true;
                                    owner.state().field920 = 1;
                                    owner.state().fields908[0] = 3;
                                    actor.setVelocity({1, 2, 3});
                                    boundEnemies.update(actor.id());
                                    require(slot.state == 2 && owner.state().field920 == 0 &&
                                        owner.state().fields908[0] == 2 && actor.velocity == std::array<float, 3>{1, 2, 3},
                                        "Pending release suppresses strategy updates and retains the original zero-timer tick.");
                                    boundEnemies.update(actor.id());
                                    require(slot.state == 1 && !owner.state().field91c && owner.state().fields908[0] == 1,
                                        "Release underflow returns the actor to its pool before the remaining owner timers run.");
                                    boundEnemies.update(actor.id());
                                    require(owner.state().fields908[0] == 1,
                                        "EnManager does not update dormant pooled owners.");
                                }
                                require(found == 1, "Each foyer placement owns exactly one active manager slot.");
                            }
                            flags.setOn(4);
                            placement.update();
                            require(placement.entries().size() == 2, "Original disappearance flag removes foyer demo entries.");
                            for (const auto& slot : boundEnemies.slots()) {
                                if (slot.state == 2 && slot.owner) require(slot.owner->actor().type() == 0,
                                    "Foyer removal returns both ordinary owners to their original pools.");
                            }
                            flags.setOff(4);
                            placement.update();
                            require(placement.entries().size() == 4, "Foyer placements reactivate after their removal condition clears.");
                            unsigned reloaded = 0;
                            for (const auto& slot : boundEnemies.slots()) {
                                if (slot.state != 2 || slot.owner->actor().type() == 0) continue;
                                boundEnemies.update(slot.owner->actor().id());
                                ++reloaded;
                            }
                            require(reloaded == 2, "Both reused foyer owners execute through the production manager route.");
                            flags.setOn(4);
                            placement.update();
                            placement.remove(characters);
                            bindingOwners.bindUpdates(nullptr);
                            flags.setOff(3);
                            flags.setOff(4);
                            firstPlayer.view.previousRoom = firstRoom;
                            secondPlayer.view.previousRoom = secondRoom;
                        }
                        {
                            playermodels::VacuumModel volume(*manager.models, 3, 0);
                            volume.parameters.field20 = 100;
                            volume.fieldB4 = 0.8f;
                            volume.segments[0].field18 = 0;
                            volume.segments[0].field1C = 10;
                            volume.segments[1].field18 = 0;
                            volume.segments[1].field1C = 10;
                            std::array<float, 3> direction{7, 8, 9};
                            require(!volume.intersects(0, direction, {100, 0, 0}) && direction == std::array<float, 3>{7, 8, 9},
                                "Vacuum outer range rejects its exact boundary without modifying output.");
                            require(volume.intersects(0, direction, {0, 0, 5}) && direction == std::array<float, 3>{0, 0, -1},
                                "Original vacuum segment returns a direction toward the segment origin.");
                            direction = {7, 8, 9};
                            require(!volume.intersects(0, direction, {8, 0, 5}) && direction == std::array<float, 3>{7, 8, 9},
                                "Vacuum cone-angle rejection preserves the caller's direction.");
                            require(!volume.intersectsSegment(0, direction, {0, 0, 10}, 0),
                                "Segment projection excludes its upper bound.");
                            volume.segments[0].field18 = 5;
                            require(volume.intersectsSegment(0, direction, {0, 0, 5}, 0),
                                "Segment projection includes its lower bound.");
                            require(volume.intersects(5, direction, {3, 0, 0}) && direction == std::array<float, 3>{-1, 0, 0} &&
                                !volume.intersects(3, direction, {3, 0, 0}),
                                "Origin sphere uses a strict radius comparison before segment queries.");
                            volume.parameters.field4C = 1;
                            require(volume.intersects(0, direction, {}) && direction == std::array<float, 3>{},
                                "Spherical vacuum mode accepts coincident points with a zero response.");
                            require(!volume.intersects(0, direction, {0, 0, 100}),
                                "Spherical mode retains the outer range gate.");
                            volume.parameters.field4C = 0;
                            volume.segments[0].field18 = 0;
                            require(!volume.intersectsSegment(0, direction, {8, 0, 5}, 0) &&
                                volume.intersectsSegment(20, direction, {8, 0, 5}, 0),
                                "Nonzero query radius computes the original segment-dependent cone threshold.");
                            volume.field9C = 10;
                            volume.points[0].position = {};
                            volume.points[1].position = {0, 0, 10};
                            volume.points[2].position = {10, 0, 10};
                            volume.updateSegments();
                            require(volume.segments[0].position == std::array<float, 3>{} && volume.segments[0].field18 == 0 &&
                                volume.segments[0].field1C == 10 && volume.segments[1].position == std::array<float, 3>{-10, 0, 10} &&
                                volume.segments[1].fieldC == 1 && volume.segments[1].field14 == 0 &&
                                volume.segments[1].field18 == 10 && volume.segments[1].field1C == 20,
                                "Vacuum query segments use cumulative bounds and extrapolated origins rather than adjacent-point capsules.");
                            require(volume.intersects(0, direction, {5, 0, 10}) && direction == std::array<float, 3>{-1, 0, 0},
                                "Native segment construction feeds the original first-matching-volume query.");
                            volume.points[2].position = volume.points[1].position;
                            volume.updateSegments();
                            require(volume.segments[1].position == volume.points[2].position && volume.segments[1].fieldC == 0 &&
                                volume.segments[1].field10 == 0 && volume.segments[1].field14 == 0,
                                "Coincident vacuum points produce zero axes without division by zero.");
                        }
                        {
                            struct EventStrategy : enemystrategy::Strategy, characterevent::Observer {
                                unsigned calls = 0, queries = 0;
                                characterevent::Event* last = nullptr;
                                std::uint32_t receive(characterevent::Event& event) override {
                                    ++calls;
                                    last = &event;
                                    event.fields[1] = 0xabcdef;
                                    return 0x87654321;
                                }
                                bool query(enemies::Owner&) override { ++queries; return true; }
                            };
                            struct Capture : characterowner::CaptureServices {
                                std::vector<unsigned> calls;
                                std::uint16_t expectedType = 0x2345;
                                void initializeMotion(characterowner::Owner& owner, std::uint16_t type) override {
                                    require(type == expectedType && !owner.state().flags904[0], "Capture motion precedes the owner's capture flag.");
                                    calls.push_back(1);
                                }
                                void dropItem(characterowner::Owner& owner) override {
                                    require(owner.state().flags904[0], "Capture item creation follows the capture flag.");
                                    calls.push_back(2);
                                }
                                void notifyPlayer(characterowner::Owner& owner) override {
                                    require(!owner.state().field91c, "Capture player notification precedes delayed owner release.");
                                    calls.push_back(3);
                                }
                            } capture;
                            actors::MoveActor eventActor(construction, 127, 0, 130);
                            characterowner::Owner eventOwner(characterowner::Kind::Ordinary, strategies, bindingOwners);
                            eventOwner.attach(eventActor, boundEnemies);
                            eventOwner.ownership().strategy = std::make_unique<EventStrategy>();
                            auto& observer = static_cast<EventStrategy&>(*eventOwner.ownership().strategy);
                            observer.owner = &eventOwner;
                            eventActor.fieldE0 = 0x12340000;
                            require(eventOwner.event(0) == 1 && eventOwner.event(28) == 1 && eventOwner.event(5) == 0 &&
                                eventOwner.event(30) == 0 && !observer.calls,
                                "Owner constant and unsupported events retain their original return values without observer delivery.");
                            require(eventOwner.event(1) == 0 && eventOwner.event(27) == 0 && eventOwner.state().fields908[1] == 0,
                                "Owner alpha events inspect the low byte rather than the whole color word.");
                            eventActor.fieldE0 |= 1;
                            require(eventOwner.event(1) == 1 && eventOwner.event(27) == 1 && eventOwner.state().fields908[1] == 1,
                                "Nonzero alpha sets the original entry timer.");
                            characterevent::Event entered(2);
                            require(eventOwner.event(entered) == 0x87654321 && observer.last == &entered && entered.fields[1] == 0xabcdef &&
                                eventOwner.state().fields908[0] == 1,
                                "Native owner dispatch preserves the complete mutable event and full observer return value.");
                            characterevent::Event life(6);
                            life.fields[5] = 0xffffabcd;
                            require(eventOwner.event(life) == 1 && eventActor.life == 0xabcd && eventOwner.state().life == 0xffffabcd,
                                "Life event uses payload word six and retains the full owner value independently of the actor halfword.");
                            require(eventOwner.event(7) == 1 && eventOwner.state().life == creationProperties.life(0, firstRecord),
                                "Owner event seven reaches the native creation-life implementation.");
                            for (const auto message : {9u, 11u, 12u, 19u}) {
                                require(eventOwner.event(message) == 0x87654321, "Ordinary notification returns the untruncated observer result.");
                            }
                            for (const auto message : {13u, 14u, 15u, 16u, 17u, 18u}) {
                                eventOwner.state().fields908 = {};
                                const unsigned index = message % 3 == 1 ? 2 : message % 3 == 2 ? 4 : 3;
                                require(eventOwner.event(message) == 0x87654321 && eventOwner.state().fields908[index] == 1,
                                    "Element contact and projectile events set their original distinct timers before observer delivery.");
                            }
                            for (unsigned i = 0; i < 5; ++i) for (const auto timer : {0u, 1u, 0x7fffffffu, 0x80000000u, 0xffffffffu}) {
                                eventOwner.state().fields908[i] = timer;
                                require(eventOwner.event(21 + i) == (static_cast<std::int32_t>(timer) > 0),
                                    "Timer query events preserve signed-positive semantics including wrapped values.");
                            }
                            eventOwner.state().flags904[2] = true;
                            require(eventOwner.event(10) == 0x87654321 && eventOwner.event(20) == 1,
                                "Leaving-room event retains the dependent flag when no actor reference exists.");
                            std::array<std::uint8_t, 381> resourceMask{};
                            require(eventOwner.event(29, &resourceMask) == 0x87654321 && resourceMask[0] == 1,
                                "Resource event marks the actor type before forwarding the original mask identity.");
                            const auto before = observer.calls;
                            require(eventOwner.event(8) == 1 && eventOwner.event(8) == 0 && observer.calls == before + 1 &&
                                eventOwner.state().embeddedStrategy.owner == &eventOwner,
                                "Replacement starts once, copies strategy ownership, and ignores the observer's result.");
                            characterevent::Event captureBegin(3);
                            captureBegin.fields[0] = 0x12345;
                            bool rejected = false;
                            try { eventOwner.event(captureBegin); } catch (const std::runtime_error&) { rejected = true; }
                            require(rejected && !eventOwner.state().flags904[0],
                                "Unconnected capture dependencies fail before mutating the owner rather than simulating success.");
                            require(characterowner::dispatchEvent(eventOwner, captureBegin, 0, &capture) == 1 &&
                                capture.calls == std::vector<unsigned>{1, 2},
                                "Capture event preserves motion and item ordering and the original halfword capture type.");
                            characterevent::Event captureEnd(4);
                            captureEnd.fields[3] = 0xffffffff;
                            for (const auto kind : {0u, 1u, 2u, 3u, 4u, 5u, 0xffffffffu}) {
                                eventOwner.state().field91c = false;
                                captureEnd.fields[4] = kind;
                                const auto before = capture.calls.size();
                                require(characterowner::dispatchEvent(eventOwner, captureEnd, 0, &capture) == 1 &&
                                    eventOwner.state().field91c && eventOwner.state().field920 == 0xffffffff &&
                                    capture.calls.size() == before + (kind >= 2 && kind <= 4),
                                    "Capture completion notifies player zero only for signed kinds two through four and preserves release delay bits.");
                            }
                            const auto stopped = observer.calls;
                            require(eventOwner.event(12) == 0 && eventOwner.event(26) == 0 && observer.calls == stopped,
                                "Release-pending owners suppress collision and strategy queries.");
                            eventOwner.state().field91c = false;
                            characterevent::Event query(26);
                            for (const auto group : {2, 3}) {
                                require(characterowner::dispatchEvent(eventOwner, query, group, nullptr) == 0,
                                    "Special capture groups require a dependent actor reference.");
                                eventOwner.state().actorUsers[1] = &eventActor;
                                require(characterowner::dispatchEvent(eventOwner, query, group, nullptr) == 1 && observer.queries == 0,
                                    "Second dependent reference satisfies a capture query without strategy execution.");
                                eventOwner.state().actorUsers[1] = nullptr;
                            }
                            require(characterowner::dispatchEvent(eventOwner, query, 0, nullptr) == 1 && observer.queries == 1,
                                "Ordinary owner capture query delegates to the existing strategy.");
                            {
                                gamearchive::Archive map;
                                require(map.read(ownerMap, error), error);
                                gamearchive::Resources resources(map);
                                modeleffects::SharedPool shared;
                                shared.load(resources, defaults);
                                modeleffects::Factory factory;
                                factory.add(shared);
                                mapcollision::State collision(ownerMission);
                                std::vector<std::uint8_t> collisionBytes;
                                require(roomarchive::read(ownerMap, "col.mp", collisionBytes, error), error);
                                collision.load(collisionBytes);
                                playermodels::VacuumEntries entries(*manager.models, sharedGlobals.effects);
                                entries.entries[0]->state = 1;
                                entries.entries[0]->target = &eventActor;
                                entries.fieldA50[0] = &eventActor;
                                auto& record = entries.targetRecords[0];
                                record = {};
                                record[0] = 0xfffd1234;
                                RuntimeProbe random;
                                random.randomValue = 3;
                                eventOwner.state().flags904[0] = false;
                                const auto originalAnimation = eventActor.animationState;
                                eventActor.animationState = 0;
                                const auto observed = observer.calls;
                                characterowner::updateVacuum(eventOwner, entries, factory, construction.parameters, collision, modelCatalog, random);
                                auto* projected = eventOwner.ownership().sharedResource;
                                require(projected && eventOwner.ownership().sharedCount == 1 && eventOwner.state().captureRecord == &record &&
                                    eventActor.life == 0xfffd && observer.calls == observed + 1,
                                    "First target capture record acquires the original projected effect, sends event nine and copies the big-endian life halfword.");
                                characterowner::updateVacuum(eventOwner, entries, factory, construction.parameters, collision, modelCatalog, random);
                                require(eventOwner.ownership().sharedCount == 1 && observer.calls == observed + 1,
                                    "Retained capture-record identity suppresses duplicate acquisition and entry notification.");
                                entries.entries[0]->state = 0;
                                characterowner::updateVacuum(eventOwner, entries, factory, construction.parameters, collision, modelCatalog, random);
                                require(!eventOwner.state().captureRecord && !eventOwner.ownership().sharedResource &&
                                    !eventOwner.ownership().sharedCount && projected->attachmentState(),
                                    "Leaving all active target slots clears record identity and releases the capture projection.");
                                entries.entries[0]->state = 1;
                                for (const auto mode : {3u, 5u}) {
                                    eventOwner.state().captureRecord = nullptr;
                                    record[4] = mode;
                                    const auto before = observer.calls;
                                    characterowner::updateVacuum(eventOwner, entries, factory, construction.parameters, collision, modelCatalog, random);
                                    require(!eventOwner.ownership().sharedResource && observer.calls == before && eventOwner.state().captureRecord == &record,
                                        "Capture record modes three and five suppress shared acquisition and entry events.");
                                }
                                record[4] = 0;
                                eventOwner.state().captureRecord = nullptr;
                                characterowner::updateVacuum(eventOwner, entries, factory, construction.parameters, collision, modelCatalog, random);
                                eventOwner.ownership().sharedUsers[2] = eventOwner.acquireShared(factory, construction.parameters, collision);
                                require(eventOwner.ownership().sharedCount == 2, "Capture and visibility retain independent references to one projection.");
                                capture.expectedType = 1;
                                capture.calls.clear();
                                bindingOwners.bindCapture(&capture);
                                record[4] = 2;
                                characterowner::updateVacuum(eventOwner, entries, factory, construction.parameters, collision, modelCatalog, random);
                                bindingOwners.bindCapture(nullptr);
                                require(random.randomCalls == 1 && capture.calls == std::vector<unsigned>{1, 2} &&
                                    eventOwner.state().flags904[0] && !eventOwner.ownership().sharedCount &&
                                    !eventOwner.ownership().sharedUsers[1] && !eventOwner.ownership().sharedUsers[2],
                                    "Capture mode two enters native event three with one random low bit and then releases both projection references.");
                                characterowner::updateVacuum(eventOwner, entries, factory, construction.parameters, collision, modelCatalog, random);
                                require(random.randomCalls == 1, "Captured owner suppresses repeated motion activation and RNG consumption.");
                                unsigned captureSounds = 0;
                                auto captureSound = [&](unsigned, const std::array<float, 3>&, unsigned) { ++captureSounds; };
                                auto suction = [&] { characterowner::updateSuction(eventOwner, entries, boundRegistry, creation, captureSound); };
                                eventActor.flags = 0;
                                eventOwner.state().flags904[0] = false;
                                entries.entries[0]->blend.field4 = 99;
                                suction();
                                require(entries.entries[0]->blend.field4 == 0 && entries.entries[0]->blendChanged && !eventActor.flags,
                                    "Uncaptured target receives the default suction command without changing actor flags.");
                                for (const auto mode : {3u, 5u}) {
                                    record[4] = mode;
                                    eventOwner.state().flags904[1] = true;
                                    entries.entries[0]->blendChanged = 0;
                                    suction();
                                    require(!eventOwner.state().flags904[1] && !entries.entries[0]->blendChanged,
                                        "Excluded suction modes clear the pending flag without delivering a blend command.");
                                }
                                record[4] = 2;
                                eventOwner.state().flags904[0] = true;
                                eventOwner.state().flags904[1] = true;
                                eventOwner.state().captureRecord = &record;
                                suction();
                                require(entries.entries[0]->blend.field4 == 2 && !eventOwner.state().flags904[1] &&
                                    !eventOwner.state().captureRecord && !eventActor.flags,
                                    "Pending suction release takes priority over captured state and delivers integer command two.");
                                actors::MoveActor child(construction, 126, 0, 130);
                                child.flags = 0x20;
                                eventActor.children[0].actor = &child;
                                eventActor.children[6].actor = &child;
                                suction();
                                require(eventActor.flags == 0xc400 && child.flags == 0xc420 && entries.entries[0]->blend.field4 == 1 &&
                                    !captureSounds, "Capture completion propagates original flags across all seven child slots and delivers integer command one.");
                                eventActor.children[0].actor = eventActor.children[6].actor = nullptr;
                                entries.entries[0]->state = 0;
                                eventOwner.state().flags904[1] = true;
                                entries.entries[0]->blendChanged = 0;
                                suction();
                                require(eventOwner.state().flags904[1] && !entries.entries[0]->blendChanged,
                                    "Absent active target leaves suction flags and delivery state untouched.");
                                eventActor.animationState = originalAnimation;
                            }
                            eventOwner.ownership().strategy.reset();
                            require(eventOwner.event(2) == 1 && eventOwner.event(9) == 1 && eventOwner.event(11) == 1 &&
                                eventOwner.event(12) == 0 && eventOwner.event(13) == 0 && eventOwner.event(19) == 0 && eventOwner.event(26) == 0,
                                "Missing event observers retain per-message original defaults.");
                        }
                        require(&boundEnemies.owner(-1) == &boundEnemies.owner(128) &&
                            &boundEnemies.owner(127) == &bindingRuntime.absentOwner() &&
                            dynamic_cast<characterowner::Owner&>(bindingRuntime.absentOwner()).kind() == characterowner::Kind::Null,
                            "Invalid and unconstructed slots share the original null-owner sentinel.");
                        require(bindingRuntime.random() == 0x3c6ef35f && randomState == 0x3c6ef35f &&
                            bindingRuntime.random() == 0x47502932,
                            "Actor admission advances the shared original 32-bit random state.");
                        randomState = 0xffffffff;
                        require(bindingRuntime.random() == 0x3c558d52, "Original random arithmetic wraps at 32 bits.");
                        require(bindingRuntime.roomCleared(0), "Actor admission reads the original room-clear bit.");
                        flags.setOn(0x3d);
                        require(!bindingRuntime.roomCleared(0), "Map two's original flag suppresses room-clear status.");
                        mapMode = 1;
                        require(bindingRuntime.roomCleared(0), "Other map modes ignore the map-two flag.");
                        mapMode = 2;
                        flags.setOff(0x3d);
                        const auto roomId = rooms.entries()[0].id;
                        std::uint16_t savedRoom = 0;
                        visibility.mark(3, 2, roomId, roomFlags[0], savedRoom, 0, 0, 0, {}, {});
                        require(bindingRuntime.roomVisible(roomId, 3) && !bindingRuntime.roomVisible(roomId, 0),
                            "Actor visibility reads the requested original traversal buffer, including marker rooms.");
                        firstRecord[1] = 41;
                        secondRecord[1] = 73;
                        playerinput::Pad pad;
                        pad.port = 0;
                        std::int32_t audioLife = 0;
                        manager.bindActors(boundEnemies, boundRegistry, {&firstRecord, &secondRecord}, &pad, nullptr, audioLife);
                        {
                            actors::MoveActor captured(construction, 127, 0, 130);
                            characterowner::Owner owner(characterowner::Kind::Ordinary, strategies, bindingOwners);
                            owner.attach(captured, boundEnemies);
                            owner.state().flags904[0] = true;
                            captured.flags = 0x80000;
                            auto& player = dynamic_cast<playeractor::Player&>(*boundRegistry.actor(boundEnemies.appearPoint(0).actor));
                            captured.position = *player.view.position;
                            const auto* previousTarget = player.view.field8A4;
                            const std::array<float, 3> target{123, 456, 789};
                            player.view.field8A4 = &target;
                            playermodels::VacuumEntries entries(*manager.models, sharedGlobals.effects);
                            entries.entries[0]->state = 1;
                            entries.entries[0]->target = &captured;
                            entries.fieldA50[0] = &captured;
                            entries.targetRecords[0] = {};
                            unsigned sounds = 0;
                            auto sound = [&](unsigned id, const std::array<float, 3>& position, unsigned parameter) {
                                require(id == 0x408f && position == captured.position && !parameter && captured.velocity == std::array<float, 3>{},
                                    "Capture sound uses its original ID and actor position after stopping velocity.");
                                ++sounds;
                            };
                            unsigned captureType = 0;
                            while (captureType < 381 && creation.group(captureType) != 2) ++captureType;
                            require(captureType < 381, "Imported executable provides a group-two capture type.");
                            captured.actorType = captureType;
                            captured.setVelocity({3, 4, 0});
                            characterowner::updateSuction(owner, entries, boundRegistry, creation, sound);
                            require(captured.field100 == target && captured.velocity == std::array<float, 3>{} && captured.speed == 0 &&
                                sounds == 1 && entries.entries[0]->blend.field4 == 0,
                                "Active capture follows the nearest original Player view and preserves default blend command delivery.");
                            captured.actorType = 0;
                            characterowner::updateSuction(owner, entries, boundRegistry, creation, sound);
                            require(sounds == 1, "Other capture groups do not issue the group-two suction sound.");
                            player.view.field8A4 = previousTarget;
                            unsigned attachedType = 0;
                            while (attachedType < 381 && creation.group(attachedType) != 3) ++attachedType;
                            require(attachedType < 381, "Imported executable provides the original special-attachment group.");
                            captured.actorType = attachedType;
                            captured.flags = 0;
                            const auto previousSpecial = player.view.field895;
                            player.view.field895 = 0;
                            owner.updateSpecialAttachment(boundRegistry, creation);
                            require(!owner.state().field928 && !owner.state().actorReferences,
                                "Inactive player special state prevents dependent actor allocation.");
                            player.view.field895 = 1;
                            owner.updateSpecialAttachment(boundRegistry, creation);
                            require(!owner.state().field928, "Hidden rooms suppress the original special attachment.");
                            const auto attachmentRoom = captured.resolvePositionRoom();
                            visibility.mark(0, 2, attachmentRoom, roomFlags[attachmentRoom >> 24], savedRoom, 0, 0, 0, {}, {});
                            captured.flags = 0xc000;
                            owner.updateSpecialAttachment(boundRegistry, creation);
                            require(!owner.state().field928, "Inactive actor flags suppress dependent creation in a visible room.");
                            captured.flags = 0;
                            owner.updateSpecialAttachment(boundRegistry, creation);
                            auto* attached = static_cast<actors::MoveActor*>(owner.state().field928);
                            require(attached && attached->actorType == 0x3e && attached->parent == &captured &&
                                captured.children[0].joint == strategies.attachmentJoint(attachedType) &&
                                !captured.children[0].orientation && owner.state().actorReferences == 1 &&
                                owner.ownership().dependentActor == attached && attached->fieldE4 == 0xffffffff &&
                                attached->animationState == static_cast<unsigned>(modelCatalog.findAnimation(0x3e, "SINPAKU")),
                                "Special attachment creates type 3E through native EnManager ownership, original joint selection and SINPAKU animation.");
                            owner.updateSpecialAttachment(boundRegistry, creation);
                            require(owner.state().field928 == attached && owner.state().actorReferences == 1,
                                "Existing special attachment does not allocate another child or acquire another reference.");
                            owner.state().actorReferences = 2;
                            owner.state().actorUsers[1] = attached;
                            captured.flags = 0x8000;
                            owner.updateDependentActors(boundRegistry);
                            require((attached->flags & 0xc000) == 0xc000, "Dependent child follows the owning actor's inactive mask.");
                            player.view.field895 = 0;
                            captured.flags = 0;
                            owner.updateDependentActors(boundRegistry);
                            require(!owner.state().field928 && owner.state().actorReferences == 1 && attached->parent == &captured &&
                                !(attached->flags & 0xc000), "Leaving special state releases only its own reference and restores child flags.");
                            owner.releaseActorReference(owner.state().actorUsers[1]);
                            require(!owner.ownership().dependentActor && !owner.state().actorReferences && !attached->parent &&
                                !boundEnemies.activeOwner(*attached), "Last dependent reference detaches and returns the child to EnManager.");
                            player.view.field895 = previousSpecial;
                            playerrecord::Record damageRecord;
                            damageRecord.fill(0xa5);
                            unsigned damageEffects = 0;
                            auto damage = [&](unsigned type, const std::array<float, 3>& position) {
                                require(!type && position == captured.position && !owner.state().field8a0,
                                    "Damage particles receive original type zero and position before cooldown reset.");
                                ++damageEffects;
                            };
                            auto updateDamage = [&] { characterowner::updateDamage(owner, creationProperties, modelCatalog, damageRecord, damage); };
                            unsigned damageType = 0;
                            while (damageType < 381 && !modelCatalog.type(damageType).field4) ++damageType;
                            require(damageType < 381, "Original actor catalog contains damage-enabled types.");
                            captured.actorType = damageType;
                            captured.life = 9;
                            owner.state().life = 10;
                            owner.state().field8a0 = 1;
                            updateDamage();
                            require(damageEffects == 1 && owner.state().field8a0 == 6 && owner.state().life == 9,
                                "Cooldown one expires before the damage test and reloads the original six-tick delay.");
                            captured.life = 8;
                            updateDamage();
                            require(damageEffects == 1 && owner.state().field8a0 == 5 && owner.state().life == 8,
                                "Suppressed damage still updates previous life and does not defer its particle until cooldown expiry.");
                            for (unsigned i = 0; i < 5; ++i) updateDamage();
                            require(damageEffects == 1 && !owner.state().field8a0, "Cooldown expiry does not replay suppressed damage.");
                            captured.life = 0xffff;
                            owner.state().life = 0;
                            updateDamage();
                            require(damageEffects == 2 && owner.state().life == 0xffffffff,
                                "Owner damage compares and stores the actor life halfword with original sign extension.");
                            owner.state().field8a0 = 0x80000000;
                            owner.state().life = 10;
                            captured.life = 9;
                            updateDamage();
                            require(damageEffects == 2 && owner.state().field8a0 == 0x80000000 && owner.state().life == 9,
                                "Negative cooldown words neither decrement nor allow damage emission.");
                            owner.state().field8a0 = 0;
                            owner.state().life = 0x80000000;
                            captured.life = 0;
                            updateDamage();
                            require(damageEffects == 3, "Damage subtraction preserves original 32-bit wraparound.");
                            unsigned savedCount = 0;
                            for (unsigned type = 0; type < 381; ++type) {
                                if (creationProperties.group(type) != 3 || creationProperties.savedIndex(type) < 0) continue;
                                damageRecord.fill(0xa5);
                                captured.actorType = type;
                                captured.life = 0x8123;
                                owner.state().field8a0 = 2;
                                updateDamage();
                                const auto offset = 4 + creationProperties.savedIndex(type) * 2;
                                for (unsigned byte = 0; byte < damageRecord.size(); ++byte) {
                                    const auto expected = byte == offset ? 0x81 : byte == offset + 1 ? 0x23 : 0xa5;
                                    require(damageRecord[byte] == expected, "Saved ghost life updates only its original big-endian record halfword.");
                                }
                                ++savedCount;
                            }
                            require(savedCount != 0, "Imported saved-ghost mappings exercise native damage writeback.");
                            captured.position = {};
                            captured.setVelocity({7, 8, 9});
                            owner.state().field8d4 = 0;
                            auto motion = [&] { characterowner::updateMotion(owner, modelCatalog, modelResources, pose); };
                            motion();
                            require(captured.velocity == std::array<float, 3>{7, 8, 9}, "Inactive owner motion leaves velocity unchanged.");
                            owner.state().field8d4 = 1;
                            owner.state().field8d8 = {};
                            motion();
                            require(!owner.state().field8d4 && captured.velocity == std::array<float, 3>{7, 8, 9},
                                "Arrival clears motion but deliberately retains existing velocity.");
                            owner.state().field8d4 = 1;
                            owner.state().field8d8 = {3, 4, 0};
                            owner.state().field8e4 = 2;
                            motion();
                            require(owner.state().field8d4 == 1 && std::abs(captured.velocity[0] - 1.2f) < 0.00001f &&
                                std::abs(captured.velocity[1] - 1.6f) < 0.00001f, "Target motion limits normalized displacement to original speed.");
                            owner.state().field8e4 = 5;
                            motion();
                            require(owner.state().field8d4 == 1, "Exact target distance retains motion for the following tick.");
                            owner.state().field8e4 = 6;
                            motion();
                            require(!owner.state().field8d4 && std::abs(captured.velocity[0] - 3) < 0.00001f &&
                                std::abs(captured.velocity[1] - 4) < 0.00001f, "Short remaining distance clears motion after setting the final displacement.");
                            owner.state().field8d4 = 1;
                            owner.state().field8e4 = -2;
                            motion();
                            require(owner.state().field8d4 == 1 && captured.velocity[0] < 0 && captured.velocity[1] < 0,
                                "Negative original motion speed is not silently clamped.");
                            unsigned pathType = 381, pathAnimation = 0;
                            for (unsigned type = 0; type < 381 && pathType == 381; ++type) {
                                const auto& info = modelCatalog.type(type);
                                if (info.model >= 262 || !modelResources.entry(info.model).archive) continue;
                                for (unsigned animation = 0; animation < info.animations.size(); ++animation) {
                                    const auto& record = info.animations[animation];
                                    if (!(record[5] & 2) && modelResources.path(info.model, static_cast<std::uint16_t>(record[0]))) {
                                        pathType = type;
                                        pathAnimation = animation;
                                        break;
                                    }
                                }
                            }
                            require(pathType < 381, "Imported models provide a forward owner-motion path.");
                            captured.actorType = pathType;
                            captured.animationState = pathAnimation;
                            captured.animationFrame = 1;
                            captured.animationFrameStep = 1;
                            owner.state().field8d4 = 2;
                            owner.state().field8e8 = 0;
                            motion();
                            require(owner.state().field8d4 == 2 && std::isfinite(captured.speed),
                                "Owner path motion resolves the original type, animation key and frame interval through shared resources.");
                            captured.animationFrame = 0;
                            captured.animationFrameStep = -1;
                            bool needsScratch = false;
                            try { motion(); } catch (const std::runtime_error&) { needsScratch = true; }
                            require(needsScratch && owner.state().field8d4 == 2,
                                "Original unwritten forward-wrap scratch cannot silently become zero-filled displacement.");
                            const std::array<float, 3> scratch{11, 12, 13};
                            characterowner::updateMotion(owner, modelCatalog, modelResources, pose, &scratch);
                            const auto& pathInfo = modelCatalog.type(pathType);
                            const auto key = static_cast<std::uint16_t>(pathInfo.animations[pathAnimation][0]);
                            const auto& keyBytes = modelResources.entry(pathInfo.model).archive->keys[key]->bytes;
                            std::array<float, 3> terminal;
                            actoranimation::samplePath(*modelResources.path(pathInfo.model, key),
                                (unsigned(keyBytes[4]) << 8) | keyBytes[5], terminal);
                            for (unsigned axis = 0; axis < 3; ++axis) require(captured.velocity[axis] == scratch[axis] + terminal[axis],
                                "Captured scratch reproduces the original forward-wrap addition rather than a corrected delta.");
                        }
                        {
                            modeleffects::Factory modelFactory;
                            gamearchive::Resources commonEffects(common);
                            particles::EmitterPool emitters(0, 0, 0);
                            particles::EffectFactory particleFactory(dol, commonEffects, emitters);
                            unsigned variant = 0;
                            charactereffects::Resources resources(modelFactory, particleFactory, variant);
                            resources.bind(boundEnemies, boundRegistry);
                            auto& player = dynamic_cast<playeractor::Player&>(*boundRegistry.actor(boundEnemies.appearPoint(0).actor));
                            const auto previous = player.view.fieldsED4[0];
                            player.view.fieldsED4[0] = 399;
                            require(resources.playerValue() == 399 && resources.gameVariant() == 0,
                                "Character effect selection reads player zero's current original view through EnManager and the registry.");
                            variant = 1;
                            player.view.fieldsED4[0] = 0xffffffff;
                            require(resources.playerValue() == -1 && resources.gameVariant() == 1,
                                "Character effect selection preserves signed player values and live game-variant changes.");
                            player.view.fieldsED4[0] = previous;
                        }
                        auto& firstPlayer = dynamic_cast<playeractor::Player&>(*boundRegistry.actor(0));
                        auto& secondPlayer = dynamic_cast<playeractor::Player&>(*boundRegistry.actor(1));
                        {
                            auto& player = firstPlayer;
                            auto& actions = *player.components->actions;
                            auto& attached = *player.components->attached;
                            auto& feedback = player.state.field7A0->feedback;
                            const auto savedState = player.state;
                            const auto savedView = player.view;
                            const auto savedActions = actions;
                            const auto savedFeedback = feedback;
                            const auto savedRecord = *player.state.record;
                            const auto savedLife = player.life;
                            const auto savedReaction = player.components->reactions->state;
                            const auto savedSuctionQuaternion = player.components->quaternion->quaternion;
                            const auto firstTransition = attached.field2C0;
                            const auto secondTransition = attached.field2D0;
                            const std::array<float, 5> transitionValues{attached.field2C4, attached.field2C8, attached.field2CC,
                                attached.field2D4, attached.field2D8};
                            struct Sounds : playerevents::SuctionSounds {
                                struct Call { unsigned id; const itemsuction::Vector* position; bool weapon; };
                                std::vector<Call> calls;
                                void actor(unsigned id, const itemsuction::Vector& position, unsigned parameter) override {
                                    require(parameter == 0, "Suction actor sound preserves the original zero parameter.");
                                    calls.push_back({id, &position, false});
                                }
                                void weapon(unsigned id, const itemsuction::Vector& position) override {
                                    calls.push_back({id, &position, true});
                                }
                            } sounds;
                            gameflags::State suctionFlags;
                            std::int32_t elementDuration = 100;
                            playerevents::SuctionEnvironment environment{modelCatalog, creationProperties, suctionFlags, sounds, elementDuration};
                            itemsuction::Completion payload;
                            payload.amount = 2;
                            for (unsigned kind : {0u, 1u, 2u, 4u, 5u, 6u}) {
                                feedback.reset();
                                sounds.calls.clear();
                                payload.kind = kind;
                                auto& counter = kind < 3 ? player.state.fields11D8[kind] : kind == 4 ?
                                    player.state.field12F0 : player.state.fields12F4[kind - 5];
                                counter = 0xffffffff;
                                require(playerevents::completeSuction(player, payload, environment) && counter == 1,
                                    "Every suction counter branch preserves unsigned wraparound.");
                                if (kind < 2) require(player.state.field1120 == 8 && sounds.calls.empty(),
                                    "First two counters reset the eight-frame field without playing a sound.");
                                if (kind == 2 || kind == 5 || kind == 6) {
                                    const unsigned expected = kind == 2 ? 0x1038 : kind == 5 ? 0x810 : 0x811;
                                    require(sounds.calls.size() == 1 && sounds.calls[0].id == expected &&
                                        sounds.calls[0].position == &player.position && !sounds.calls[0].weapon,
                                        "Counter sound dispatch retains its actor position identity and sound entry point.");
                                }
                                if (kind == 5 || kind == 6) require(feedback.pulses.slots[0].active &&
                                    feedback.pulses.slots[0].remaining == 10 && feedback.pulses.slots[0].fade == 0 &&
                                    feedback.pulses.slots[0].amplitude == (kind == 5 ? 0.3f : 0.2f),
                                    "Money suction schedules the original timed feedback amplitude.");
                            }
                            actions.attached = &attached;
                            actions.field310 = 1;
                            actions.field314 = 0;
                            actions.field318 = 44;
                            actions.fields31C[4] = 2;
                            attached.field2C0 = attached.field2D0 = 1;
                            attached.field2C4 = attached.field2C8 = attached.field2CC = attached.field2D4 = attached.field2D8 = 17;
                            require(actions.suctionReaction(3, 4) && actions.field310 == 2 && actions.field314 == 14 &&
                                actions.field318 == 0 && actions.field358 == 3 && actions.field35C == 4 &&
                                actions.fields31C[4] == 0 && actions.field2FC == 1 && !attached.field2C0 && !attached.field2D0 &&
                                attached.field2C4 == 0 && attached.field2C8 == 1 && attached.field2CC == 0 &&
                                attached.field2D4 == 0 && attached.field2D8 == 1,
                                "Suction reaction transitions through action fourteen and stops both bound attachment transitions.");
                            attached.field2C4 = attached.field2D4 = 19;
                            actions.suctionReaction(2, 0);
                            require(attached.field2C4 == 19 && attached.field2D4 == 19,
                                "Inactive attachment transitions preserve their retained scalar fields.");
                            for (unsigned blocked : {0x20u, 0x21u, 0x22u}) {
                                actions.field310 = 2;
                                actions.field314 = blocked;
                                actions.field2FC = 0;
                                actions.field318 = 91;
                                require(!actions.suctionReaction(4, 3) && actions.field314 == blocked && actions.field318 == 91 && !actions.field2FC,
                                    "Protected action states reject reaction event nine without resetting their clocks.");
                            }
                            payload.kind = 3;
                            std::array<bool, 3> sizeBranches{};
                            for (unsigned type = 0; type < 381; ++type) {
                                const auto size = modelCatalog.type(type).field14;
                                const unsigned branch = size > 2 ? 2 : size > 1 ? 1 : 0;
                                if (sizeBranches[branch]) continue;
                                sizeBranches[branch] = true;
                                feedback.reset();
                                player.playFeedback(12 + branch);
                                const auto expectedScript = feedback.scripts.slots[0].script.identity;
                                feedback.reset();
                                sounds.calls.clear();
                                payload.type = type;
                                playerevents::completeSuction(player, payload, environment);
                                require(sounds.calls.size() == 1 && sounds.calls[0].weapon &&
                                    sounds.calls[0].id == (branch == 2 ? 0x4828u : 0x4808u) &&
                                    sounds.calls[0].position == &player.components->weapon->first->position &&
                                    feedback.scripts.slots[0].active && feedback.scripts.slots[0].script.identity == expectedScript,
                                    "Imported capture sizes select their original sound and feedback preset thresholds.");
                            }
                            require(sizeBranches[0] && sizeBranches[1] && sizeBranches[2],
                                "Imported actor descriptors exercise all three suction feedback sizes.");
                            player.life = 59;
                            player.state.entryCount = 64;
                            for (unsigned i = 0; i < 64; ++i) player.state.entries[i] = i;
                            payload.type = 0x40;
                            std::fill_n(player.components->bits[0]->data, player.components->bits[0]->bytes, 0);
                            std::fill_n(player.components->bits[4]->data, player.components->bits[4]->bytes, 0);
                            require(playerevents::completeSuction(player, payload, environment) && player.state.entryCount == 64 &&
                                player.state.entries[0] == 1 && player.state.entries[62] == 63 && player.state.entries[63] == 0x40 &&
                                (player.components->bits[0]->data[8] & 1),
                                "Captured actor history evicts its oldest entry and invokes capture registration.");
                            const auto gradeBefore = std::vector<std::uint8_t>(player.components->bits[4]->data,
                                player.components->bits[4]->data + player.components->bits[4]->bytes);
                            player.life = 100;
                            playerevents::completeSuction(player, payload, environment);
                            require(std::equal(gradeBefore.begin(), gradeBefore.end(), player.components->bits[4]->data),
                                "Repeated suction capture retains the first registered grade.");
                            for (unsigned type : {0x72u, 0x73u, 0x74u, 0xd6u}) {
                                payload.type = type;
                                elementDuration = -17;
                                playerevents::completeSuction(player, payload, environment);
                                require(player.state.field1184 == (type == 0x72 ? 2u : type == 0x74 ? 3u : 4u) && player.state.field1188 == -17,
                                    "Elemental suction preserves original type mapping and signed shared duration.");
                            }
                            payload.type = 0;
                            player.state.field1184 = 4;
                            player.state.field1188 = 51;
                            playerevents::completeSuction(player, payload, environment);
                            require(player.state.field1184 == 4 && player.state.field1188 == 51,
                                "Ordinary captures retain existing elemental state.");
                            std::fill_n(player.components->bits[2]->data, 8, 0);
                            player.components->bits[2]->data[0] = 15;
                            actions.field310 = 1;
                            player.components->reactions->state = 3;
                            payload.collectionBit = 60;
                            playerevents::completeSuction(player, payload, environment);
                            require(!suctionFlags.value(0x38), "Collection threshold excludes bits sixty through sixty-three.");
                            payload.collectionBit = 4;
                            playerevents::completeSuction(player, payload, environment);
                            require(suctionFlags.value(0x38) && actions.field358 == 3 && actions.field35C == 4 && player.view.count == player.state.entryCount,
                                "Fifth tracked collection sets flag thirty-eight, requests its reaction and refreshes the player view.");
                            payload.type = 0xd6;
                            actions.field358 = 91;
                            actions.field35C = 92;
                            playerevents::completeSuction(player, payload, environment);
                            require(actions.field358 == 91 && actions.field35C == 92, "Type D6 skips the collection reaction.");
                            payload.type = 0xdc;
                            playerevents::completeSuction(player, payload, environment);
                            require(actions.field35C == 3, "Type DC replaces the earlier collection reaction with kind three.");
                            payload.type = 0xdd;
                            payload.collectionBit = -1;
                            playerevents::completeSuction(player, payload, environment);
                            require(player.state.field10C4 == 600 && player.state.field117C == 600,
                                "Type DD sets both original six-hundred-frame state fields.");
                            for (unsigned kind : {7u, 8u}) {
                                payload.kind = kind;
                                feedback.reset();
                                sounds.calls.clear();
                                require(!playerevents::completeSuction(player, payload, environment) && actions.field35C == 0 &&
                                    sounds.calls.size() == 1 && sounds.calls[0].id == 0x4808 && sounds.calls[0].weapon &&
                                    sounds.calls[0].position == &player.components->weapon->first->position && feedback.scripts.slots[0].active,
                                    "Kinds seven and eight send weapon sound and reaction while preserving their zero event result.");
                            }
                            payload.delayEnabled = 1;
                            payload.delay = 37;
                            for (unsigned kind : {3u, 5u, 6u, 7u, 99u}) {
                                payload.kind = kind;
                                player.state.field1124 = 0;
                                require(playerevents::completeSuction(player, payload, environment) && player.state.field1128 == 37 &&
                                    player.state.field1124 == (kind == 3 || kind == 5 ? 0 : 10),
                                    "Delay scheduling preserves rejected negative pipe speeds and the default delay for other kinds.");
                            }
                            payload.kind = 99;
                            player.state.field1124 = std::numeric_limits<float>::quiet_NaN();
                            require(!playerevents::completeSuction(player, payload, environment) && std::isnan(player.state.field1124),
                                "Unordered existing delay does not satisfy the original nonpositive gate.");
                            payload.delayEnabled = 2;
                            player.state.field1124 = 0;
                            require(!playerevents::completeSuction(player, payload, environment), "Delay scheduling requires exactly one.");
                            bool collected = false;
                            playerevents::SuctionEvents delivery(player, boundRegistry, environment, [&](actors::MoveActor& value) {
                                require(&value == &secondPlayer && player.state.field12F0 == 123,
                                    "Collection callback runs after the native completion event updates player state.");
                                collected = true;
                            });
                            player.state.field1180 = 1;
                            delivery.attraction(secondPlayer.id());
                            require(player.state.field10A4 == secondPlayer.position, "Suction adapter routes attraction through the native actor registry.");
                            player.state.field12F0 = 122;
                            delivery.completion({4, 1, 0, -1, 0, 10});
                            delivery.collect(secondPlayer);
                            require(collected, "Suction adapter delivers the owning collection callback.");
                            *player.state.record = savedRecord;
                            player.state = savedState;
                            player.view = savedView;
                            player.life = savedLife;
                            actions = savedActions;
                            feedback = savedFeedback;
                            player.components->reactions->state = savedReaction;
                            player.components->quaternion->quaternion = savedSuctionQuaternion;
                            attached.field2C0 = firstTransition;
                            attached.field2D0 = secondTransition;
                            attached.field2C4 = transitionValues[0]; attached.field2C8 = transitionValues[1]; attached.field2CC = transitionValues[2];
                            attached.field2D4 = transitionValues[3]; attached.field2D8 = transitionValues[4];
                        }
                        {
                            const auto savedState = firstPlayer.state;
                            const auto savedQuaternion = firstPlayer.components->quaternion->quaternion;
                            const auto savedView = firstPlayer.view;
                            const auto savedActions = *firstPlayer.components->actions;
                            const auto secondCapture = secondPlayer.view.field8C9;
                            const auto savedRecord = firstRecord;
                            const auto savedFeedback = firstPlayer.state.field7A0->feedback;
                            const auto savedLife = firstPlayer.life;
                            mapcollision::State itemCollision(ownerMission);
                            actors::RotatingSurfaces surfaces;
                            characteritems::Admission admission(dol, types);
                            playerevents::ItemAward award{};
                            playerevents::ItemEnvironment collection{boundRegistry, boundEnemies, admission, &award,
                                [](unsigned, const std::array<float, 3>&) {}};
                            struct ItemSounds : enemystrategy::ItemSounds, playerevents::SuctionSounds {
                                std::vector<unsigned> calls;
                                void play(unsigned sound, const itemsuction::Vector&, unsigned parameter) override {
                                    require(parameter == 0, "EnItem sound preserves its zero parameter.");
                                    calls.push_back(sound);
                                }
                                void actor(unsigned sound, const itemsuction::Vector&, unsigned) override { calls.push_back(sound); }
                                void weapon(unsigned sound, const itemsuction::Vector&) override { calls.push_back(sound); }
                            } itemSounds;
                            std::int32_t elementDuration = 100;
                            playerevents::SuctionEnvironment feedback{modelCatalog, creationProperties, flags, itemSounds, elementDuration};
                            itemsuction::Vector origin{}, axis{0, 0, 1};
                            particles::SuctionContext suctionContext{&origin, &axis, 1};
                            itemsuction::Force force(firstPlayer.view, suctionContext);
                            struct UnboundEvents : itemsuction::Events {
                                void attraction(unsigned) override { throw std::runtime_error("EnItem must own its attraction recipient."); }
                                void completion(const itemsuction::Completion&) override { throw std::runtime_error("EnItem must own its completion recipient."); }
                                void collect(actors::MoveActor&) override { throw std::runtime_error("EnItem must own its collection recipient."); }
                            } unbound;
                            itemsuction::Environment suctionEnvironment{rooms, visibility, pose, suctionContext, force, ownerParticles, flags, unbound};
                            std::uint8_t durationBase = 0;
                            enemystrategy::ItemEnvironment itemEnvironment{common, types, strategies, itemCollision, surfaces, collisionCatalog,
                                collection, feedback, suctionEnvironment, itemSounds, [] { return itemsuction::Vector{0, 0, 1}; }, 0, durationBase};
                            ownerStrategies.bindItems(&itemEnvironment);
                            unsigned itemType = 0;
                            while (itemType < 381 && strategies.constructor(strategies.select(itemType, {})) != glme01::itemFactory) ++itemType;
                            require(itemType < 381,
                                "Imported treasure type selects the original EnItem constructor.");
                            characteritems::Creation itemCreation(ownerMission, construction.parameters, admission, boundEnemies,
                                bindingRuntime, boundRegistry, rooms, itemSounds);
                            const auto spawnBounds = rooms.entries()[0].bounds;
                            const itemsuction::Vector spawnPosition{float(spawnBounds[0] + spawnBounds[3]) / 2,
                                float(spawnBounds[1] + spawnBounds[4]) / 2, float(spawnBounds[2] + spawnBounds[5]) / 2};
                            const itemsuction::Vector launchVelocity{7, 8, 9};
                            auto randomBefore = randomState;
                            auto spawned = itemCreation.create(itemType, spawnPosition, &launchVelocity);
                            require(spawned != 0xffffffff && randomState == randomBefore &&
                                boundRegistry.actor(spawned)->position == spawnPosition &&
                                boundRegistry.actor(spawned)->velocity == launchVelocity &&
                                (boundRegistry.actor(spawned)->fieldE0 & 255) == 255 &&
                                (boundRegistry.actor(spawned)->fieldE4 & 255) == 255,
                                "Manager item route applies explicit launch and visibility without consuming random state.");
                            boundEnemies.deactivate(spawned);
                            spawned = itemCreation.create(itemType, spawnPosition);
                            require(spawned != 0xffffffff && randomState == randomBefore * 0x19660du + 0x3c6ef35fu &&
                                boundRegistry.actor(spawned)->velocity == itemsuction::Vector{0, 12, 0},
                                "Default launch at room center retains its upward velocity and still consumes one random draw.");
                            boundEnemies.deactivate(spawned);
                            randomBefore = randomState;
                            auto displaced = spawnPosition;
                            displaced[0] -= 10;
                            randomState = 0;
                            spawned = itemCreation.create(itemType, displaced);
                            const float angle = (static_cast<float>(0x3c6ef35fu % 10000) / 9999.0f - 0.5f);
                            const auto generated = boundRegistry.actor(spawned)->velocity;
                            require(std::abs(generated[0] - 3 * std::cos(angle)) < 0.00001f && generated[1] == 12 &&
                                std::abs(generated[2] + 3 * std::sin(angle)) < 0.00001f,
                                "Original quaternion launch preserves the 9999 divisor and doubled half-angle rotation.");
                            boundEnemies.deactivate(spawned);
                            randomState = randomBefore;
                            require(itemCreation.create(jmp::ToolDataRef{}, spawnPosition) == 0xffffffff && randomState == randomBefore,
                                "Missing item references return before allocation and random launch.");
                            const auto* itemInfo = ownerMission.getJmpResource("iteminfotable");
                            unsigned routedRecords = 0;
                            for (unsigned row = 0; row < itemInfo->count(); ++row) {
                                const jmp::ToolDataRef source{itemInfo, static_cast<int>(row)};
                                if (admission.nothing(source) || admission.money(source)) {
                                    require(itemCreation.create(source, spawnPosition) == 0xffffffff && randomState == randomBefore,
                                        "Nothing and money records leave manager creation and RNG untouched.");
                                    continue;
                                }
                                const auto type = admission.type(source);
                                if (type >= 381 || strategies.constructor(strategies.select(type, {})) != glme01::itemFactory) continue;
                                auto& captureByte = firstPlayer.components->bits[0]->data[type >> 3];
                                const auto retainedByte = captureByte;
                                captureByte &= static_cast<std::uint8_t>(~(1u << (type & 7)));
                                if (admission.blocked(source, firstPlayer.view)) { captureByte = retainedByte; continue; }
                                spawned = itemCreation.create(source, spawnPosition, &launchVelocity);
                                require(spawned != 0xffffffff && randomState == randomBefore,
                                    "Table-driven item creation preserves RNG for explicit and redirected launches.");
                                auto& spawnedOwner = dynamic_cast<characterowner::Owner&>(*boundEnemies.activeOwner(*boundRegistry.actor(spawned)));
                                auto& spawnedItem = dynamic_cast<enemystrategy::Item&>(*spawnedOwner.ownership().strategy);
                                require(spawnedItem.itemSource.table == source.table && spawnedItem.itemSource.row == source.row &&
                                    !spawnedOwner.source.table && boundRegistry.actor(spawned)->velocity ==
                                    (admission.redirected(source) ? itemsuction::Vector{0, 12, 0} : launchVelocity),
                                    "ItemInfoTable identity attaches to EnItem after manager creation without replacing placement ownership.");
                                boundEnemies.deactivate(spawned);
                                if ((types.flags(type) & 1) && !admission.key(type)) {
                                    captureByte |= static_cast<std::uint8_t>(1u << (type & 7));
                                    require(itemCreation.create(source, spawnPosition) == 0xffffffff && randomState == randomBefore,
                                        "Captured unique items are rejected before manager allocation or launch randomization.");
                                }
                                captureByte = retainedByte;
                                ++routedRecords;
                            }
                            require(routedRecords != 0, "Imported item records exercise the original manager route.");
                            std::vector<unsigned> occupiedItems;
                            while ((spawned = itemCreation.create(itemType, spawnPosition, &launchVelocity)) != 0xffffffff) {
                                occupiedItems.push_back(spawned);
                                require(occupiedItems.size() <= 128, "Item creation stays within original manager pools.");
                            }
                            require(!occupiedItems.empty() && itemCreation.create(itemType, spawnPosition) == 0xffffffff && randomState == randomBefore,
                                "Exhausted item pools return minus one before default launch consumes RNG.");
                            for (auto id : occupiedItems) boundEnemies.deactivate(id);
                            const auto itemId = boundEnemies.createActor(itemType, -1);
                            require(itemId != 0xffffffff, "EnManager creates EnItem through its original actor pool.");
                            auto& itemOwner = dynamic_cast<characterowner::Owner&>(*boundEnemies.activeOwner(*boundRegistry.actor(itemId)));
                            auto& itemActor = dynamic_cast<actors::MoveActor&>(itemOwner.actor());
                            auto& item = dynamic_cast<enemystrategy::Item&>(*itemOwner.ownership().strategy);
                            const auto bounds = rooms.entries()[0].bounds;
                            {
                                const auto* choices = ownerMission.getJmpResource("itemappeartable");
                                const auto nextRandom = randomState * 0x19660du + 0x3c6ef35fu;
                                const auto column = nextRandom % choices->fieldCount();
                                unsigned row = 1;
                                while (row < choices->count() && choices->fieldString(row, column) != "nothing") ++row;
                                require(row < choices->count(), "Imported capture tables supply a nonzero nothing-drop row.");
                                std::vector<std::uint8_t> data(32);
                                word(data, 0, 1); word(data, 4, 1); word(data, 8, 28); word(data, 12, 4);
                                word(data, 16, hash("item_table")); word(data, 20, 0xffffffff); word(data, 24, 0); word(data, 28, row);
                                jmp::ToolData source;
                                require(source.attach(data, error), error);
                                itemOwner.source = {&source, 0};
                                std::array<std::uint32_t, 7> capture{};
                                itemOwner.state().captureRecord = &capture;
                                struct CaptureRoute : characterowner::CaptureServices {
                                    bool initialized = false;
                                    void initializeMotion(characterowner::Owner&, std::uint16_t) override { initialized = true; }
                                    void dropItem(characterowner::Owner&) override { throw std::runtime_error("Native item creation must replace the former drop dependency."); }
                                    void notifyPlayer(characterowner::Owner&) override { throw std::runtime_error("Unexpected capture completion."); }
                                } captureRoute;
                                bindingOwners.bindCapture(&captureRoute);
                                bindingOwners.bindItemCreation(&itemCreation);
                                require(itemOwner.event(3) == 1 && captureRoute.initialized && randomState == nextRandom,
                                    "Character capture event invokes native table selection and item admission after motion initialization.");
                                bindingOwners.bindItemCreation(nullptr);
                                bindingOwners.bindCapture(nullptr);
                                itemOwner.state().captureRecord = nullptr;
                                itemOwner.state().flags904[0] = false;
                                itemOwner.source = {};
                            }
                            itemActor.setPosition({float(bounds[0] + bounds[3]) / 2, float(bounds[1] + bounds[4]) / 2,
                                float(bounds[2] + bounds[5]) / 2});
                            itemActor.fieldB4 = roomId;
                            itemOwner.updateStrategy();
                            require(item.state == 0 && item.nextState == 3 && item.flags == 0x17 && !item.collected,
                                "EnItem initialization preserves the deferred active transition and unplaced-item flags.");
                            itemOwner.updateStrategy();
                            require(item.state == 3 && item.frames == 1 && item.physics.owner == &itemOwner &&
                                item.physics.state == 0 && item.physics.nextState == 1 && item.initialRoom == roomId &&
                                item.lifetime.remaining == 480 && itemSounds.calls.front() == 0x1875,
                                "Native EnItem activation binds physics, starts its sound and latches room and lifetime.");
                            item.frames = 90;
                            firstPlayer.view.field8C9 = secondPlayer.view.field8C9 = 0;
                            itemActor.visibility(255, 255);
                            require(!item.canAttract(), "Ignore time excludes the exact ninety-frame boundary.");
                            item.frames = 91;
                            require(item.canAttract(), "Attraction opens after the unsigned ignore-time boundary.");
                            secondPlayer.view.field8C9 = 1;
                            require(!item.canAttract(), "Either player's active capture suppresses item attraction.");
                            secondPlayer.view.field8C9 = 0;
                            itemActor.visibility(0, 0);
                            require(!item.canAttract(), "Invisible items cannot enter suction.");
                            itemActor.visibility(255, 255);
                            itemActor.position = *firstPlayer.view.position;
                            item.frames = 29;
                            require(!item.touchingPlayer(), "Contact pickup excludes frame twenty-nine.");
                            item.frames = 30;
                            require(item.touchingPlayer(), "Original imported collision volumes permit pickup starting at frame thirty.");
                            itemActor.visibility(254, 254);
                            require(!item.touchingPlayer(), "Partially faded items do not qualify for contact pickup.");
                            itemActor.visibility(255, 255);
                            itemActor.actorType = 0xc2;
                            firstPlayer.life = 0;
                            require(!item.collectItem() && !item.collected && (item.flags & 0x10),
                                "Player refusal leaves EnItem available for a later attempt.");
                            firstPlayer.life = 100;
                            firstPlayer.state.flags1040[2] = firstPlayer.state.field1058 = firstPlayer.state.flags1140[2] = 0;
                            firstPlayer.state.field1170 = firstPlayer.state.field1174 = firstPlayer.state.field1130 = 0;
                            firstPlayer.state.field1338 = 1;
                            firstPlayer.state.flags10E8[1] = firstPlayer.state.field1060 = 0;
                            firstPlayer.state.field11A0 = nullptr;
                            firstPlayer.components->actions->field310 = 1;
                            itemActor.parent = &firstPlayer;
                            require(!item.collectItem(), "Attached items retain their collection guard.");
                            itemActor.parent = nullptr;
                            item.parameters.setBits(0, 0x55);
                            const auto counter = firstPlayer.state.fields12F4[9];
                            require(item.collectItem() && item.collected && !(item.flags & 0x10) && flags.value(0x55) &&
                                firstPlayer.state.field1064 == &itemActor && firstPlayer.state.fields12F4[9] == counter + 1 &&
                                ownerParticles.history[0] == 0x46 &&
                                (firstPlayer.components->bits[0]->data[0xc2 >> 3] & (1u << (0xc2 & 7))),
                                "EnItem collection connects original player acceptance, counter, flag, effect and capture registration.");
                            require(!item.collectItem() && firstPlayer.state.fields12F4[9] == counter + 1,
                                "Repeated collection does not award the item twice.");
                            itemActor.setVelocity({3, 4, 5});
                            item.update();
                            require(itemActor.velocity == itemsuction::Vector{}, "Collected EnItem stops its velocity on the next strategy frame.");
                            item.collected = false;
                            item.expired = true;
                            require(!item.collectItem(), "Expired EnItem rejects collection.");
                            item.expired = false;
                            item.flags = 1;
                            itemOwner.state().field91c = false;
                            firstPlayer.state.field1060 = 0;
                            require(item.collectItem() && itemOwner.state().field91c && ownerParticles.history[0] == 0x46 &&
                                ownerParticles.history[1] == 0x46 && ownerParticles.history[2] == 0x46,
                                "Nonanimated collection preserves both owner notifications around the collection effect.");
                            item.collected = false;
                            item.flags = 0x11;
                            itemOwner.state().field91c = false;
                            item.frames = 0;
                            item.initialRoom = itemActor.fieldB4;
                            itemActor.fieldB4 ^= 0x10000;
                            item.update();
                            require(!(item.flags & 1) && itemOwner.state().field91c && !item.expired,
                                "Changed room subfield sends the native owner room event and capture-end notification.");
                            itemOwner.state().field91c = false;
                            item.update();
                            require(itemOwner.state().field91c, "Original room comparison retains its initial snapshot and repeats notification.");
                            item.flags = 1;
                            itemOwner.state().field91c = false;
                            require(itemOwner.event(10) == 0 && !itemOwner.state().field91c,
                                "Placed items ignore the room-removal callback.");
                            item.lifetime.remaining = 1;
                            itemActor.fieldB4 = item.initialRoom;
                            item.update();
                            require(item.lifetime.remaining == 1 && !item.expired, "Ordinary EnItem keeps the original infinite-lifetime override.");
                            item.lifetime.parameters.setBits(3, 50);
                            item.lifetime.remaining = 4;
                            item.lifetime.blink = 0;
                            for (unsigned i = 0; i < 3; ++i) item.lifetime.update(itemOwner, false, false, item.flags, item.expired);
                            require(item.lifetime.remaining == 1 && !item.lifetime.blink && (itemActor.fieldE0 & 255) == 0,
                                "Finite-item blinking hides exactly on its third blink tick.");
                            item.lifetime.update(itemOwner, false, false, item.flags, item.expired);
                            require(item.expired && !(item.flags & 1) && itemOwner.state().field91c && (itemActor.fieldE0 & 255) == 255,
                                "Finite-item expiration follows blinking, clears its effect flag and notifies the owner.");
                            item.lifetime.remaining = 0x80000000;
                            item.expired = false;
                            item.lifetime.update(itemOwner, false, false, item.flags, item.expired);
                            require(item.lifetime.remaining == 0x7fffffff && !item.expired, "Lifetime subtraction wraps before signed comparison.");
                            item.lifetime.update(itemOwner, false, true, item.flags, item.expired);
                            require(item.lifetime.remaining == 0x7fffffff, "Collected finite items pause lifetime progression.");
                            for (unsigned appearance : {3u, 4u}) {
                                std::vector<std::uint8_t> placement(48);
                                word(placement, 0, 1); word(placement, 4, 2); word(placement, 8, 40); word(placement, 12, 8);
                                word(placement, 16, hash("cond_type")); word(placement, 20, 0xffffffff); word(placement, 24, 0);
                                word(placement, 28, hash("appear_type")); word(placement, 32, 0xffffffff); word(placement, 36, 4 << 16);
                                word(placement, 40, 2); word(placement, 44, appearance);
                                jmp::ToolData source;
                                require(source.attach(placement, error), error);
                                itemOwner.source = {&source, 0};
                                item.flags = 1;
                                item.state = 0;
                                item.nextState = 0xffff;
                                item.behavior();
                                require(item.nextState == 1, "Base item selects its original nested condition before appearance.");
                                itemActor.setVelocity({7, 8, 9});
                                item.update();
                                require(item.state == 1 && item.nextState == 0xffff && itemActor.velocity == itemsuction::Vector{},
                                    "Condition completion remains deferred and stops velocity on its first tick.");
                                item.update();
                                require(item.nextState == 2, "Condition terminal state queues appearance on its second tick.");
                                item.update();
                                require(item.state == 2 && item.nextState == 0xffff, "Appearance terminal state is not consumed early.");
                                if (appearance == 4) {
                                    require((itemActor.fieldE0 & 255) == 0 && (itemActor.fieldE4 & 255) == 255 && itemActor.fieldE8 == 4,
                                        "Nested fade initializes the actor's original alpha owner.");
                                    itemActor.visibility(254, 254);
                                    item.update();
                                    require(item.nextState == 0xffff, "Nested fade waits for exactly full alpha.");
                                    itemActor.visibility(255, 255);
                                    item.update();
                                    require(item.nextState == 0xffff, "Fade terminal transition preserves its final deferred tick.");
                                }
                                item.update();
                                require(item.nextState == 3, "Completed appearance queues the active item stage.");
                                itemOwner.source = {};
                            }
                            item.collected = item.expired = false;
                            item.flags = 7;
                            itemOwner.state().field91c = false;
                            firstPlayer.state.field1060 = 0;
                            firstPlayer.state.field1064 = nullptr;
                            itemActor.setPosition({float(bounds[0] + bounds[3]) / 2, float(bounds[1] + bounds[4]) / 2,
                                float(bounds[2] + bounds[5]) / 2});
                            itemActor.fieldB4 = roomId;
                            item.update();
                            item.frames = 91;
                            itemActor.visibility(255, 255);
                            itemActor.rotation = {11, 12, 13};
                            itemActor.previousRotation = {21, 22, 23};
                            origin = itemActor.position;
                            origin[2] -= 10;
                            firstPlayer.view.field8A0 = 1;
                            firstPlayer.view.field8B4 = firstPlayer.components->vacuum->model.get();
                            visibility.mark(0, 2, roomId, roomFlags[roomId >> 24], savedRoom, 0, 0, 0, {}, {});
                            const auto captures = firstPlayer.state.entryCount;
                            item.update();
                            require(item.collected && firstPlayer.state.field1064 == &itemActor &&
                                firstPlayer.state.entryCount == (captures == 64 ? 64 : captures + 1) &&
                                firstPlayer.state.entries[firstPlayer.state.entryCount - 1] == itemActor.type() &&
                                itemActor.scale == itemsuction::Vector{1, 1, 1} && itemActor.rotation[0] == 0 &&
                                itemActor.rotation[1] == 0 && itemActor.rotation[2] == 13 &&
                                itemActor.previousRotation[1] == 0 && itemActor.previousRotation[2] == 0,
                                "Native suction completion reaches player event thirteen and EnItem collection before restoring original scale and angles.");
                            unsigned loadedItemParameters = 0;
                            for (unsigned type = 0; type < 381; ++type) {
                                if (strategies.constructor(strategies.select(type, {})) != glme01::itemFactory) continue;
                                itemActor.actorType = type;
                                item.state = 0;
                                item.initializeBehavior();
                                if (common.findPathResource("/param/th/EnItem_" + types.name(type) + ".prm") >= 0) {
                                    require(item.parameters.loaded(), "Every available EnItem parameter file loads through the native byte parameter reader.");
                                    ++loadedItemParameters;
                                }
                            }
                            require(loadedItemParameters != 0, "Imported archive exercises EnItem-specific parameter loading.");
                            boundEnemies.deactivate(itemId);
                            {
                                const auto savedFlags = flags;
                                std::vector<unsigned> globalSounds;
                                itemEnvironment.globalSound = [&](unsigned sound) { globalSounds.push_back(sound); };
                                firstPlayer.state.flags1040[2] = firstPlayer.state.field1058 = firstPlayer.state.flags1140[2] = 0;
                                firstPlayer.state.field1170 = firstPlayer.state.field1174 = firstPlayer.state.field1130 = 0;
                                firstPlayer.state.field1338 = 1;
                                firstPlayer.state.flags10E8[1] = firstPlayer.state.field1060 = 0;
                                firstPlayer.state.field11A0 = nullptr;
                                firstPlayer.components->actions->field310 = 1;
                                firstPlayer.life = 100;
                                firstPlayer.view.field8C9 = 1;
                                auto& keyBits = *firstPlayer.components->flags;
                                for (unsigned door = 0; door < 256; ++door) {
                                    for (unsigned flag : {0x27u, 0x50u, 0x0cu, 0x44u, 0x52u}) flags.setOff(static_cast<std::uint8_t>(flag));
                                    keyBits.data[door >> 3] &= static_cast<std::uint8_t>(~(1u << (door & 7)));
                                    firstPlayer.state.field1061 = 0;
                                    firstPlayer.state.field1068 = -1;
                                    require(playerevents::collectKey(firstPlayer, static_cast<std::uint8_t>(door), flags) &&
                                        firstPlayer.state.field1061 == 1 && firstPlayer.state.field1068 == static_cast<int>(door) &&
                                        (firstRecord[0x144 + (door >> 3)] & (1u << (door & 7))),
                                        "Key event writes all byte-sized door IDs directly into the original saved flag buffer.");
                                    require(flags.value(0x27) == (door == 3) && flags.value(0x50) == (door == 0x1b) &&
                                        flags.value(0x0c) == (door == 0x21) && flags.value(0x44) == (door == 0x2a) &&
                                        flags.value(0x52) == (door == 0x3b), "Only the five original door IDs set their exact progression flags.");
                                    firstPlayer.state.field1061 = 0;
                                    firstPlayer.state.field1068 = -1;
                                    require(!playerevents::collectKey(firstPlayer, static_cast<std::uint8_t>(door), flags) &&
                                        firstPlayer.state.field1061 == 0 && firstPlayer.state.field1068 == -1,
                                        "Duplicate key events leave the award state unchanged.");
                                }
                                for (unsigned i = 0; i < keyBits.bytes; ++i) keyBits.data[i] = 0;

                                std::vector<std::uint8_t> placement(76);
                                word(placement, 0, 1); word(placement, 4, 2); word(placement, 8, 40); word(placement, 12, 36);
                                word(placement, 16, hash("OpenDoorNo")); word(placement, 20, 0xffffffff); word(placement, 24, 0);
                                word(placement, 28, hash("create_name")); word(placement, 32, 0xffffffff); word(placement, 36, 4 << 16);
                                word(placement, 40, 0x103);
                                placement[44] = '3'; placement[45] = '_'; placement[46] = '1';
                                jmp::ToolData placedSource, droppedSource;
                                require(placedSource.attach(placement, error), error);
                                word(placement, 40, 0x21);
                                require(droppedSource.attach(placement, error), error);
                                unsigned keyTypes = 0;
                                for (unsigned type = 0; type < 381; ++type) {
                                    if (strategies.constructor(strategies.select(type, {})) != glme01::keyFactory) continue;
                                    ++keyTypes;
                                    const auto soundCount = itemSounds.calls.size();
                                    const auto id = itemCreation.create(type, spawnPosition, &launchVelocity);
                                    require(id != 0xffffffff, "Every original EnKey type creates through EnManager.");
                                    auto& keyActor = *boundRegistry.actor(id);
                                    auto& keyOwner = dynamic_cast<characterowner::Owner&>(*boundEnemies.activeOwner(keyActor));
                                    auto& key = dynamic_cast<enemystrategy::Key&>(*keyOwner.ownership().strategy);
                                    require(!key.awarded && itemSounds.calls.size() == soundCount + 1 && itemSounds.calls.back() == 0x1875,
                                        "Key creation owns one original launch sound and starts without an award.");
                                    key.update();
                                    require(key.flags == 0x17 && key.nextState == 3 && key.physics.parameters.loaded(),
                                        "Dropped keys load their throw parameters and bypass placed condition/appearance states.");
                                    require(itemEnvironment.keyParameters.loaded(), "Original shared EnKey parameters load from the imported archive.");
                                    key.update();
                                    require(key.state == 3 && itemSounds.calls.size() == soundCount + 1,
                                        "Entering the key active state does not replay the launch sound.");
                                    const int noCollision = modelCatalog.findAnimation(type, "NOMAPCOL");
                                    const int normal = modelCatalog.findAnimation(type, "NORMAL");
                                    require(noCollision >= 0 && normal >= 0, "Imported key types provide original collision animation states.");
                                    itemEnvironment.keyParameters.setBits(0, 0x41200000);
                                    for (unsigned frame : {10u, 11u}) {
                                        key.frames = frame;
                                        keyOwner.state().field8cc = 17;
                                        keyOwner.state().field8d0 = 1;
                                        key.behavior();
                                        require(keyActor.animationState == unsigned(frame == 10 ? noCollision : normal) &&
                                            keyOwner.state().field8cc == 0 && keyOwner.state().field8d0 == 0,
                                            "Dropped key collision delay includes frame ten and restores NORMAL at frame eleven.");
                                    }
                                    keyActor.visibility(0, 0);
                                    const auto physicsFrames = key.physics.frames;
                                    key.behavior();
                                    require(key.physics.frames == physicsFrames, "Fully transparent keys pause active physics and suction.");
                                    keyActor.visibility(255, 255);
                                    keyOwner.source = {&placedSource, 0};
                                    key.flags = 1;
                                    key.state = 0;
                                    key.nextState = 0xffff;
                                    key.initializeBehavior();
                                    key.behavior();
                                    require(key.flags == 3 && key.nextState == 1 && globalSounds.back() == 0x381c,
                                        "Placed keys retain nested appearance routing and the exact 3_1 creation sound.");
                                    key.setNextState(3);
                                    key.update();
                                    keyActor.selectAnimation(static_cast<unsigned>(normal));
                                    key.frames = 10;
                                    keyOwner.state().field8cc = 9;
                                    key.behavior();
                                    require(keyActor.animationState == unsigned(normal) && keyOwner.state().field8cc == 9,
                                        "Placed keys do not apply the early dropped-key collision override.");
                                    for (unsigned i = 0; i < key.physics.parameters.entries().size(); ++i) key.physics.parameters.setBits(i, 0);
                                    key.physics.state = 2;
                                    key.physics.nextState = 0xffff;
                                    keyActor.flags |= 0x10000;
                                    keyActor.fieldB0 = keyActor.fieldB2 = -1;
                                    keyActor.setVelocity({0, 4, 0});
                                    const auto landSounds = itemSounds.calls.size();
                                    key.behavior();
                                    require(itemSounds.calls.size() == landSounds + 1 && itemSounds.calls.back() == 0x1829 && key.groundedFrames != 0,
                                        "Grounded keys use the original key landing sound.");
                                    keyActor.setVelocity({0, 3, 0});
                                    key.behavior();
                                    require(itemSounds.calls.size() == landSounds + 1, "Key landing sound excludes exactly three units of vertical speed.");
                                    key.frames = 600;
                                    key.lifetime.remaining = 1;
                                    keyActor.fieldB4 ^= 0x10000;
                                    key.behavior();
                                    require(key.lifetime.remaining == 1 && !key.expired && !keyOwner.state().field91c,
                                        "Keys retain infinite lifetime without Mario-item room-change notifications.");
                                    characterevent::Event end(4);
                                    require(key.captureEnd(end) == 0, "Uncollected key capture-end effects do not award a door.");
                                    firstPlayer.life = 0;
                                    require(!key.collectItem() && !key.awarded, "Refused key collection leaves the award latch unset.");
                                    firstPlayer.life = 100;
                                    firstPlayer.state.field1060 = 0;
                                    const auto effectsBefore = ownerParticles.history;
                                    require(key.collectItem() && key.awarded && key.collected && ownerParticles.history == effectsBefore,
                                        "Accepted key collection sets its latch without Mario-item collection particles.");
                                    require(!key.collectItem(), "Already collected keys cannot be collected twice.");
                                    key.itemSource = {&droppedSource, 0};
                                    keyBits.data[0] = 0;
                                    keyBits.data[0x21 >> 3] = 0;
                                    keyOwner.event(4);
                                    require((keyBits.data[0] & 8) && !(keyBits.data[0x21 >> 3] & 2) && firstPlayer.state.field1068 == 3,
                                        "Owner capture-end awards placement OpenDoorNo before ItemInfoTable and truncates to one byte.");
                                    keyOwner.source = {};
                                    keyOwner.event(4);
                                    require((keyBits.data[0x21 >> 3] & 2) && firstPlayer.state.field1068 == 0x21,
                                        "Dropped key capture-end falls back to its retained ItemInfoTable door.");
                                    key.itemSource = {};
                                    firstPlayer.state.field1061 = 0;
                                    require(key.captureEnd(end) == 1 && firstPlayer.state.field1061 == 0,
                                        "An awarded key with no door metadata completes without inventing a door number.");
                                    key.flags = 0x11;
                                    keyOwner.state().field91c = false;
                                    keyOwner.event(10);
                                    require(!(key.flags & 1) && keyOwner.state().field91c,
                                        "Key room notifications retain shared owner removal and effect suppression.");
                                    keyOwner.state().field91c = false;
                                    key.flags = 0x17;
                                    key.collected = key.awarded = false;
                                    firstPlayer.state.field1060 = 0;
                                    firstPlayer.state.field1064 = nullptr;
                                    firstPlayer.view.field8C9 = 0;
                                    firstPlayer.view.field8A0 = 1;
                                    firstPlayer.view.field8B4 = firstPlayer.components->vacuum->model.get();
                                    keyActor.setPosition({float(bounds[0] + bounds[3]) / 2, float(bounds[1] + bounds[4]) / 2,
                                        float(bounds[2] + bounds[5]) / 2});
                                    keyActor.fieldB4 = roomId;
                                    key.setNextState(3);
                                    key.update();
                                    key.frames = 91;
                                    origin = keyActor.position;
                                    origin[2] -= 10;
                                    key.update();
                                    require(key.collected && key.awarded && firstPlayer.state.field1064 == &keyActor,
                                        "Native key suction completion reaches the shared player event and the key-specific award hook.");
                                    firstPlayer.view.field8C9 = 1;
                                    boundEnemies.deactivate(id);
                                }
                                require(keyTypes == 5, "All five original key types use the recovered EnKey factory.");
                                unsigned routedKeys = 0;
                                for (unsigned i = 0; i < keyBits.bytes; ++i) keyBits.data[i] = 0;
                                for (unsigned row = 0; row < itemInfo->count(); ++row) {
                                    const jmp::ToolDataRef source{itemInfo, static_cast<int>(row)};
                                    if (!admission.key(admission.type(source))) continue;
                                    const auto id = itemCreation.create(source, spawnPosition, &launchVelocity);
                                    if (id == 0xffffffff) continue;
                                    auto& owner = dynamic_cast<characterowner::Owner&>(*boundEnemies.activeOwner(*boundRegistry.actor(id)));
                                    const auto& key = dynamic_cast<enemystrategy::Key&>(*owner.ownership().strategy);
                                    require(jmp::sameEntry(key.itemSource, source) && !owner.source.table,
                                        "Original key table routing retains door metadata separately from placement ownership.");
                                    ++routedKeys;
                                    boundEnemies.deactivate(id);
                                }
                                require(routedKeys != 0, "Imported item tables exercise manager-owned key creation.");
                                flags = savedFlags;
                            }
                            {
                                std::uint32_t mansionMode = 0;
                                std::int32_t heartAudioLife = -1;
                                std::vector<unsigned> healthSounds;
                                unsigned deathSounds = 0, moneyCount = 0;
                                playerevents::LifeEnvironment health{heartAudioLife, ownerParticles,
                                    [&](unsigned sound, const itemsuction::Vector& position) {
                                        require(position == firstPlayer.position, "Health audio retains player position.");
                                        healthSounds.push_back(sound);
                                    },
                                    [&](const itemsuction::Vector&) { ++deathSounds; },
                                    [&](const itemsuction::Vector& position, const itemsuction::Vector& direction,
                                        std::uint8_t count, std::uint16_t frames, double speed, std::uint8_t kind) {
                                        require(position == firstPlayer.components->weapon->first->position &&
                                            direction == firstPlayer.components->weapon->first->field610 &&
                                            frames == 150 && speed == 30 && kind == 0,
                                            "Health-loss scatter uses the original weapon vectors and manager arguments.");
                                        moneyCount += count;
                                    }};
                                itemEnvironment.life = &health;
                                itemEnvironment.mansionMode = &mansionMode;
                                gamearchive::Resources heartResources(ownerMission.archive());
                                std::vector<std::unique_ptr<modeleffects::Pool>> heartPools;
                                for (const auto& definition : modeleffects::readDefinitions(dol, 2)) {
                                    if (definition.type < 0x19 || definition.type > 0x1b) continue;
                                    auto pool = std::make_unique<modeleffects::Pool>(definition);
                                    pool->load(heartResources, defaults);
                                    ownerModels.add(*pool);
                                    heartPools.push_back(std::move(pool));
                                }
                                {
                                    particles::EmitterPool lifeEmitters(1, 0, 0);
                                    particles::EffectFactory lifeParticles(dol, ownerArchive, lifeEmitters);
                                    const itemsuction::Vector position{2, 3, 4};
                                    auto* emitter = lifeParticles.createLifeLoss(0, position, -10);
                                    require(emitter && emitter->position == position && emitter->field220 == itemsuction::Vector{} &&
                                        emitter->field22C == itemsuction::Vector{} && !emitter->fieldC4 && !emitter->fieldC8,
                                        "Health-loss particles load the original resources and retain their callback-free emitter setup.");
                                    float scale;
                                    std::memcpy(&scale, &emitter->parameters.fields10C[8], sizeof(scale));
                                    require(scale == 0.45f + 0.04f * 10 && lifeParticles.history[0] == 210,
                                        "Health-loss particle scale uses absolute damage and original single-precision arithmetic.");
                                    require(!lifeParticles.createLifeLoss(2, position, 1) && lifeParticles.history[0] == 212,
                                        "Exhausted health particle pools preserve history without creating substitute emitters.");
                                }
                                firstPlayer.state.maximumLife = 100;
                                firstPlayer.life = 50;
                                {
                                    const auto retainedState = firstPlayer.state;
                                    const auto retainedActions = *firstPlayer.components->actions;
                                    const auto retainedControls = *firstPlayer.components->controls;
                                    const auto retainedSupport = firstPlayer.fieldA0;
                                    const auto retainedFloor = firstPlayer.fieldB2;
                                    const auto retainedFlags = firstPlayer.flags;
                                    const auto retainedReaction = firstPlayer.components->reactions->state;
                                    playercontrols::Frame frame(dol);
                                    auto& actions = *firstPlayer.components->actions;
                                    auto& controls = *firstPlayer.components->controls;
                                    actions.field310 = 1;
                                    actions.field30D = 0;
                                    actions.fields31C[4] = 0;
                                    controls.flagsC = {};
                                    firstPlayer.fieldA0 = -1;
                                    firstPlayer.fieldB2 = 0;
                                    firstPlayer.components->reactions->state = 1;
                                    playeractions::updateState(firstPlayer, health);
                                    require(firstPlayer.life == 50 && firstPlayer.state.maximumLife == 100 && firstPlayer.state.field1000 == 50 &&
                                        healthSounds.empty(), "Normal reaction frame uses its imported life multiplier without emitting a health change.");
                                    controls.flagsC[13] = 1;
                                    firstPlayer.state.field1114 = 0;
                                    frame.charge(firstPlayer, health);
                                    require(firstPlayer.state.field1114 == 0 && healthSounds == std::vector<unsigned>{0x2814},
                                        "Charge onset plays its sound before the same-frame countdown.");
                                    healthSounds.clear();
                                    controls.flagsC[13] = 0;
                                    controls.flagsC[14] = 1;
                                    firstPlayer.state.field1114 = 0.5f;
                                    firstPlayer.state.field1110 = 1;
                                    frame.charge(firstPlayer, health);
                                    require(firstPlayer.state.field1114 == 1 && healthSounds == std::vector<unsigned>{0x2805},
                                        "Charge hold retains its clock and original repeating sound.");
                                    healthSounds.clear();
                                    controls.flagsC[12] = 1;
                                    frame.attack(firstPlayer);
                                    require(firstPlayer.state.field110C == 5 && firstPlayer.state.field1114 == 0,
                                        "Attack starts the imported six-frame clock and advances it immediately.");
                                    actions.field310 = 2;
                                    actions.field314 = 1;
                                    firstPlayer.state.field110C = 0.25f;
                                    frame.attack(firstPlayer);
                                    require(firstPlayer.state.field110C == 1, "Held attack preserves the expiry latch outside ordinary action mode.");
                                    controls.flagsC[12] = 0;
                                    frame.attack(firstPlayer);
                                    require(firstPlayer.state.field110C == 0, "Released attack expires without a hold latch.");
                                    actions.field310 = 1;
                                    firstPlayer.state.field10C4 = 2;
                                    frame.blink(firstPlayer, 1);
                                    require(firstPlayer.state.field10C4 == 1 && firstPlayer.state.fieldF5C == 127,
                                        "Damage blink uses original simulation parity and decrements its clock.");
                                    actions.fields31C[4] = 10;
                                    frame.blink(firstPlayer, 3);
                                    require(firstPlayer.state.field10C4 == 1 && firstPlayer.state.fieldF5C == 255,
                                        "Protected normal modes suspend damage blinking and its clock.");
                                    std::uint8_t specialActive = 1;
                                    firstPlayer.state.field1338 = 2;
                                    firstPlayer.state.field133C = 1;
                                    firstPlayer.flags |= 0x20000;
                                    frame.special(firstPlayer, 1, false, specialActive, health);
                                    require(firstPlayer.state.field1338 == 1 && !firstPlayer.state.field133C,
                                        "Special animation counter expires only at its original animation marker.");
                                    firstPlayer.state.field1338 = 3;
                                    actions.field310 = 2;
                                    actions.field314 = 23;
                                    frame.special(firstPlayer, 1, false, specialActive, health);
                                    require(specialActive && firstPlayer.state.field1338 == 3,
                                        "Protected special reaction retains the shared state.");
                                    actions.field314 = 1;
                                    frame.special(firstPlayer, 1, false, specialActive, health);
                                    require(!specialActive && firstPlayer.state.field1338 == 1,
                                        "Leaving the special reaction clears the shared state.");
                                    firstPlayer.state.field1338 = 4;
                                    firstPlayer.state.field1340 = 3;
                                    firstPlayer.state.field1344 = 29;
                                    frame.special(firstPlayer, 1, false, specialActive, health);
                                    require(firstPlayer.state.field1344 == 30 && std::abs(firstPlayer.state.field1340 - 2.9f) < 0.00001f,
                                        "Special duration starts its imported decay on the thirtieth idle frame.");
                                    controls.flagsC[16] = 1;
                                    frame.special(firstPlayer, 1, false, specialActive, health);
                                    require(firstPlayer.state.field1344 == 0 && firstPlayer.state.field10C8 == 2 &&
                                        firstPlayer.state.field10CC == 0.5f && healthSounds == std::vector<unsigned>{0x814},
                                        "Special input produces the original sound, feedback timers and idle reset.");
                                    healthSounds.clear();
                                    firstPlayer.state = retainedState;
                                    *firstPlayer.components->actions = retainedActions;
                                    *firstPlayer.components->controls = retainedControls;
                                    firstPlayer.fieldA0 = retainedSupport;
                                    firstPlayer.fieldB2 = retainedFloor;
                                    firstPlayer.flags = retainedFlags;
                                    firstPlayer.components->reactions->state = retainedReaction;
                                }
                                {
                                    const auto retainedState = firstPlayer.state;
                                    const auto retainedActions = *firstPlayer.components->actions;
                                    const auto retainedControls = *firstPlayer.components->controls;
                                    const auto retainedTransition = firstPlayer.components->attached->field2C0;
                                    const auto retainedAudioLife = heartAudioLife;
                                    playeractor::TimerFrame frame(dol);
                                    std::vector<unsigned> calls;
                                    auto timerLife = health;
                                    timerLife.sound = [&](unsigned id, const std::array<float, 3>& position) {
                                        require(position == firstPlayer.position, "Timer sounds use the player's retained position.");
                                        calls.push_back(id);
                                    };
                                    playeractor::TimerServices services{timerLife, false, 1,
                                        [&](bool enabled) { require(!enabled, "Expired power state restores the original audio rate."); calls.push_back(1); },
                                        [&](playeractor::Player& player, const std::array<float, 3>& origin, std::uint32_t damage) {
                                            require(&player == &firstPlayer && damage == 7, "Delayed damage preserves its original amount and actor.");
                                            for (unsigned i = 0; i < 3; ++i) require(origin[i] == player.position[i] - player.state.direction[i],
                                                "Delayed damage retains the original behind-player event origin.");
                                            calls.push_back(2);
                                        },
                                        [&](playeractor::Player& player, std::uint32_t color, unsigned duration, unsigned amount, unsigned kind) {
                                            require(&player == &firstPlayer && color == 0xff5f1fff && duration == 300 && amount == 99 && kind == 15,
                                                "Delayed damage preserves the original weapon-pulse arguments.");
                                            calls.push_back(3);
                                        },
                                        [&](unsigned type, const actormodel::Matrix& matrix) {
                                            require(type == 4 && matrix == firstPlayer.state.matrix1010,
                                                "Delayed damage submits its retained matrix effect.");
                                            calls.push_back(4);
                                        }};
                                    auto& state = firstPlayer.state;
                                    firstPlayer.components->actions->field310 = 1;
                                    firstPlayer.components->attached->field2C0 = 0;
                                    firstPlayer.components->controls->field20 = firstPlayer.components->controls->field2C = 0;
                                    firstPlayer.components->controls->flagsC[16] = 0;
                                    state.fieldFB8 = state.fieldFBC = nullptr;
                                    state.field11A0 = nullptr;
                                    state.flags1140 = {};
                                    state.field106C = state.field1174 = state.field1170 = 0.5f;
                                    state.field118C = state.field105C = state.field117C = 0.5f;
                                    state.field1124 = 0.25f;
                                    state.field1128 = 7;
                                    state.field1130 = state.field1348 = 0;
                                    state.field1138 = 10;
                                    state.field10C8 = 0.25f;
                                    state.field10CC = 0.5f;
                                    state.field116C = 2;
                                    frame.run(firstPlayer, services);
                                    require(calls == std::vector<unsigned>{0x846, 1, 2, 3, 0x2809, 0x80a, 4},
                                        "Timer expiry preserves power-audio, damage, weapon, sound and matrix-effect ordering.");
                                    require(state.field1174 == 20 && state.field1170 == 0 && state.field1124 == -0.75f &&
                                        state.field106C == 0 && state.field118C == 0 && state.field105C == 0 &&
                                        state.field117C == 0 && state.field1138 == 0 && state.field10C8 == 0 && state.field116C == 3,
                                        "Timer stage preserves same-frame power expiry, the unclamped damage countdown and idle accumulation.");
                                    calls.clear();
                                    state.field1130 = 3;
                                    state.field113C = 29;
                                    frame.run(firstPlayer, services);
                                    require(state.field113C == 30 && std::abs(state.field1130 - 2.9f) < 0.00001f && !state.flags1140[0] && calls.empty(),
                                        "Escape decay begins at the original idle threshold without repeating expired damage.");
                                    firstPlayer.components->controls->flagsC[16] = 1;
                                    frame.run(firstPlayer, services);
                                    require(state.field113C == 0 && state.field1138 == 5 && state.flags1140[0],
                                        "Escape input resets inactivity and immediately advances its six-frame pulse.");
                                    state.field1130 = 0;
                                    state.field1348 = 0.5f;
                                    state.fields12F4[9] = 0;
                                    services.frame = 5;
                                    frame.run(firstPlayer, services);
                                    require(firstPlayer.life == 49 && state.field1348 == 0 && calls == std::vector<unsigned>{0x839},
                                        "Periodic damage still fires on the timer's final fractional frame before clamping.");
                                    firstPlayer.life = 50;
                                    heartAudioLife = retainedAudioLife;
                                    firstPlayer.state = retainedState;
                                    *firstPlayer.components->actions = retainedActions;
                                    *firstPlayer.components->controls = retainedControls;
                                    firstPlayer.components->attached->field2C0 = retainedTransition;
                                }
                                {
                                    const auto retainedState = firstPlayer.state;
                                    const auto retainedActions = *firstPlayer.components->actions;
                                    const auto retainedControls = *firstPlayer.components->controls;
                                    const auto retainedReaction = firstPlayer.components->reactions->state;
                                    auto& attached = *firstPlayer.components->attached;
                                    const auto upper = attached.field2C0;
                                    const auto lower = attached.field2D0;
                                    const std::array<float, 5> clocks{attached.field2C4, attached.field2C8, attached.field2CC,
                                        attached.field2D4, attached.field2D8};
                                    playeractor::EquipmentFrame equipment(dol);
                                    auto& state = firstPlayer.state;
                                    auto& actions = *firstPlayer.components->actions;
                                    auto& controls = *firstPlayer.components->controls;
                                    unsigned sounds = 0;
                                    const auto sound = [&](unsigned id, const std::array<float, 3>& position) {
                                        require(id == 0x849 && position == firstPlayer.position,
                                            "Equipment release uses its original sound and player position.");
                                        ++sounds;
                                    };
                                    state.field7A4 = nullptr;
                                    state.flags792 = 0;
                                    state.field1180 = 6;
                                    state.flags10E8 = {};
                                    equipment.run(firstPlayer, sound);
                                    require(state.field1180 == 6, "Absent equipment capabilities preserve the retained selection.");
                                    state.flags792 = 12;
                                    state.flagsFC0[1] = state.field1334 = state.flags1140[2] = state.flags1330[2] = 0;
                                    state.field1008 = state.field1130 = state.field1170 = 0;
                                    state.field119D = 1;
                                    actions.field310 = 1;
                                    actions.fields31C[4] = 0;
                                    firstPlayer.components->reactions->state = 1;
                                    attached.field2C0 = 0;
                                    controls.field20 = 1;
                                    controls.flags30[2] = 0;
                                    state.field1180 = 0;
                                    equipment.run(firstPlayer, sound);
                                    require(state.field1180 == 1, "Original suction input takes priority over other equipment.");
                                    controls.field20 = 0;
                                    controls.field2C = 1;
                                    state.field1184 = 1;
                                    state.field1180 = 0;
                                    equipment.run(firstPlayer, sound);
                                    require(state.field1180 == 6, "The original default emission selection maps to mode six.");
                                    controls.field2C = 0;
                                    state.field1180 = 0;
                                    controls.settings.fields[27] = 0;
                                    controls.flags24[2] = 1;
                                    state.field119E = 1;
                                    equipment.run(firstPlayer, sound);
                                    require(state.field1180 == 5 && !state.field119E,
                                        "Light mode applies the original toggle control after selection.");
                                    for (unsigned blocked : {22U, 4U, 5U, 6U, 7U, 12U, 25U, 26U, 23U, 24U, 28U, 34U}) {
                                        actions.field310 = 2;
                                        actions.field314 = blocked;
                                        equipment.run(firstPlayer, sound);
                                        require(!state.field1180, "Protected original action states suppress equipment selection.");
                                    }
                                    actions.field314 = 32;
                                    state.field1180 = 5;
                                    equipment.run(firstPlayer, sound);
                                    require(state.field1180 == 5, "Door interaction preserves selection instead of clearing it.");
                                    state.flags792 = 0;
                                    state.flags10E8[1] = 1;
                                    state.fields10EC.fill(nullptr);
                                    state.flags1040[2] = 0;
                                    equipment.run(firstPlayer, sound);
                                    require(!state.flags10E8[1] && state.flags10E8[2] && sounds == 1,
                                        "Capture release still executes when the capability selection block is skipped.");
                                    firstPlayer.state = retainedState;
                                    *firstPlayer.components->actions = retainedActions;
                                    *firstPlayer.components->controls = retainedControls;
                                    firstPlayer.components->reactions->state = retainedReaction;
                                    attached.field2C0 = upper;
                                    attached.field2D0 = lower;
                                    attached.field2C4 = clocks[0]; attached.field2C8 = clocks[1]; attached.field2CC = clocks[2];
                                    attached.field2D4 = clocks[3]; attached.field2D8 = clocks[4];
                                }
                                {
                                    const auto retainedState = firstPlayer.state;
                                    const auto retainedActions = *firstPlayer.components->actions;
                                    const auto retainedWall = firstPlayer.fieldB0;
                                    const auto retainedFloor = firstPlayer.fieldB2;
                                    const auto retainedSupport = firstPlayer.fieldA0;
                                    const auto retainedLife = firstPlayer.life;
                                    const auto retainedAudioLife = heartAudioLife;
                                    const auto retainedEffects = ownerParticles.history;
                                    auto& actions = *firstPlayer.components->actions;
                                    playeractor::ContactReaction reaction(dol);
                                    gameplaycamera::CameraOwner camera;
                                    camera.position = {123, 456, 789};
                                    mapcollision::State wallCollision(ownerMission);
                                    std::vector<std::uint8_t> wallBytes(104);
                                    word(wallBytes, 0x28, 64);
                                    word(wallBytes, 0x2c, 80);
                                    word(wallBytes, 64, 0x3f800000);
                                    wallCollision.load(wallBytes);
                                    firstPlayer.fieldB0 = 0;
                                    actions.field310 = 2;
                                    actions.field314 = 4;
                                    actions.fields31C[6] = 6;
                                    firstPlayer.state.direction = {1, 0, 0};
                                    reaction.run(firstPlayer, wallCollision, camera, false, health);
                                    require(actions.field314 == 7 && ownerParticles.history[0] == 0x3b,
                                        "An aligned wall impact dispatches the original reaction and impact particle.");
                                    ownerParticles.history = retainedEffects;
                                    firstPlayer.fieldB0 = -1;
                                    actions.field310 = 2;
                                    actions.field314 = 25;
                                    reaction.run(firstPlayer, itemCollision, camera, false, health);
                                    require(firstPlayer.state.field1070 == 5 && firstPlayer.state.field1074 ==
                                        std::array<float, 3>{123, firstPlayer.position[1], 789} && firstPlayer.state.field109C == 0.18f,
                                        "Contact reaction retains the camera-facing target at player height.");
                                    actions.field314 = 31;
                                    firstPlayer.fieldB2 = -1;
                                    firstPlayer.fieldA0 = -1;
                                    reaction.run(firstPlayer, itemCollision, camera, false, health);
                                    require(firstPlayer.life == retainedLife && ownerParticles.history == retainedEffects,
                                        "The landing reaction waits for actual floor or actor support.");
                                    firstPlayer.fieldA0 = 0;
                                    firstPlayer.state.fields12F4[9] = 0;
                                    reaction.run(firstPlayer, itemCollision, camera, false, health);
                                    require(firstPlayer.life == retainedLife - 15 && ownerParticles.history[0] == 210 &&
                                        ownerParticles.history[1] == 0x2e && ownerParticles.history[2] == 0x2d,
                                        "Actor-supported landing emits both original effects before applying fifteen damage.");
                                    firstPlayer.state = retainedState;
                                    *firstPlayer.components->actions = retainedActions;
                                    firstPlayer.fieldB0 = retainedWall;
                                    firstPlayer.fieldB2 = retainedFloor;
                                    firstPlayer.fieldA0 = retainedSupport;
                                    firstPlayer.life = retainedLife;
                                    heartAudioLife = retainedAudioLife;
                                    ownerParticles.history = retainedEffects;
                                    healthSounds.clear();
                                }
                                playerevents::recoverLife(firstPlayer, 10, health);
                                require(firstPlayer.life == 60 && heartAudioLife == 60 &&
                                    healthSounds == std::vector<unsigned>{0x282b, 0x83d}, "Healing preserves setter-before-pickup audio order.");
                                healthSounds.clear();
                                playerevents::recoverLife(firstPlayer, 0, health);
                                require(firstPlayer.life == 100 && healthSounds == std::vector<unsigned>{0x282b, 0x83d},
                                    "A zero recovery amount restores maximum life.");
                                healthSounds.clear();
                                heartAudioLife = 777;
                                playerevents::recoverLife(firstPlayer, 50, health);
                                require(firstPlayer.life == 100 && heartAudioLife == 777 && healthSounds == std::vector<unsigned>{0x83d},
                                    "Full-health pickup keeps its pickup sound without rewriting unchanged audio life.");
                                firstPlayer.state.fields12F4[9] = 20;
                                healthSounds.clear();
                                playerevents::recoverLife(firstPlayer, 0xfffffff6, health);
                                require(firstPlayer.life == 90 && heartAudioLife == 90 && moneyCount == 5 &&
                                    firstPlayer.state.fields12F4[9] == 15 && ownerParticles.history[0] == 210 &&
                                    healthSounds == std::vector<unsigned>{0x839, 0x83d},
                                    "Recovery retains original wrapping signed arithmetic and the health-loss dependency.");
                                playerevents::setLife(firstPlayer, 89, health);
                                require(moneyCount == 6, "One life lost scatters a coin only when the resulting life is odd.");
                                playerevents::setLife(firstPlayer, 88, health);
                                require(moneyCount == 6, "One life lost at even resulting life does not scatter money.");
                                firstPlayer.state.fields12F4[9] = 2;
                                playerevents::setLife(firstPlayer, 0, health);
                                require(deathSounds == 1 && firstPlayer.state.fields12F4[9] == 0 && moneyCount == 8,
                                    "Health loss caps money at the held count and forwards death audio after damage audio.");
                                firstPlayer.life = 100;
                                firstPlayer.state.flags1040[2] = firstPlayer.state.field1058 = firstPlayer.state.flags1140[2] = 0;
                                firstPlayer.state.field1170 = firstPlayer.state.field1174 = firstPlayer.state.field1130 = 0;
                                firstPlayer.state.field1338 = 1;
                                firstPlayer.state.flags10E8[1] = firstPlayer.state.field1060 = 0;
                                firstPlayer.state.field11A0 = nullptr;
                                firstPlayer.components->actions->field310 = 1;
                                firstPlayer.view.field8C9 = 1;
                                unsigned heartTypes = 0;
                                for (unsigned type = 0; type < 381; ++type) {
                                    if (strategies.constructor(strategies.select(type, {})) != glme01::heartItemFactory) continue;
                                    ++heartTypes;
                                    const auto id = itemCreation.create(type, spawnPosition, &launchVelocity);
                                    if (id == 0xffffffff) throw std::runtime_error("EnHeartItem allocation failed for type " + std::to_string(type) + " " + types.name(type));
                                    auto& heartActor = *boundRegistry.actor(id);
                                    auto& heartOwner = dynamic_cast<characterowner::Owner&>(*boundEnemies.activeOwner(heartActor));
                                    auto& heart = dynamic_cast<enemystrategy::Heart&>(*heartOwner.ownership().strategy);
                                    require(heart.healing.has_value(), "EnManager calls the heart begin hook before active processing.");
                                    heartActor.actorType = 0xcd;
                                    for (unsigned mode : {0u, 1u}) {
                                        mansionMode = mode;
                                        const unsigned threshold = mode ? 400 : 200;
                                        for (unsigned money : {threshold - 1, threshold, 0x80000000u, 0xffffffffu}) {
                                            firstPlayer.view.fieldsED4[0] = money;
                                            heart.begin();
                                            require(*heart.healing == (money == threshold ? 20u : 10u),
                                                "Small-heart recovery uses the cached counter, both mode thresholds and the signed-negative exception.");
                                        }
                                    }
                                    heartActor.actorType = 0xce;
                                    heart.begin();
                                    require(*heart.healing == 50, "Large hearts restore fifty life.");
                                    heartActor.actorType = type;
                                    firstPlayer.view.fieldsED4[0] = 0;
                                    heart.begin();
                                    const auto soundsBefore = itemSounds.calls.size();
                                    heart.update();
                                    require(heart.flags == 5 && heart.nextState == 3 && heartActor.field788 == &heart.volume &&
                                        heart.physics.parameters.loaded() && heart.lifetime.parameters.loaded(),
                                        "Hearts initialize nonanimated pickup, their collision override, throw parameters and finite lifetime.");
                                    const auto& values = heart.parameters.entries();
                                    require(heart.volume.field8 == 1 && *heart.volume.secondary == actorcollision::Record{
                                        values[2].bits, values[3].bits, values[4].vectorBits[0], values[4].vectorBits[1], values[4].vectorBits[2]},
                                        "Heart pickup cylinder copies the original radius, height and vector parameter.");
                                    heart.parameters.setBits(0, 0x3f800000);
                                    heart.parameters.setBits(1, 0x3f000000);
                                    heart.update();
                                    require(heart.angle == 1 && heart.omega == 0.5f && heartActor.rotation[2] == 0 &&
                                        itemSounds.calls.size() == soundsBefore + 1 && itemSounds.calls.back() == 0x1875,
                                        "Heart activation sets angular speed and applies the old angle before advancing it.");
                                    heart.update();
                                    require(heart.angle == 1.5f && heart.omega == 0.25f && heartActor.rotation[2] == 10430 &&
                                        heartActor.previousRotation[0] == 10430, "Heart spin decays and updates the original paired angle fields.");
                                    heart.angle = 20;
                                    heart.omega = 0;
                                    heart.behavior();
                                    require(heart.angle == static_cast<float>(20.0 - 6.28318530717958647692),
                                        "Heart angle wrapping subtracts a single double-precision cycle rather than taking a modulus.");
                                    heart.angle = -1;
                                    heart.behavior();
                                    require(heart.angle == -1, "Negative heart angles retain original one-sided wrapping.");
                                    heartActor.visibility(0, 0);
                                    const auto physicsFrames = heart.physics.frames;
                                    const auto remaining = heart.lifetime.remaining;
                                    heart.behavior();
                                    require(heart.physics.frames == physicsFrames + 1 && heart.lifetime.remaining == remaining - 1,
                                        "Invisible hearts continue physics and lifetime while suction stays suppressed.");
                                    heartActor.visibility(255, 255);
                                    for (unsigned i = 0; i < heart.physics.parameters.entries().size(); ++i) heart.physics.parameters.setBits(i, 0);
                                    heart.physics.state = 2;
                                    heart.physics.nextState = 0xffff;
                                    heartActor.flags |= 0x10000;
                                    heartActor.fieldB0 = heartActor.fieldB2 = -1;
                                    heartActor.setVelocity({0, 4, 0});
                                    auto landing = itemSounds.calls.size();
                                    heart.behavior();
                                    require(itemSounds.calls.size() == landing + 1 && itemSounds.calls.back() == 0x1876,
                                        "Hearts retain their distinct grounded bounce sound.");
                                    heartActor.setVelocity({0, 3, 0});
                                    landing = itemSounds.calls.size();
                                    heart.behavior();
                                    require(itemSounds.calls.size() == landing, "Heart bounce sound excludes the exact three-unit boundary.");
                                    heart.lifetime.parameters.setBits(2, 3);
                                    heart.lifetime.parameters.setBits(3, 50);
                                    heart.lifetime.remaining = 5;
                                    heart.lifetime.blink = 2;
                                    heart.behavior();
                                    require(heart.lifetime.blink == 0 && (heartActor.fieldE0 & 255) == 0,
                                        "Finite hearts hide on the exact original blink-frequency tick.");
                                    heart.lifetime.remaining = 1;
                                    heart.behavior();
                                    require(heart.expired && heartOwner.state().field91c && !(heart.flags & 1) && !heart.collectItem(),
                                        "Heart expiry suppresses completion particles and rejects pickup through the owner route.");
                                    heart.expired = false;
                                    heart.flags = 5;
                                    heartOwner.state().field91c = false;
                                    heartActor.visibility(255, 255);
                                    firstPlayer.life = 0;
                                    require(!heart.collectItem() && !heart.collected, "Dead-player refusal does not consume a heart.");
                                    firstPlayer.life = 40;
                                    healthSounds.clear();
                                    require(heart.collectItem() && heart.collected && heartOwner.state().field91c &&
                                        firstPlayer.life == 40 + *heart.healing && heart.angle == 0 && heart.omega == 0 &&
                                        healthSounds == std::vector<unsigned>{0x282b, 0x83d},
                                        "Heart collection heals through player event D after immediate owner notification and resets spin.");
                                    require(!heart.collectItem(), "Collected hearts cannot heal twice.");
                                    heart.collected = false;
                                    heartOwner.state().field91c = false;
                                    heart.flags = 5;
                                    firstPlayer.life = 10;
                                    firstPlayer.view.field8C9 = 0;
                                    firstPlayer.view.field8A0 = 1;
                                    firstPlayer.view.field8B4 = firstPlayer.components->vacuum->model.get();
                                    heartActor.setPosition({float(bounds[0] + bounds[3]) / 2, float(bounds[1] + bounds[4]) / 2,
                                        float(bounds[2] + bounds[5]) / 2});
                                    heartActor.fieldB4 = roomId;
                                    heart.setNextState(3);
                                    heart.update();
                                    heart.frames = 91;
                                    heart.lifetime.parameters.setBits(0, 90);
                                    origin = heartActor.position;
                                    origin[2] -= 10;
                                    heart.update();
                                    require(heart.collected && firstPlayer.life == 10 + *heart.healing,
                                        "Native heart suction completion reaches healing and owner removal.");
                                    firstPlayer.view.field8C9 = 1;
                                    boundEnemies.deactivate(id);
                                }
                                require(heartTypes != 0, "Imported strategy selectors exercise EnHeartItem.");
                                ownerModels.clear();
                            }
                            {
                                std::int32_t audioLife = 0, transitionFrames = 20;
                                float audioScale = 1;
                                unsigned scattered = 0;
                                std::vector<unsigned> sounds;
                                playerevents::LifeEnvironment health{audioLife, ownerParticles,
                                    [&](unsigned sound, const itemsuction::Vector&) { sounds.push_back(sound); }, {},
                                    [&](const itemsuction::Vector& position, const itemsuction::Vector& direction,
                                        std::uint8_t count, std::uint16_t duration, double speed, std::uint8_t kind) {
                                        require(position == firstPlayer.components->weapon->first->position &&
                                            direction == firstPlayer.components->weapon->first->field610 && duration == 150 && speed == 30 && kind == 0,
                                            "Mushroom money loss uses the original scatter-manager arguments.");
                                        scattered += count;
                                    }};
                                playerevents::ShrinkEnvironment shrink{health, transitionFrames, audioScale};
                                itemEnvironment.life = &health;
                                itemEnvironment.shrink = &shrink;
                                actortrail::Pool trails(0);
                                trails.load(common);
                                itemEnvironment.trails = &trails;
                                firstPlayer.state.flags1140[2] = 0;
                                firstPlayer.state.field1170 = firstPlayer.state.field1174 = 0;
                                firstPlayer.state.fields12F4[9] = 25;
                                firstPlayer.components->actions->field310 = 1;
                                require(playerevents::shrink(firstPlayer, 0.3f, 300, shrink) == 0 &&
                                    firstPlayer.state.field1170 == 300 && firstPlayer.state.field1174 == 20 &&
                                    firstPlayer.state.field1178 == 0.3f && firstPlayer.state.fields12F4[9] == 15 &&
                                    audioScale == 2 && scattered == 10 && sounds == std::vector<unsigned>{0x847},
                                    "Shrink event 38 retains its zero result, timers, ratio, sound, audio multiplier and ten-coin loss.");
                                playerevents::shrink(firstPlayer, 0.1f, 5, shrink);
                                require(scattered == 10 && firstPlayer.state.field1170 == 300, "An active shrink rejects another shrink event.");
                                firstPlayer.state.field1170 = 0;
                                firstPlayer.state.flags1140[2] = 1;
                                playerevents::shrink(firstPlayer, 0.1f, 5, shrink);
                                require(firstPlayer.state.field1170 == 0 && scattered == 10, "The player state gate refuses shrinking.");
                                firstPlayer.state.flags1140[2] = 0;
                                firstPlayer.components->actions->field310 = 2;
                                firstPlayer.components->actions->field35C = 4;
                                for (unsigned action : {0xcu, 0xeu}) {
                                    firstPlayer.components->actions->field314 = action;
                                    playerevents::shrink(firstPlayer, 0.1f, 5, shrink);
                                    require(firstPlayer.state.field1170 == 0 && scattered == 10, "Both protected action states refuse shrinking.");
                                }
                                firstPlayer.components->actions->field314 = 0xd;
                                playerevents::shrink(firstPlayer, -2, 0xffffffff, shrink);
                                require(firstPlayer.state.field1170 == -1 && firstPlayer.state.field1178 == -2 && scattered == 20,
                                    "Other action states retain signed duration conversion and unclamped scale ratios.");
                                firstPlayer.state.fields12F4[9] = 300;
                                require(playerevents::loseMoney(firstPlayer, 300, health) == 1 && scattered == 64 &&
                                    !firstPlayer.state.fields12F4[9], "Money loss subtracts the full count while scattering its low byte.");
                                firstPlayer.state.fields12F4[9] = 0x80000000;
                                require(!playerevents::loseMoney(firstPlayer, 10, health), "Negative signed balances do not scatter.");
                                firstPlayer.state.fields12F4[9] = 10;
                                require(!playerevents::loseMoney(firstPlayer, 0xffffffff, health), "Negative signed requests do not scatter.");
                                unsigned selector = 0;
                                while (selector < 187 && strategies.constructor(selector) != glme01::negativeItemFactory) ++selector;
                                require(selector < 187, "The imported strategy table contains EnNegativeItem.");
                                unsigned baseType = 0;
                                while (baseType < 381 && strategies.constructor(strategies.select(baseType, {})) != glme01::keyFactory) ++baseType;
                                require(baseType < 381, "A manager-owned actor is available for the original strategy replacement route.");
                                const auto id = boundEnemies.createActor(baseType, static_cast<int>(selector));
                                require(id != 0xffffffff, "EnManager allocates the replacement strategy owner.");
                                auto& object = *boundRegistry.actor(id);
                                auto& owner = dynamic_cast<characterowner::Owner&>(*boundEnemies.activeOwner(object));
                                auto& negative = dynamic_cast<enemystrategy::Negative&>(*owner.ownership().strategy);
                                object.setPosition(spawnPosition);
                                object.fieldB4 = rooms.resolve(spawnPosition);
                                object.visibility(255, 255);
                                firstPlayer.view.field8C9 = 1;
                                firstPlayer.view.fieldF15 = 0;
                                firstPlayer.life = 100;
                                firstPlayer.state.field1170 = firstPlayer.state.field1174 = 0;
                                firstPlayer.components->actions->field310 = 1;
                                negative.update();
                                require(negative.flags == 5 && negative.nextState == 3 && negative.physics.parameters.loaded() &&
                                    negative.lifetime.parameters.loaded() && itemEnvironment.negativeParameters.loaded() &&
                                    negative.trail.endpoints == actortrail::Endpoints{{{30, 30, 0}, {-30, -30, 0}}} &&
                                    negative.trail.countdown == 2 && negative.trail.type == 2,
                                    "Mushroom initialization loads the original shared, throw and finite parameters and joint-zero trail.");
                                const auto snapshot = negative.trail.color;
                                itemEnvironment.negativeParameters.setBits(2, 0);
                                require(negative.trail.color == snapshot, "Trail color is captured at initialization rather than read every frame.");
                                const auto before = itemSounds.calls.size();
                                negative.update();
                                require(itemSounds.calls.size() == before + 1 && itemSounds.calls.back() == 0x1898,
                                    "Mushroom activation plays its original spawn sound.");
                                const auto savedVisibility = visibility;
                                visibility.reset(0, static_cast<std::int16_t>(rooms.entries().size()));
                                negative.trail.reset();
                                const auto hiddenRemaining = negative.lifetime.remaining;
                                negative.behavior();
                                require(negative.trail.countdown == 2 && negative.lifetime.remaining == hiddenRemaining - 1,
                                    "Hidden rooms pause the mushroom trail while finite lifetime continues.");
                                std::uint32_t trailRoomFlags = 0;
                                std::uint16_t trailSavedFlags = 0;
                                visibility.mark(0, 2, object.fieldB4, trailRoomFlags, trailSavedFlags, 0, 0, 0, {}, {});
                                negative.behavior();
                                require(negative.trail.countdown == 1 && (visibility.record(object.fieldB4, 0).flags & 2),
                                    "Visible mushroom trails update even when the traversal marker bit is also set.");
                                visibility = savedVisibility;
                                actortrail::Controller trail;
                                trail.initialize(&object, {0, 0}, negative.trail.endpoints, 2, snapshot, 30, 0, 0);
                                trail.update(&trails);
                                const auto initialEndpoints = *trail.previous;
                                object.matrix[3] += 5;
                                trail.update(&trails);
                                require(trails.entries()[0].remaining == 30 && trails.entries()[0].vertices[0] == initialEndpoints[0] &&
                                    trails.entries()[0].vertices[10][0] == initialEndpoints[0][0] + 5,
                                    "Native trail endpoints follow the owned model's joint matrix and actor world matrix.");
                                unsigned draws = 0;
                                trails.draw([&](const actortrail::Segment& segment, const std::vector<std::uint8_t>& texture) {
                                    require(segment.type == 2 && !texture.empty(), "Mushroom trail draw submits its imported rampbase texture.");
                                    ++draws;
                                });
                                trails.update();
                                require(draws && trails.entries()[0].remaining == 29 && (trails.entries()[0].flags & 2),
                                    "Drawing enables original pool aging.");
                                object.matrix[3] -= 5;
                                for (unsigned i = 0; i < negative.physics.parameters.entries().size(); ++i) negative.physics.parameters.setBits(i, 0);
                                negative.physics.state = 2;
                                negative.physics.nextState = 0xffff;
                                object.flags |= 0x10000;
                                object.fieldB0 = object.fieldB2 = -1;
                                object.setVelocity({0, 4, 0});
                                auto bounce = itemSounds.calls.size();
                                negative.behavior();
                                require(itemSounds.calls.size() == bounce + 1 && itemSounds.calls.back() == 0x1876,
                                    "Grounded mushrooms use the original bounce sound.");
                                object.setVelocity({0, 3, 0});
                                bounce = itemSounds.calls.size();
                                negative.behavior();
                                require(itemSounds.calls.size() == bounce, "The exact three-unit bounce threshold remains silent.");
                                negative.lifetime.remaining = 1;
                                negative.behavior();
                                require(negative.expired && owner.state().field91c && !(negative.flags & 1),
                                    "Mushrooms expire through the original finite-item owner notification.");
                                negative.expired = false;
                                negative.flags = 5;
                                owner.state().field91c = false;
                                object.visibility(255, 255);
                                firstPlayer.state.flags1040[2] = firstPlayer.state.field1058 = 0;
                                firstPlayer.state.field1130 = 0;
                                firstPlayer.state.field1338 = 1;
                                firstPlayer.state.flags10E8[1] = firstPlayer.state.field1060 = 0;
                                firstPlayer.state.field11A0 = nullptr;
                                firstPlayer.life = 0;
                                require(!negative.collectItem(), "Dead players cannot consume a mushroom.");
                                firstPlayer.life = 100;
                                firstPlayer.state.fields12F4[9] = 7;
                                sounds.clear();
                                scattered = 0;
                                require(negative.collectItem() && negative.collected && owner.state().field91c &&
                                    firstPlayer.state.field1170 == float(itemEnvironment.negativeParameters.entries()[1].bits) &&
                                    !firstPlayer.state.fields12F4[9] && scattered == 7 && sounds == std::vector<unsigned>{0x847} &&
                                    ownerParticles.history[0] == 0x46,
                                    "Mushroom collection dispatches shrinking, clamps scatter to available money and emits completion particles.");
                                require(!negative.collectItem(), "Mushrooms cannot be collected twice.");
                                negative.collected = false;
                                owner.state().field91c = false;
                                firstPlayer.state.field1170 = firstPlayer.state.field1174 = 0;
                                firstPlayer.view.fieldF15 = 1;
                                sounds.clear();
                                ownerParticles.history.fill(123);
                                require(negative.collectItem() && sounds.empty() && !firstPlayer.state.field1170 &&
                                    ownerParticles.history[0] == 0x46 && ownerParticles.history[1] == 0x46 && ownerParticles.history[2] == 123,
                                    "The cached player flag suppresses mushroom dispatch and its particle, retaining both inherited owner particles.");
                                negative.collected = false;
                                owner.state().field91c = false;
                                firstPlayer.view.fieldF15 = 0;
                                firstPlayer.state.field1170 = 10;
                                ownerParticles.history.fill(123);
                                require(negative.collectItem() && sounds.empty() && firstPlayer.state.field1170 == 10 &&
                                    ownerParticles.history[0] == 0x46 && ownerParticles.history[1] == 0x46 &&
                                    ownerParticles.history[2] == 0x46 && ownerParticles.history[3] == 123,
                                    "A refused shrink event still emits its particle between the two inherited owner notifications.");
                                boundEnemies.deactivate(id);
                            }
                            {
                                gamearchive::Resources jewelResources(ownerMission.archive());
                                std::vector<std::unique_ptr<modeleffects::Pool>> jewelPools;
                                for (const auto& definition : modeleffects::readDefinitions(dol, 2)) {
                                    if (definition.type < 0xf || definition.type > 0x1f) continue;
                                    auto pool = std::make_unique<modeleffects::Pool>(definition);
                                    pool->load(jewelResources, defaults);
                                    ownerModels.add(*pool);
                                    jewelPools.push_back(std::move(pool));
                                }
                                particles::EmitterPool jewelEmitters(64, 0, 0);
                                particles::EffectFactory jewelParticles(dol, ownerArchive, jewelEmitters);
                                charactereffects::Resources jewelEffects(ownerModels, jewelParticles, ownerVariant);
                                jewelEffects.bind(boundEnemies, boundRegistry);
                                bindingOwners.itemResources = &jewelEffects;
                                firstPlayer.life = 100;
                                firstPlayer.state.flags1040[2] = firstPlayer.state.field1058 = firstPlayer.state.flags1140[2] = 0;
                                firstPlayer.state.field1170 = firstPlayer.state.field1174 = firstPlayer.state.field1130 = 0;
                                firstPlayer.state.field1338 = 1;
                                firstPlayer.state.flags10E8[1] = firstPlayer.state.field1060 = 0;
                                firstPlayer.state.field11A0 = nullptr;
                                firstPlayer.components->actions->field310 = 1;
                                firstPlayer.view.field8C9 = 1;
                                unsigned jewelTypes = 0;
                                for (unsigned type = 0; type < 381; ++type) {
                                    if (strategies.constructor(strategies.select(type, {})) != glme01::jewelFactory) continue;
                                    ++jewelTypes;
                                    const auto id = itemCreation.create(type, spawnPosition, &launchVelocity);
                                    require(id != 0xffffffff, "Jewel allocation requires and acquires the original model and particle resources.");
                                    auto& object = *boundRegistry.actor(id);
                                    auto& owner = dynamic_cast<characterowner::Owner&>(*boundEnemies.activeOwner(object));
                                    auto& jewel = dynamic_cast<enemystrategy::Jewel&>(*owner.ownership().strategy);
                                    jewel.update();
                                    require(jewel.flags == 7 && jewel.nextState == 3 &&
                                        jewel.lifetime.parameters.loaded() == (type == 0xca) && !jewel.physics.parameters.loaded(),
                                        "Jewels select direct animated pickup and only red diamonds load the special finite parameters.");
                                    const auto before = itemSounds.calls.size();
                                    jewel.update();
                                    require(jewel.angle == 0.06f && jewel.omega == 0.06f && object.rotation[2] == 0 &&
                                        itemSounds.calls.size() == before + (type == 0xcc ? 0 : 1),
                                        "Jewel activation advances its fixed angular rate and preserves the special silent type.");
                                    jewel.update();
                                    require(jewel.angle == 0.12f && jewel.omega == 0.06f && object.rotation[2] == 625 &&
                                        object.previousRotation[0] == 625, "Jewel rotation writes the previous angle without angular decay.");
                                    object.actorType = 0xcc;
                                    const auto silent = itemSounds.calls.size();
                                    jewel.setNextState(3);
                                    jewel.update();
                                    require(itemSounds.calls.size() == silent, "The inherited type CC branch suppresses jewel activation audio.");
                                    object.actorType = type;
                                    jewel.angle = 20;
                                    jewel.omega = 0;
                                    jewel.behavior();
                                    require(jewel.angle == static_cast<float>(20.0 - 6.28318530717958647692),
                                        "Jewel angle wrapping subtracts one double-precision cycle.");
                                    jewel.angle = -1;
                                    jewel.behavior();
                                    require(jewel.angle == -1, "Jewel angle wrapping retains negative angles.");
                                    object.visibility(0, 0);
                                    const auto remaining = jewel.lifetime.remaining, physicsFrames = jewel.physics.frames;
                                    jewel.behavior();
                                    require(jewel.lifetime.remaining == remaining - 1 && jewel.physics.frames == physicsFrames + 1,
                                        "Transparent jewels retain physics and finite countdown.");
                                    for (unsigned i = 0; i < jewel.physics.parameters.entries().size(); ++i) jewel.physics.parameters.setBits(i, 0);
                                    jewel.physics.state = 2;
                                    jewel.physics.nextState = 0xffff;
                                    object.flags |= 0x10000;
                                    object.fieldB0 = object.fieldB2 = -1;
                                    object.setVelocity({0, -4, 0});
                                    const auto bounce = itemSounds.calls.size();
                                    jewel.behavior();
                                    require(itemSounds.calls.size() == bounce + 1 && itemSounds.calls.back() == 0x1879,
                                        "Jewel landing sound uses absolute vertical velocity and the original sound ID.");
                                    object.setVelocity({0, 3, 0});
                                    jewel.behavior();
                                    require(itemSounds.calls.size() == bounce + 1, "The exact jewel bounce threshold stays silent.");
                                    jewel.lifetime.parameters.setBits(2, 3);
                                    jewel.lifetime.parameters.setBits(3, 50);
                                    jewel.lifetime.remaining = 5;
                                    jewel.lifetime.blink = 2;
                                    jewel.behavior();
                                    require(!(object.fieldE0 & 255) && !jewel.lifetime.blink, "Finite jewels blink on the original equality tick.");
                                    jewel.lifetime.remaining = 1;
                                    jewel.behavior();
                                    require(jewel.expired && owner.state().field91c && !(jewel.flags & 1) && !jewel.collectItem(),
                                        "Jewel expiry removes through the owner and prevents collection.");
                                    jewel.expired = false;
                                    jewel.flags = 7;
                                    owner.state().field91c = false;
                                    object.visibility(255, 255);
                                    firstPlayer.state.field1170 = 1;
                                    require(!jewel.collectItem(), "Animated jewel pickup respects the player's shrink gate.");
                                    firstPlayer.state.field1170 = 0;
                                    firstPlayer.state.field1060 = 0;
                                    firstPlayer.state.field11A0 = nullptr;
                                    jewel.angle = 2;
                                    jewel.omega = 0.06f;
                                    const auto counters = firstPlayer.state.fields12F4;
                                    require(jewel.collectItem() && jewel.collected && !jewel.angle && !jewel.omega &&
                                        firstPlayer.state.field1060 == 1 && firstPlayer.state.field1064 == &object && !owner.state().field91c,
                                        "Jewel collection stops spin and retains animated ownership instead of immediate removal.");
                                    const int counter = type == 0xc5 ? 3 : type == 0xc6 ? 4 : type == 0xc7 ? 5 :
                                        type == 0xc8 ? 6 : (type == 0xc9 || type == 0xcc) ? 7 : type == 0xca ? 8 : -1;
                                    if (counter >= 0) require(firstPlayer.state.fields12F4[counter] == counters[counter] + 1 &&
                                        award.kind == static_cast<unsigned>(counter) && award.frames == 100,
                                        "Jewels award their original counter and hundred-frame notification through player event 2D.");
                                    require(!jewel.collectItem(), "A collected jewel cannot award twice.");
                                    characterevent::Event capture(4);
                                    require(jewel.captureEnd(capture) == 1 && ownerParticles.history[0] == 0x46,
                                        "Jewel capture completion retains the inherited particle hook.");
                                    firstPlayer.state.field1060 = 0;
                                    firstPlayer.state.field1064 = nullptr;
                                    jewel.collected = false;
                                    firstPlayer.view.field8C9 = 0;
                                    firstPlayer.view.field8A0 = 1;
                                    firstPlayer.view.field8B4 = firstPlayer.components->vacuum->model.get();
                                    object.setPosition({float(bounds[0] + bounds[3]) / 2, float(bounds[1] + bounds[4]) / 2,
                                        float(bounds[2] + bounds[5]) / 2});
                                    object.fieldB4 = roomId;
                                    jewel.setNextState(3);
                                    jewel.update();
                                    jewel.frames = 91;
                                    jewel.lifetime.parameters.setBits(0, 90);
                                    origin = object.position;
                                    origin[2] -= 10;
                                    jewel.update();
                                    require(jewel.collected && firstPlayer.state.field1064 == &object,
                                        "Jewel suction reaches the original animated collection route.");
                                    firstPlayer.view.field8C9 = 1;
                                    firstPlayer.state.field1060 = 0;
                                    firstPlayer.state.field1064 = nullptr;
                                    boundEnemies.deactivate(id);
                                }
                                require(jewelTypes == 6, "All six imported jewel selectors exercise the base EnJewel factory.");
                                std::array<unsigned, 3> treasureTypes{};
                                auto& grades = *firstPlayer.components->bits[4];
                                const auto gradeAt = [&](unsigned index) {
                                    unsigned result = 0;
                                    for (unsigned bit = index * grades.width; bit < (index + 1) * grades.width; ++bit)
                                        result = (result << 1) | ((grades.data[bit >> 3] >> (bit & 7)) & 1);
                                    return result;
                                };
                                for (unsigned type = 0; type < 381; ++type) {
                                    const auto constructor = strategies.constructor(strategies.select(type, {}));
                                    const bool gold = constructor == glme01::goldFactory, pearl = constructor == glme01::pearlFactory,
                                        crown = constructor == glme01::crownFactory;
                                    if (!gold && !pearl && !crown) continue;
                                    ++treasureTypes[gold ? 0 : pearl ? 1 : 2];
                                    const auto id = itemCreation.create(type, spawnPosition, &launchVelocity);
                                    require(id != 0xffffffff, "Treasure actors allocate through the original manager and resource pools.");
                                    auto& object = *boundRegistry.actor(id);
                                    auto& owner = dynamic_cast<characterowner::Owner&>(*boundEnemies.activeOwner(object));
                                    auto& treasure = dynamic_cast<enemystrategy::Jewel&>(*owner.ownership().strategy);
                                    require((dynamic_cast<enemystrategy::Gold*>(&treasure) != nullptr) == gold &&
                                        (dynamic_cast<enemystrategy::Pearl*>(&treasure) != nullptr) == pearl &&
                                        (dynamic_cast<enemystrategy::Crown*>(&treasure) != nullptr) == crown &&
                                        owner.ownership().attachmentCount == 2,
                                        "Gold, pearl and crown factories retain distinct types and both native resource attachments.");
                                    enemystrategy::Pearl* pearlStrategy = dynamic_cast<enemystrategy::Pearl*>(&treasure);
                                    if (pearlStrategy) {
                                        require(pearlStrategy->sourceIndex == -1, "Pearls start without a source ghost grade index.");
                                        for (unsigned source = 0; source < 381; ++source) {
                                            pearlStrategy->bindSource(source);
                                            require(pearlStrategy->sourceIndex == creationProperties.savedIndex(source),
                                                "Pearl source binding uses the original twenty-three-entry saved-ghost table.");
                                        }
                                        pearlStrategy->bindSource(0xffffffff);
                                        require(pearlStrategy->sourceIndex == -1, "Unknown pearl sources retain the original missing-index value.");
                                    }
                                    treasure.update();
                                    require(treasure.flags == (crown ? 7u : 5u) && treasure.nextState == 3 &&
                                        treasure.lifetime.parameters.loaded() == pearl,
                                        "Gold and pearls clear animated pickup, pearls load their finite parameters, and crowns retain animated pickup.");
                                    auto soundCount = itemSounds.calls.size();
                                    treasure.update();
                                    require(treasure.angle == 0.06f && treasure.omega == 0.06f &&
                                        itemSounds.calls.size() == soundCount + (crown ? 0 : 1),
                                        "All three treasures retain jewel spin and the crown's silent activation.");
                                    for (unsigned i = 0; i < treasure.physics.parameters.entries().size(); ++i) treasure.physics.parameters.setBits(i, 0);
                                    treasure.physics.state = 2;
                                    treasure.physics.nextState = 0xffff;
                                    object.flags |= 0x10000;
                                    object.fieldB0 = object.fieldB2 = -1;
                                    object.setVelocity({0, -4, 0});
                                    if (crown) {
                                        object.parent = &firstPlayer;
                                        soundCount = itemSounds.calls.size();
                                        treasure.behavior();
                                        require(itemSounds.calls.size() == soundCount && !treasure.collectItem(),
                                            "Attached crowns suppress landing sound and reject collection.");
                                        object.parent = nullptr;
                                    }
                                    soundCount = itemSounds.calls.size();
                                    treasure.behavior();
                                    require(itemSounds.calls.size() == soundCount + 1 && itemSounds.calls.back() == (gold ? 0x18cbu : 0x1879u),
                                        "Gold uses its distinct landing sound while pearls and free crowns use jewel landing audio.");
                                    object.setVelocity({0, 3, 0});
                                    treasure.behavior();
                                    require(itemSounds.calls.size() == soundCount + 1, "Treasure bounce thresholds exclude exactly three units.");
                                    treasure.lifetime.remaining = 1;
                                    treasure.lifetime.blink = 2;
                                    object.visibility(255, 255);
                                    treasure.behavior();
                                    if (crown) require(!treasure.expired && treasure.lifetime.remaining == 1 && treasure.lifetime.blink == 2 &&
                                        !owner.state().field91c && (object.fieldE0 & 255) == 255,
                                        "Crowns never count down, blink or expire through the finite-item lifetime.");
                                    else require(treasure.expired && owner.state().field91c && !(treasure.flags & 1),
                                        "Gold and pearls expire through native owner removal.");
                                    treasure.expired = false;
                                    treasure.flags = crown ? 7 : 5;
                                    owner.state().field91c = false;
                                    object.visibility(255, 255);
                                    firstPlayer.state.field1060 = 0;
                                    firstPlayer.state.field1064 = nullptr;
                                    firstPlayer.state.field1170 = firstPlayer.state.field1174 = 0;
                                    firstPlayer.life = 0;
                                    const auto counterIndex = gold ? 2u : crown ? 7u : 9 + type - 0xc2;
                                    const auto priorCounter = firstPlayer.state.fields12F4[counterIndex];
                                    std::vector<std::uint8_t> priorGrades(grades.data, grades.data + grades.bytes);
                                    require(!treasure.collectItem() && !treasure.collected &&
                                        firstPlayer.state.fields12F4[counterIndex] == priorCounter &&
                                        std::equal(priorGrades.begin(), priorGrades.end(), grades.data),
                                        "Refused treasure pickup changes neither counters nor ghost grades.");
                                    firstPlayer.life = 100;
                                    if (pearlStrategy) {
                                        unsigned source = 0;
                                        while (source < 381 && creationProperties.savedIndex(source) != 0) ++source;
                                        require(source < 381, "The original first grade source is available.");
                                        pearlStrategy->bindSource(source);
                                        for (unsigned bit = 0; bit < grades.width; ++bit) grades.data[bit >> 3] &= ~(1u << (bit & 7));
                                    }
                                    treasure.angle = 2;
                                    ownerParticles.history.fill(123);
                                    require(treasure.collectItem() && treasure.collected && !treasure.angle && !treasure.omega &&
                                        firstPlayer.state.fields12F4[counterIndex] == priorCounter + 1 &&
                                        owner.state().field91c == !crown && firstPlayer.state.field1060 == (crown ? 1 : 0),
                                        "Treasure collection awards once, stops spin and preserves immediate versus animated owner handling.");
                                    if (pearlStrategy) require(gradeAt(0) == type - 0xc2 + 1,
                                        "Each pearl size promotes its source ghost to the original bronze, silver or gold grade.");
                                    if (crown) require(award.kind == 7 && award.frames == 100 && !(object.flags & 0xc000),
                                        "Crown collection retains its diamond counter, award notification and actor flag clearing.");
                                    require(!treasure.collectItem(), "Treasure cannot increment counters or grades twice.");
                                    if (pearlStrategy) {
                                        treasure.collected = false;
                                        playerevents::promoteGrade(grades, 0, 3);
                                        require(treasure.collectItem() && gradeAt(0) == 3, "A smaller pearl cannot downgrade an existing gold grade.");
                                        treasure.collected = false;
                                        pearlStrategy->bindSource(0xffffffff);
                                        priorGrades.assign(grades.data, grades.data + grades.bytes);
                                        require(treasure.collectItem() && std::equal(priorGrades.begin(), priorGrades.end(), grades.data),
                                            "Unattributed pearls award their counter without inventing a source grade.");
                                    }
                                    treasure.collected = false;
                                    owner.state().field91c = false;
                                    firstPlayer.state.field1060 = 0;
                                    firstPlayer.state.field1064 = nullptr;
                                    firstPlayer.view.field8C9 = 0;
                                    firstPlayer.view.field8A0 = 1;
                                    firstPlayer.view.field8B4 = firstPlayer.components->vacuum->model.get();
                                    object.setPosition({float(bounds[0] + bounds[3]) / 2, float(bounds[1] + bounds[4]) / 2,
                                        float(bounds[2] + bounds[5]) / 2});
                                    object.fieldB4 = roomId;
                                    treasure.setNextState(3);
                                    treasure.update();
                                    treasure.frames = 91;
                                    treasure.lifetime.parameters.setBits(0, 90);
                                    origin = object.position;
                                    origin[2] -= 10;
                                    ownerParticles.history.fill(123);
                                    treasure.update();
                                    require(treasure.collected && owner.state().field91c == !crown,
                                        "All three treasures collect through native suction and their original owner routes.");
                                    require(crown ? ownerParticles.history[0] == 123 :
                                        ownerParticles.history[0] == 0x46 && ownerParticles.history[1] == 0x46 && ownerParticles.history[2] == 123,
                                        "Finite items suppress generic suction particles while preserving their inherited removal particles.");
                                    firstPlayer.view.field8C9 = 1;
                                    firstPlayer.state.field1060 = 0;
                                    firstPlayer.state.field1064 = nullptr;
                                    boundEnemies.deactivate(id);
                                }
                                require(treasureTypes == std::array<unsigned, 3>{1, 3, 1},
                                    "Imported selectors exercise gold, all three pearl sizes and the crown.");
                                const auto elementFlags = flags;
                                unsigned elementTypes = 0;
                                for (unsigned type = 0; type < 381; ++type) {
                                    if (strategies.constructor(strategies.select(type, {})) != glme01::elementJewelFactory) continue;
                                    ++elementTypes;
                                    const unsigned unlock = type == 0x75 ? 0x2b : type == 0x76 ? 0x2d : 0x2c;
                                    flags.setOff(static_cast<std::uint8_t>(unlock));
                                    const auto id = itemCreation.create(type, spawnPosition, &launchVelocity);
                                    require(id != 0xffffffff, "Element jewels acquire their original model and particle attachments through EnManager.");
                                    auto& object = *boundRegistry.actor(id);
                                    auto& owner = dynamic_cast<characterowner::Owner&>(*boundEnemies.activeOwner(object));
                                    auto& element = dynamic_cast<enemystrategy::ElementJewel&>(*owner.ownership().strategy);
                                    require(!element.awarded && owner.ownership().attachmentCount == 2,
                                        "Element jewel construction starts with an unawarded latch and both native resources.");
                                    element.update();
                                    require(element.flags == 0x17 && element.nextState == 3 && !element.lifetime.parameters.loaded(),
                                        "Element initialization retains animated pickup, direct activation and room-removal notification.");
                                    const auto activationSounds = itemSounds.calls.size();
                                    element.update();
                                    require(element.angle == 0.06f && itemSounds.calls.size() == activationSounds + 1 &&
                                        itemSounds.calls.back() == 0x1875, "Element jewels inherit the original spin and activation sound.");
                                    element.lifetime.remaining = 1;
                                    element.lifetime.blink = 2;
                                    element.behavior();
                                    require(element.lifetime.remaining == 1 && element.lifetime.blink == 2 && !element.expired,
                                        "Element jewels never count down or blink through the finite lifetime routine.");
                                    for (unsigned i = 0; i < element.physics.parameters.entries().size(); ++i) element.physics.parameters.setBits(i, 0);
                                    element.physics.state = 2;
                                    element.physics.nextState = 0xffff;
                                    object.flags |= 0x10000;
                                    object.fieldB0 = object.fieldB2 = -1;
                                    object.setVelocity({0, -4, 0});
                                    const auto landingSounds = itemSounds.calls.size();
                                    element.behavior();
                                    require(itemSounds.calls.size() == landingSounds + 1 && itemSounds.calls.back() == 0x18bc,
                                        "Element jewels use their original landing sound above the absolute velocity threshold.");
                                    object.setVelocity({0, 3, 0});
                                    element.behavior();
                                    require(itemSounds.calls.size() == landingSounds + 1, "Element landing audio excludes the exact threshold.");
                                    characterevent::Event capture(4), departure(10);
                                    require(element.receive(capture) == 1 && !flags.value(static_cast<std::uint8_t>(unlock)),
                                        "Capture-end before collection retains its return value without granting an element.");
                                    ownerParticles.history.fill(123);
                                    require(element.receive(departure) == 1 && owner.state().field91c && !(element.flags & 1) &&
                                        ownerParticles.history[0] == 123 && !flags.value(static_cast<std::uint8_t>(unlock)),
                                        "Leaving the room removes an uncollected element jewel without completion particles or an unlock.");
                                    owner.state().field91c = false;
                                    element.flags = 0x17;
                                    object.visibility(255, 255);
                                    firstPlayer.state.field1060 = 0;
                                    firstPlayer.state.field1064 = nullptr;
                                    firstPlayer.state.field1170 = 1;
                                    require(!element.collectItem() && !element.awarded, "Refused animated pickup never sets the element award latch.");
                                    firstPlayer.state.field1170 = 0;
                                    const auto counters = firstPlayer.state.fields12F4;
                                    require(element.collectItem() && element.awarded && !element.angle && !element.omega &&
                                        !(element.flags & 0x10) && !owner.state().field91c && firstPlayer.state.field1064 == &object &&
                                        firstPlayer.state.fields12F4 == counters && !flags.value(static_cast<std::uint8_t>(unlock)),
                                        "Accepted element pickup stops spin and retains animated ownership without granting the flag early.");
                                    require(!element.collectItem() && element.receive(departure) == 0 && !owner.state().field91c,
                                        "Accepted element jewels reject duplicate pickup and stop responding to room departure.");
                                    ownerParticles.history.fill(123);
                                    require(owner.event(capture) == 1 && flags.value(static_cast<std::uint8_t>(unlock)) &&
                                        owner.state().field91c && ownerParticles.history[0] == 0x46,
                                        "Original owner capture-end dispatch emits the particle and grants the exact element flag.");
                                    element.flags &= ~1u;
                                    flags.setOff(static_cast<std::uint8_t>(unlock));
                                    ownerParticles.history.fill(123);
                                    require(element.receive(capture) == 1 && flags.value(static_cast<std::uint8_t>(unlock)) &&
                                        ownerParticles.history[0] == 123, "The award flag is independent of the particle-enable bit.");
                                    flags.setOff(0);
                                    object.actorType = 0;
                                    element.receive(capture);
                                    require(flags.value(0), "An unsupported element-jewel type retains the original flag-zero fallback.");
                                    object.actorType = type;
                                    element.collected = element.awarded = false;
                                    element.flags = 0x17;
                                    owner.state().field91c = false;
                                    flags.setOff(static_cast<std::uint8_t>(unlock));
                                    firstPlayer.state.field1060 = 0;
                                    firstPlayer.state.field1064 = nullptr;
                                    firstPlayer.view.field8C9 = 0;
                                    firstPlayer.view.field8A0 = 1;
                                    firstPlayer.view.field8B4 = firstPlayer.components->vacuum->model.get();
                                    object.setPosition({float(bounds[0] + bounds[3]) / 2, float(bounds[1] + bounds[4]) / 2,
                                        float(bounds[2] + bounds[5]) / 2});
                                    object.fieldB4 = roomId;
                                    element.setNextState(3);
                                    element.update();
                                    element.frames = 91;
                                    element.lifetime.parameters.setBits(0, 90);
                                    origin = object.position;
                                    origin[2] -= 10;
                                    ownerParticles.history.fill(123);
                                    element.update();
                                    require(element.collected && element.awarded && !flags.value(static_cast<std::uint8_t>(unlock)) &&
                                        ownerParticles.history[0] == 123, "Element suction reaches animated pickup without premature unlock or generic suction particles.");
                                    owner.event(capture);
                                    require(flags.value(static_cast<std::uint8_t>(unlock)), "Suction-collected elements unlock at original owner completion.");
                                    firstPlayer.view.field8C9 = 1;
                                    firstPlayer.state.field1060 = 0;
                                    firstPlayer.state.field1064 = nullptr;
                                    boundEnemies.deactivate(id);
                                }
                                require(elementTypes == 3, "All three original elemental-jewel selectors are exercised.");
                                std::uint32_t mansionMode = 0;
                                std::int32_t audioLife = 100;
                                playerevents::LifeEnvironment health{audioLife, ownerParticles,
                                    [&](unsigned sound, const itemsuction::Vector&) { itemSounds.calls.push_back(sound); },
                                    [](const itemsuction::Vector&) {},
                                    [](const itemsuction::Vector&, const itemsuction::Vector&, std::uint8_t, std::uint16_t, double, std::uint8_t) {}};
                                gameplaycamera::CameraOwner bombCamera;
                                jaudio::PositionOwners bombAudio;
                                actortrail::Pool randomTrails(0);
                                itemEnvironment.life = &health;
                                itemEnvironment.mansionMode = &mansionMode;
                                itemEnvironment.camera = &bombCamera;
                                itemEnvironment.audioPositions = &bombAudio;
                                itemEnvironment.trails = &randomTrails;
                                unsigned escapeTypes = 0, randomTypes = 0, bombTypes = 0;
                                for (unsigned type = 0; type < 381; ++type) {
                                    const auto constructor = strategies.constructor(strategies.select(type, {}));
                                    if (constructor != glme01::heartEscapeFactory && constructor != glme01::negativeRandomFactory && constructor != glme01::bombFactory) continue;
                                    firstPlayer.life = 100;
                                    firstPlayer.state.field1338 = 1;
                                    firstPlayer.state.field1060 = 0;
                                    firstPlayer.view.fieldF15 = 1;
                                    const auto id = itemCreation.create(type, spawnPosition, &launchVelocity);
                                    require(id != 0xffffffff, "Advanced items allocate through the original item manager route.");
                                    auto& object = *boundRegistry.actor(id);
                                    auto& owner = dynamic_cast<characterowner::Owner&>(*boundEnemies.activeOwner(object));
                                    auto& item = dynamic_cast<enemystrategy::FiniteItem&>(*owner.ownership().strategy);
                                    item.update();
                                    item.update();
                                    item.frames = 2;
                                    object.flags |= 0x10000;
                                    object.fieldB0 = object.fieldB2 = -1;
                                    item.physics.state = 2;
                                    item.physics.nextState = 0xffff;
                                    for (unsigned i = 0; i < item.physics.parameters.entries().size(); ++i) item.physics.parameters.setBits(i, 0);
                                    if (constructor == glme01::heartEscapeFactory) {
                                        ++escapeTypes;
                                        require(dynamic_cast<enemystrategy::HeartEscape*>(&item), "Escape hearts use the native derived strategy.");
                                        object.setVelocity({7, 5, 9});
                                        item.behavior();
                                        require(object.velocity == itemsuction::Vector{7, 5, 9}, "A missing escape field leaves the heart's horizontal motion unchanged.");
                                        std::vector<std::uint8_t> escapeData(32);
                                        word(escapeData, 0, 1); word(escapeData, 4, 1); word(escapeData, 8, 28); word(escapeData, 12, 4);
                                        word(escapeData, 16, hash("IsEscape")); word(escapeData, 20, 0xffffffff);
                                        word(escapeData, 24, 0); word(escapeData, 28, 1);
                                        jmp::ToolData escapeSource;
                                        require(escapeSource.attach(escapeData, error), error);
                                        item.itemSource = {&escapeSource, 0};
                                        object.position = *firstPlayer.view.position;
                                        object.position[0] += 10;
                                        for (float vertical : {4.0f, 5.0f, -5.0f}) {
                                            object.setVelocity({7, vertical, 9});
                                            item.behavior();
                                            const auto direction = pose.horizontalDirection(pose.angle(0, 10));
                                            const float speed = vertical > 4 ? 14.0f : 0.0f;
                                            require(object.velocity == itemsuction::Vector{speed * direction[0], vertical, speed * direction[2]},
                                                "Escape hearts flee the cached player position only above the strict upward bounce threshold.");
                                        }
                                        item.itemSource = {};
                                    } else if (constructor == glme01::negativeRandomFactory) {
                                        ++randomTypes;
                                        require(dynamic_cast<enemystrategy::NegativeRandom*>(&item), "Random mushrooms use the native derived strategy.");
                                        for (float vertical : {4.0f, 5.0f, -5.0f}) {
                                            randomState = 0;
                                            object.setVelocity({7, vertical, 9});
                                            item.behavior();
                                            const auto direction = pose.horizontalDirection(0xf35f);
                                            const float speed = vertical > 4 ? 14.0f : 0.0f;
                                            require(randomState == 0x3c6ef35f && object.velocity == itemsuction::Vector{speed * direction[0], vertical, speed * direction[2]},
                                                "Every mushroom landing consumes one original random value, preserves vertical speed, and applies the strict upward threshold.");
                                        }
                                    } else {
                                        ++bombTypes;
                                        auto& bomb = dynamic_cast<enemystrategy::Bomb&>(item);
                                        require(bomb.flags == 0x14 && bomb.baseScale == (type == 0xd0 ? 0.7f : 1.0f),
                                            "Bomb initialization retains direct finite pickup, room removal and type-specific scale.");
                                        object.setVelocity({0, 4, 0});
                                        bomb.behavior();
                                        require(!bomb.spin, "Bomb spin stops at the inclusive landing threshold.");
                                        characterevent::Event fire(13), water(15);
                                        bomb.receive(fire);
                                        require(bomb.phase == 1, "Fire schedules the original explosion phase.");
                                        bomb.behavior();
                                        require(bomb.phase == 2 && bomb.radius > 0 && !bomb.emitters[0] && !bomb.emitters[1] &&
                                            !(object.fieldE0 & 255) && (object.flags & 0xc400) == 0xc400 && bombCamera.fieldA8 == 100 &&
                                            ownerParticles.history[0] == (type == 0xd0 ? 252u : 251u),
                                            "Bomb explosion hides its actor, releases fuse emitters, expands collision and dispatches native effects and camera shake.");
                                        owner.state().field91c = false;
                                        const auto endFrame = itemEnvironment.bombParameters.entries()[8].bits;
                                        bomb.phase = endFrame;
                                        bomb.behavior();
                                        require(bomb.phase == endFrame + 1 && !owner.state().field91c, "Bomb lifetime retains the inclusive final frame.");
                                        bomb.behavior();
                                        require(owner.state().field91c, "Bomb removal follows the original owner notification.");
                                        owner.state().field91c = false;
                                        bomb.receive(water);
                                        require(owner.state().field91c && ownerParticles.history[0] == 2, "Water extinguishes bombs through particle and owner routes.");
                                        owner.state().field91c = false;
                                        bomb.phase = 0;
                                        const auto fuseFrame = itemEnvironment.bombParameters.entries()[type == 0xd0 ? 4 : 6].bits;
                                        bomb.frames = fuseFrame + 1;
                                        bomb.behavior();
                                        require(!bomb.phase, "Skipping the exact fuse frame preserves the original equality-only detonation check.");
                                        bomb.frames = fuseFrame;
                                        bomb.behavior();
                                        require(bomb.phase == 2, "The exact imported fuse frame detonates and advances the blast in the same update.");
                                        owner.state().field91c = false;
                                        bomb.phase = endFrame;
                                        bomb.radius = 1000;
                                        object.position = *firstPlayer.view.position;
                                        firstPlayer.view.fieldF15 = 0;
                                        firstPlayer.life = 0;
                                        bomb.behavior();
                                        require(owner.state().field91c && bomb.phase == endFrame && !firstPlayer.life,
                                            "A blast still collides after growth stops and removes its owner even when player damage is refused.");
                                        firstPlayer.life = 100;
                                        firstPlayer.state.field1338 = 1;
                                        firstPlayer.state.field10C4 = firstPlayer.state.field1150 = firstPlayer.state.field1170 = 0;
                                        owner.state().field91c = false;
                                        object.visibility(255, 255);
                                        require(bomb.collectItem() && owner.state().field91c && firstPlayer.state.field1338 == 2 &&
                                            firstPlayer.life == 100 - object.hitDamage(),
                                            "Direct bomb pickup dispatches the original explosion and player event through finite-item collection.");
                                    }
                                    boundEnemies.deactivate(id);
                                }
                                require(escapeTypes && randomTypes && bombTypes, "All three advanced item factories are exercised through imported selectors.");
                                firstPlayer.state.field10C4 = firstPlayer.state.field1150 = firstPlayer.state.field1170 = 0;
                                firstPlayer.state.field1060 = firstPlayer.state.flags1040[2] = 0;
                                firstPlayer.state.field11A0 = nullptr;
                                firstPlayer.components->actions->field310 = 1;
                                firstPlayer.life = 100;
                                require(playerevents::bombDamage(firstPlayer, 5, 0, health) && firstPlayer.life == 95 &&
                                    firstPlayer.state.field10C4 == 120 && firstPlayer.state.field1338 == 2 && firstPlayer.state.field133C == 2,
                                    "Bomb damage updates health, invulnerability and the original player event state.");
                                require(!playerevents::bombDamage(firstPlayer, 5, 0, health) && firstPlayer.life == 95,
                                    "Bomb damage respects invulnerability without repeated health loss.");
                                firstPlayer.state.field10C4 = 0;
                                firstPlayer.state.field1170 = 1;
                                firstPlayer.state.field1178 = 0.3f;
                                require(playerevents::bombDamage(firstPlayer, 5, 1, health) && firstPlayer.life == 75,
                                    "Shrinking and mansion difficulty each apply their original damage doubling.");
                                {
                                    particles::EmitterPool explosionEmitters(4, 0, 0);
                                    particles::EffectFactory explosionEffects(dol, ownerArchive, explosionEmitters);
                                    explosionEffects.createExplosion(1, spawnPosition);
                                    require(explosionEffects.history[0] == 251, "Explosion history records its original offset type.");
                                    require(!explosionEffects.create(0x53, spawnPosition, {}),
                                        "The four original explosion layers consume all four emitter slots.");
                                }
                                itemEnvironment.life = nullptr;
                                itemEnvironment.mansionMode = nullptr;
                                itemEnvironment.camera = nullptr;
                                itemEnvironment.audioPositions = nullptr;
                                itemEnvironment.trails = nullptr;
                                flags = elementFlags;
                                bindingOwners.itemResources = nullptr;
                                ownerModels.clear();
                            }
                            ownerStrategies.bindItems(nullptr);
                            firstRecord = savedRecord;
                            firstPlayer.state = savedState;
                            firstPlayer.view = savedView;
                            firstPlayer.life = savedLife;
                            *firstPlayer.components->actions = savedActions;
                            firstPlayer.state.field7A0->feedback = savedFeedback;
                            firstPlayer.components->quaternion->quaternion = savedQuaternion;
                            secondPlayer.view.field8C9 = secondCapture;
                        }
                        {
                            const auto savedState = firstPlayer.state;
                            const auto savedQuaternion = firstPlayer.components->quaternion->quaternion;
                            const std::array<float, 3> explicitTarget{13, 14, 15};
                            require(playerevents::resolvePosition(boundRegistry, &explicitTarget, -1) == &explicitTarget &&
                                playerevents::resolvePosition(boundRegistry, &explicitTarget, 1) == &secondPlayer.position,
                                "Player event targets prefer a nonnegative actor index over an explicit position.");
                            firstPlayer.state.field1180 = 0;
                            require(!playerevents::beginAttraction(firstPlayer, boundRegistry, nullptr, 999, 10),
                                "Inactive attraction does not resolve the requested actor.");
                            firstPlayer.state.field1180 = 1;
                            require(!playerevents::beginAttraction(firstPlayer, boundRegistry, nullptr, -1, 10),
                                "Attraction with no target leaves its prior state untouched.");
                            require(playerevents::beginAttraction(firstPlayer, boundRegistry, &explicitTarget, -1, 10) &&
                                firstPlayer.state.field10A4 == explicitTarget && firstPlayer.state.field10A0 == 10 &&
                                firstPlayer.components->quaternion->quaternion == firstPlayer.state.quaternion,
                                "Attraction snapshots its target, duration and current player orientation.");
                            const auto savedLife = firstPlayer.life;
                            const auto savedType = secondPlayer.actorType;
                            const auto savedFlags = secondPlayer.flags;
                            const auto actionKind = firstPlayer.components->actions->field310;
                            const auto actionState = firstPlayer.components->actions->field314;
                            firstPlayer.life = 100;
                            firstPlayer.state.flags1040[2] = 0;
                            firstPlayer.state.field1058 = 0;
                            firstPlayer.state.flags1140[2] = 0;
                            firstPlayer.state.field1170 = firstPlayer.state.field1174 = firstPlayer.state.field1130 = 0;
                            firstPlayer.state.field1338 = 1;
                            firstPlayer.state.flags10E8[1] = 0;
                            firstPlayer.state.field1060 = 0;
                            firstPlayer.state.field11A0 = nullptr;
                            firstPlayer.components->actions->field310 = 0;
                            characteritems::Admission admission(dol, types);
                            playerevents::ItemAward award{99, 7};
                            std::vector<unsigned> sounds;
                            playerevents::ItemEnvironment environment{boundRegistry, boundEnemies, admission, &award,
                                [&](unsigned id, const std::array<float, 3>& position) {
                                    require(position == firstPlayer.position, "Collection audio uses the player position.");
                                    sounds.push_back(id);
                                }};
                            playerevents::ItemRequest request;
                            request.actor = 1;
                            secondPlayer.actorType = 0xc5;
                            const auto count = firstPlayer.state.fields12F4[3];
                            require(playerevents::acceptItem(firstPlayer, request, environment) && firstPlayer.state.field1060 == 1 &&
                                firstPlayer.state.field1064 == &secondPlayer && firstPlayer.state.fields12F4[3] == count + 1 &&
                                award.kind == 3 && award.frames == 100 && sounds == std::vector<unsigned>{0x848},
                                "Item acceptance retains the actor, increments the original counter and notifies audio and award state.");
                            require(!playerevents::acceptItem(firstPlayer, request, environment) && sounds.size() == 1,
                                "Pending animated collection rejects a second item without repeating rewards.");
                            firstPlayer.state.field1060 = 0;
                            firstPlayer.life = 0xffff;
                            require(!playerevents::acceptItem(firstPlayer, request, environment), "Item acceptance uses signed player health.");
                            firstPlayer.life = 100;
                            firstPlayer.components->actions->field310 = 2;
                            for (unsigned action : {0x20u, 0x21u, 0x22u}) {
                                firstPlayer.components->actions->field314 = action;
                                require(!playerevents::acceptItem(firstPlayer, request, environment), "Original blocked action states reject collection.");
                            }
                            firstPlayer.components->actions->field310 = 0;
                            firstPlayer.state.field1170 = std::numeric_limits<float>::quiet_NaN();
                            require(!playerevents::acceptItem(firstPlayer, request, environment), "Unordered player motion blocks animated collection.");
                            request.animated = false;
                            request.actor = -1;
                            require(playerevents::acceptItem(firstPlayer, request, environment) && !firstPlayer.state.field1060,
                                "Nonanimated requests bypass animation-specific gates without creating a pending actor.");
                            firstPlayer.state.field1170 = 0;
                            request.animated = true;
                            request.actor = 1;
                            secondPlayer.actorType = 0xcc;
                            secondPlayer.flags |= 0xc000;
                            require(playerevents::acceptItem(firstPlayer, request, environment) && !(secondPlayer.flags & 0xc000) && award.kind == 7,
                                "Type CC collection restores the original actor visibility flags and award category.");
                            const std::array<unsigned, 11> itemTypes{0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc};
                            const std::array<unsigned, 11> counters{9, 10, 11, 3, 4, 5, 6, 7, 8, 2, 7};
                            const std::array<unsigned, 11> awards{0, 0, 0, 3, 4, 5, 6, 7, 8, 0, 7};
                            for (unsigned i = 0; i < itemTypes.size(); ++i) {
                                firstPlayer.state.field1060 = 0;
                                firstPlayer.state.fields12F4.fill(0xffffffff);
                                secondPlayer.actorType = itemTypes[i];
                                award = {99, 7};
                                sounds.clear();
                                require(playerevents::acceptItem(firstPlayer, request, environment) &&
                                    firstPlayer.state.fields12F4[counters[i]] == 0 && sounds == std::vector<unsigned>{0x848} &&
                                    award.kind == (awards[i] ? awards[i] : 99) && award.frames == (awards[i] ? 100 : 7),
                                    "Every item counter preserves original mapping, unsigned wrap and optional award delivery.");
                            }
                            for (const auto entry : std::array<std::array<unsigned, 2>, 3>{{{0x56, 0x80f}, {0x75, 0x84e}, {0xbc, 0x84d}}}) {
                                firstPlayer.state.field1060 = 0;
                                secondPlayer.actorType = entry[0];
                                sounds.clear();
                                require(playerevents::acceptItem(firstPlayer, request, environment) && sounds == std::vector<unsigned>{entry[1]},
                                    "Keys, element items and redirected items retain their distinct collection sound IDs.");
                            }
                            firstPlayer.state = savedState;
                            firstPlayer.components->quaternion->quaternion = savedQuaternion;
                            firstPlayer.life = savedLife;
                            secondPlayer.actorType = savedType;
                            secondPlayer.flags = savedFlags;
                            firstPlayer.components->actions->field310 = actionKind;
                            firstPlayer.components->actions->field314 = actionState;
                        }
                        {
                            auto& captures = *manager.entries;
                            require(captures.containsTarget(nullptr) && captures.findTarget(nullptr, 5) == captures.entries[5].get() &&
                                !captures.findTarget(nullptr, 6) && !captures.findSource(nullptr, -1),
                                "Inactive vacuum entries retain original null-match and ordinal lookup behavior.");
                            for (unsigned i : {1u, 4u}) {
                                captures.entries[i]->state = 1;
                                captures.entries[i]->source = &firstPlayer;
                                captures.entries[i]->target = &secondPlayer;
                            }
                            require(captures.containsSource(&firstPlayer) && captures.containsTarget(&secondPlayer) &&
                                !captures.containsTarget(&firstPlayer) && captures.findSource(&firstPlayer, 1) == captures.entries[4].get(),
                                "Vacuum lookups preserve source/target distinction and original slot order.");
                            playermodels::VacuumTracking tracking;
                            tracking.field24 = 71;
                            playermodels::VacuumTrackingBlend blend;
                            blend.field4 = 1;
                            require(captures.setTracking(&firstPlayer, tracking) && captures.setBlend(&secondPlayer, blend) &&
                                captures.entries[1]->tracking.field24 == 71 && captures.entries[4]->trackingChanged == 1 &&
                                captures.entries[4]->blend.field4 == 1 && captures.entries[1]->blendChanged == 1,
                                "Tracking and blend delivery updates every matching active entry and its changed flag.");
                            require(captures.setBlend(nullptr, blend) && captures.entries[0]->blend.field4 == 0 &&
                                !captures.entries[0]->blendChanged, "Inactive null matches return success without applying blend data.");
                            captures.fieldA38[2] = captures.fieldA38[5] = &firstPlayer;
                            captures.fieldA50[3] = captures.fieldA50[5] = &secondPlayer;
                            require(captures.sourceIndex(&firstPlayer) == 2 && captures.targetIndex(&secondPlayer) == 3 &&
                                captures.sourceRecord(&firstPlayer) == &captures.sourceRecords[2] &&
                                captures.targetRecord(&secondPlayer) == &captures.targetRecords[3] && !captures.targetRecord(&firstPlayer),
                                "Capture record identity follows the first original owner-table match independently of live entries.");
                            captures.targetRecords[3].fill(0x12345678);
                            captures.reset();
                            require(captures.targetIndex(&secondPlayer) == -1 && captures.targetRecord(nullptr) == &captures.targetRecords[0] &&
                                captures.targetRecords[3][0] == 0x12345678 && captures.entries[1]->state == 1,
                                "Manager reset clears record bindings while retaining records and active entry state.");
                            for (unsigned i : {1u, 4u}) {
                                captures.entries[i]->state = 0;
                                captures.entries[i]->tracking.reset();
                                captures.entries[i]->blend.reset();
                                captures.entries[i]->trackingChanged = captures.entries[i]->blendChanged = 0;
                            }
                        }
                        {
                            const auto firstDoor = firstPlayer.view.flags->data[31];
                            const auto secondDoor = secondPlayer.view.flags->data[31];
                            const auto captured = firstPlayer.view.bits[0]->data[32];
                            firstPlayer.view.flags->data[31] = 0x80;
                            secondPlayer.view.flags->data[31] = 0;
                            firstPlayer.view.bits[0]->data[32] = 0x20;
                            require(bindingRuntime.doorOpened(0, 255) && !bindingRuntime.doorOpened(0, 254) &&
                                !bindingRuntime.doorOpened(1, 255) && bindingRuntime.typeCaptured(0, 261) &&
                                !bindingRuntime.typeCaptured(0, 260),
                                "Concrete admission checks use per-player door and capture buffers with original bit order.");
                            firstPlayer.view.flags->data[31] = firstDoor;
                            secondPlayer.view.flags->data[31] = secondDoor;
                            firstPlayer.view.bits[0]->data[32] = captured;
                            const auto position = firstPlayer.view.position;
                            const auto room = firstPlayer.view.previousRoom;
                            const auto firstFlag = firstPlayer.view.fieldF2C;
                            const auto secondFlag = secondPlayer.view.fieldF2C;
                            const std::array<float, 3> alternatePosition{17, 23, 41};
                            firstPlayer.view.position = &alternatePosition;
                            firstPlayer.view.previousRoom = 0x17000029;
                            firstPlayer.view.fieldF2C = 0;
                            secondPlayer.view.fieldF2C = 0x80;
                            require(boundEnemies.playerPosition(0) == alternatePosition &&
                                boundEnemies.playerRoom(0) == 0x17000029 && boundEnemies.anyPlayerSpecialState(),
                                "EnManager queries use live Player views, including alternate positions and the second player.");
                            secondPlayer.view.fieldF2C = 0;
                            require(!boundEnemies.anyPlayerSpecialState(), "Player special-state queries retain zero results.");
                            firstPlayer.view.position = position;
                            firstPlayer.view.previousRoom = room;
                            firstPlayer.view.fieldF2C = firstFlag;
                            secondPlayer.view.fieldF2C = secondFlag;
                        }
                        require(firstPlayer.state.field794 == &pad && secondPlayer.state.field794 == nullptr &&
                            firstPlayer.state.field7A0 == manager.controllers[0].get() &&
                            secondPlayer.state.field7A0 == manager.controllers[1].get() &&
                            firstPlayer.state.field790 && secondPlayer.state.field790 &&
                            firstPlayer.life == 41 && secondPlayer.life == 73 && audioLife == 73,
                            "Player-manager binding uses original pool creation order, controller ownership and per-player records.");
                        characterownership::Links links(firstPlayer, boundEnemies, sounds);
                        auto* firstChild = firstPlayer.state.field7A4;
                        auto* secondChild = firstPlayer.state.field7A8;
                        auto* thirdChild = firstPlayer.state.field7AC;
                        auto& owner = dynamic_cast<characterowner::Owner&>(*boundEnemies.activeOwner(firstPlayer));
                        require(firstChild, "The bound player has an original equipment actor for dependent ownership.");
                        {
                            gamearchive::Resources mapEffects(ownerMission.archive());
                            j3dmodel::MaterialDefaults defaults;
                            require(defaults.read(dol, error), error);
                            modeleffects::SharedPool projectionPool;
                            projectionPool.load(mapEffects, defaults);
                            modeleffects::Factory projectionFactory;
                            projectionFactory.add(projectionPool);
                            mapcollision::State collision(ownerMission);
                            std::vector<std::uint8_t> collisionBytes;
                            require(roomarchive::read(ownerMap, "col.mp", collisionBytes, error), error);
                            collision.load(collisionBytes);
                            {
                                const auto retained = firstPlayer.state;
                                const auto actions = *firstPlayer.components->actions;
                                const auto life = firstPlayer.life;
                                const auto position = firstPlayer.position;
                                const auto previousPosition = firstPlayer.previousPosition;
                                const auto field5C = firstPlayer.field5C;
                                const auto velocity = firstPlayer.velocity;
                                const auto direction = firstPlayer.direction;
                                const auto speed = firstPlayer.speed;
                                const auto volume = firstPlayer.field788;
                                const auto floor = firstPlayer.fieldB2;
                                const auto support = firstPlayer.fieldA0;
                                const auto room = firstPlayer.fieldB4;
                                unsigned roomSounds = 0, captureSounds = 0;
                                playerenvironment::Frame frame(dol);
                                playerenvironment::Services services{collision, modelCatalog, modelResources, pose,
                                    boundEnemies, *manager.entries,
                                    [](playeractor::Player&) { throw std::runtime_error("Unexpected player type refresh in environment check."); },
                                    [](const playeractor::Player&) -> playeractor::Target* {
                                        throw std::runtime_error("Unexpected capture source in environment check.");
                                    }, [](unsigned) { return true; }, [&](bool) { ++captureSounds; },
                                    [&](unsigned room) { require(room == firstPlayer.fieldB4, "Room audio receives the full original room ID."); ++roomSounds; }};
                                firstPlayer.state.field1324 &= ~0x20;
                                firstPlayer.state.fieldF94 = -1;
                                firstPlayer.state.field11A0 = nullptr;
                                firstPlayer.fieldB4 = 0x02000102;
                                firstPlayer.state.field1170 = 0;
                                firstPlayer.fieldA0 = firstPlayer.fieldB2 = -1;
                                firstPlayer.state.field10E0 = -1;
                                firstPlayer.life = 31;
                                firstPlayer.state.maximumLife = 100;
                                frame.run(firstPlayer, services);
                                require(firstPlayer.state.fieldF68 == 1 && firstPlayer.state.fieldF6C == 1,
                                    "Environment stage sets the original playback rates.");
                                require(!firstPlayer.state.flags1040[0] && firstPlayer.state.field119D == 0,
                                    "Environment stage preserves the original low-life and room-lighting tests.");
                                require(roomSounds == 1 && captureSounds == 0,
                                    "Environment stage sends room audio only at the original transition.");
                                require(firstPlayer.state.field7B4 == boundEnemies.activeOwner(firstPlayer),
                                    "Environment stage retains the real actor owner.");
                                firstPlayer.life = 29;
                                firstPlayer.state.field1170 = 1;
                                frame.run(firstPlayer, services);
                                require(firstPlayer.state.flags1040[0] && firstPlayer.state.fieldF68 == 2 && roomSounds == 1,
                                    "Low life uses a strict signed comparison and unchanged rooms do not repeat the audio transition.");
                                firstPlayer.state.field1324 |= 0x20;
                                firstPlayer.position[1] = 1051;
                                firstPlayer.setVelocity({1, 2, 3});
                                frame.run(firstPlayer, services);
                                require(firstPlayer.state.flags1330[1] && firstPlayer.field788 && firstPlayer.position[1] == 1040 &&
                                    firstPlayer.velocity == std::array<float, 3>{} && firstPlayer.components->actions->field314 == 30,
                                    "Inversion publishes its original volume, action request, height ceiling and velocity reset.");
                                firstPlayer.components->actions->field310 = 2;
                                firstPlayer.components->actions->field314 = 32;
                                frame.run(firstPlayer, services);
                                require(!firstPlayer.state.flags1330[1] && !firstPlayer.field788 && firstPlayer.components->actions->field314 == 32,
                                    "Protected interaction suppresses inversion and rejects its action request.");
                                firstPlayer.state = retained;
                                *firstPlayer.components->actions = actions;
                                firstPlayer.life = life;
                                firstPlayer.position = position;
                                firstPlayer.previousPosition = previousPosition;
                                firstPlayer.field5C = field5C;
                                firstPlayer.velocity = velocity;
                                firstPlayer.direction = direction;
                                firstPlayer.speed = speed;
                                firstPlayer.field788 = volume;
                                firstPlayer.fieldB2 = floor;
                                firstPlayer.fieldA0 = support;
                                firstPlayer.fieldB4 = room;
                            }
                            const auto actorType = firstPlayer.actorType;
                            {
                                const auto retained = firstPlayer.state;
                                const auto retainedActions = *firstPlayer.components->actions;
                                const auto retainedFlags = firstPlayer.flags;
                                const auto retainedBlend = firstPlayer.blendFramesRemaining;
                                auto& actions = *firstPlayer.components->actions;
                                actions.field310 = 1;
                                actions.fields31C[4] = 6;
                                actions.fields304 = {1, 0, 1};
                                actions.field300 = 1;
                                firstPlayer.state.fieldF68 = 2;
                                firstPlayer.state.flags134C[1] = 1;
                                firstPlayer.state.fieldF98 = 1;
                                playercontrols::Motion moving{0.5f, {}, {1, 0, 0}};
                                firstPlayer.state.fieldFB8 = &moving;
                                unsigned requests = 0;
                                gameflags::State actionFlags;
                                playeractions::Frame frame(dol, pose);
                                playeractions::FrameServices services{actionFlags, modelCatalog, modelResources, {},
                                    [&](std::uint32_t id, const std::array<float, 3>& position) {
                                        require(id == 0x281c && position == firstPlayer.position,
                                            "Action mode six emits its original positional request at the player.");
                                        ++requests;
                                    }};
                                require(frame.fallbackDuration(0, modelResources) > 0,
                                    "Action fallback duration resolves the original Luigi key through its action table.");
                                const auto animation = firstPlayer.animationState;
                                frame.apply(firstPlayer, animation ^ 1, 0, services);
                                require(requests == 1 && firstPlayer.animationState == animation && firstPlayer.blendFramesRemaining == 1 &&
                                    (firstPlayer.flags & 0x1000) && std::abs(firstPlayer.state.fieldF68 - 0.15f) < 0.000001f,
                                    "Action output preserves the animation lock, blends first movement and applies material/stop speed factors in order.");
                                frame.apply(firstPlayer, animation, 6, services);
                                require(requests == 1, "Unchanged action mode does not repeat its transition sound.");
                                firstPlayer.state = retained;
                                *firstPlayer.components->actions = retainedActions;
                                firstPlayer.flags = retainedFlags;
                                firstPlayer.blendFramesRemaining = retainedBlend;
                            }
                            {
                                actors::MoveActor attachedActor(construction, 126, 0, 130);
                                playercomponents::AttachedState attached(dol, 0, *attachedActor.model, &attachedActor);
                                playercomponents::AttachedInput input;
                                input.enabled = true;
                                input.mode = 1;
                                input.frame = 7;
                                auto output = attached.update(input, modelCatalog, modelResources, pose);
                                const auto& archive = *modelResources.entry(modelCatalog.type(0).model).archive;
                                require(attached.effects[0].field4 == archive.keys.at(attached.parameters[5] >> 16) &&
                                    attached.effects[1].field4 == archive.keys.at(attachedActor.keyIndex) &&
                                    attached.effects[1].field8 == 7 && attached.fields2B8[0] && attached.fields2B8[1],
                                    "Attached animation binds its own original upper key and the actor's retained lower key.");
                                attached.effects[0].field74 = 2;
                                attached.update(input, modelCatalog, modelResources, pose);
                                require(attached.effects[0].field74 == 2, "Repeated attached keys preserve their active blend countdown.");
                                input.selection = 6;
                                attached.update(input, modelCatalog, modelResources, pose);
                                require(attached.effects[0].field4 == attached.effects[1].field4 &&
                                    attached.effects[0].field8 == attached.effects[1].field8,
                                    "Scanner selections share the original attached key and sampled frame across both branches.");
                                input.enabled = false;
                                input.aim = false;
                                attached.update(input, modelCatalog, modelResources, pose);
                                require(!attached.effects[0].field4 && !attached.effects[1].field4 &&
                                    !(attached.effects[0].flags & 1) && attached.effects[0].field74 == 6 && attached.effects[1].field74 == 6,
                                    "Leaving attached animation releases its overrides with the original six-frame blend.");
                                input.enabled = input.aim = true;
                                input.selection = 0;
                                input.mode = 2;
                                input.horizontal = input.vertical = 1000;
                                output = attached.update(input, modelCatalog, modelResources, pose);
                                require(output.limits[0] == 1 && output.limits[1] == 1 && attached.field240 == 1 && attached.field244 == 1 &&
                                    (attached.effects[2].flags & 4), "Attached analog limits clamp pose channels and retain overflow for player turning.");
                                for (unsigned mode = 1; mode <= 11; ++mode) {
                                    input.mode = mode;
                                    input.horizontal = -0.25f;
                                    input.vertical = 0.5f;
                                    input.sourceDirection = {0, 0, 1};
                                    input.targetDirection = {1, 0, 0};
                                    attached.fields2B8[4] = 0;
                                    output = attached.update(input, modelCatalog, modelResources, pose);
                                    require(std::isfinite(attached.field240) && std::isfinite(attached.field244) &&
                                        attached.field240 >= 0 && attached.field240 <= 1 && attached.field244 >= 0 && attached.field244 <= 1,
                                        "All original attached-control modes retain bounded animation channels.");
                                    actormodel::samplePose(*attachedActor.model, pose);
                                    actormodel::blendPose(*attachedActor.model);
                                    actormodel::updateEnvelopes(*attachedActor.model);
                                    const auto& modelBytes = attachedActor.model->model->bytes;
                                    const unsigned matrices = ((unsigned(modelBytes[8]) << 8) | modelBytes[9]) +
                                        ((unsigned(modelBytes[12]) << 8) | modelBytes[13]);
                                    for (unsigned joint = 0; joint < matrices; ++joint)
                                        for (const auto component : attachedActor.model->matrix(0, joint)) require(std::isfinite(component),
                                            "Original attached keys, aiming, blend clocks and skin envelopes produce finite Luigi poses.");
                                }
                                input.mode = 8;
                                attached.fields2B8[4] = 0;
                                attached.update(input, modelCatalog, modelResources, pose);
                                require(attached.fields2B8[4], "Return-to-neutral begins on the original mode edge.");
                                for (unsigned i = 0; i < 300 && attached.fields2B8[4]; ++i) attached.update(input, modelCatalog, modelResources, pose);
                                require(!attached.fields2B8[4], "Return-to-neutral completes only inside both original channel tolerances.");
                                input.enabled = input.aim = false;
                                attached.update(input, modelCatalog, modelResources, pose);
                                unsigned transitionSounds = 0;
                                const auto emit = [&](unsigned id, const std::array<float, 3>& position) {
                                    require(id == 0x805 && position == attachedActor.position,
                                        "Attached transition timing emits its original head sound at its actor.");
                                    ++transitionSounds;
                                };
                                unsigned animationRequests = 0;
                                actoranimation::SoundServices unavailableAudio{
                                    [&](unsigned, actoranimation::SoundHandle*& handle) { ++animationRequests; handle = nullptr; },
                                    [](actoranimation::SoundHandle&, float) { throw std::runtime_error("Unexpected unowned pitch handle."); },
                                    [](actoranimation::SoundHandle&, float) { throw std::runtime_error("Unexpected unowned volume handle."); },
                                    [](actoranimation::SoundHandle&, float) { throw std::runtime_error("Unexpected unowned auxiliary handle."); }
                                };
                                for (unsigned transition = 1; transition < attached.fields24C.size(); ++transition) {
                                    const auto keyIndex = attached.fields24C[transition];
                                    const auto& key = archive.keys.at(keyIndex);
                                    const float duration = float((unsigned(key->bytes.at(4)) << 8) | key->bytes.at(5));
                                    attached.startFirstTransition(transition, 1);
                                    attached.startSecondTransition(transition, 2);
                                    attached.finish(0.5, modelCatalog, modelResources, emit, unavailableAudio);
                                    require(!attached.field36C && (!attached.sound.resource || attached.sound.frame == attached.field2C4),
                                        "The enclosing attached pass clears its update latch before publishing the original BAS clock.");
                                    require(attached.field2C4 == 0.5f && attached.field2CC == 0.5f && attached.field2D4 == 1 &&
                                        attached.effects[0].field4 == key && attached.effects[1].field4 == key &&
                                        attached.effects[0].field8 == 0.5f && attached.effects[1].field8 == 1,
                                        "Late attached update advances independent branch clocks before publishing their keys and frames.");
                                    const auto soundIndex = modelCatalog.model(modelCatalog.type(0).model).keyMetadata.at(keyIndex)[5];
                                    require(attached.field2DC == static_cast<std::int32_t>(keyIndex) &&
                                        attached.sound.resource == (soundIndex < archive.auxiliary[4].size() ? archive.auxiliary[4][soundIndex] : nullptr),
                                        "Attached transition binds the original key's animation sound resource.");
                                    actoranimation::SoundState importedSound;
                                    importedSound.bind(attached.sound.resource, 1, 0);
                                    for (unsigned frame = 0; frame <= unsigned(duration); ++frame)
                                        importedSound.update(frame, 1, unavailableAudio);
                                    attached.effects[0].field74 = 2;
                                    attached.advance(0, modelCatalog, modelResources, emit);
                                    require(attached.effects[0].field74 == 2 && attached.field2CC == 0,
                                        "Unchanged attached keys retain their blend countdown at zero clock rate.");
                                    attached.field2C4 = duration - 1;
                                    attached.field2D4 = duration - 2;
                                    attached.advance(1, modelCatalog, modelResources, emit);
                                    require(!attached.field2C0 && !attached.field2D0 && !attached.effects[0].field4 &&
                                        !attached.effects[1].field4 && attached.effects[0].field74 == 6 &&
                                        attached.effects[1].field74 == 6 && attached.field2DC == -1 && !attached.sound.resource,
                                        "Exact duration equality retires both attached transitions and releases overrides with six-frame blends.");
                                }
                                attached.advance(1, modelCatalog, modelResources, emit);
                                require(attached.field2CC == 0, "An inactive upper transition clears its retained frame delta.");
                                {
                                    actors::MoveActor child(construction, 125, 0, 130);
                                    child.position = {100, 200, 300};
                                    child.previousPosition = {4, 5, 6};
                                    child.fieldB4 = 0x12345678;
                                    attachedActor.fieldB4 = 0x02000102;
                                    require(attachedActor.attachChild(child, 0, nullptr, 1), "Pose child attaches through its original owner.");
                                    const auto transform = attachedActor.childMatrix(0);
                                    const std::array<float, 3> expected{transform[3], transform[7], transform[11]};
                                    attachedActor.flags |= 0x40;
                                    attachedActor.updateChildren();
                                    require((child.flags & 0x60) == (attachedActor.flags & 0x60) && child.position == expected &&
                                        child.field5C == expected && child.previousPosition == std::array<float, 3>{4, 5, 6} &&
                                        child.velocity == std::array<float, 3>{expected[0] - 100, expected[1] - 200, expected[2] - 300} &&
                                        child.fieldB4 == 0x12345678,
                                        "Inactive attached children still inherit flags, position and velocity without publishing a new room or previous position.");
                                    attachedActor.flags &= ~0x60u;
                                    attachedActor.updateChildren();
                                    require(!(child.flags & 0x60) && child.fieldB4 == attachedActor.fieldB4 && child.speed == 0 &&
                                        child.rotation == std::array<std::int16_t, 3>{} && child.rotation == child.previousRotation,
                                        "Active child publication updates its pose and room after clearing local rotation.");
                                    for (float value : child.model->matrix(0, 0)) require(std::isfinite(value),
                                        "Attached child pose remains finite through original hierarchy and envelope evaluation.");
                                    require(attachedActor.detachChild(child), "Pose child releases through its original owner.");
                                }
                                const auto retained = firstPlayer.state;
                                const auto actions = *firstPlayer.components->actions;
                                const auto settings = firstPlayer.components->controls->settings;
                                playercomponents::AttachedFrame frame(dol);
                                firstPlayer.state.field10A0 = 1;
                                firstPlayer.state.field1180 = 5;
                                firstPlayer.state.field10A4 = {3, 4, 5};
                                firstPlayer.components->controls->settings.flags |= 0x40;
                                frame.build(firstPlayer);
                                require(firstPlayer.state.field10A0 == 0 && firstPlayer.state.field1070 == 5 &&
                                    firstPlayer.state.field1074 == std::array<float, 3>{3, firstPlayer.position[1], 5} && firstPlayer.state.field109C == 0.5f,
                                    "Expired attached aim restores the original horizontal look-at orientation target.");
                                firstPlayer.state = retained;
                                *firstPlayer.components->actions = actions;
                                firstPlayer.components->controls->settings = settings;
                            }
                            {
                                actors::MoveActor contactActor(construction, 127, 0, 130);
                                {
                                    actors::MoveActor supportActor(construction, 125, 0, 130);
                                    actorcollision::Manager pairCollision(collisionCatalog, modelCatalog);
                                    actorcollision::Volume volume;
                                    volume.field8 = 1;
                                    actorcollision::Record cylinder{};
                                    const float radius = 10, height = 10;
                                    std::memcpy(&cylinder[0], &radius, 4);
                                    std::memcpy(&cylinder[1], &height, 4);
                                    volume.secondary = cylinder;
                                    supportActor.field788 = contactActor.field788 = &volume;
                                    contactActor.position = {0, 9, 0};
                                    supportActor.position = {};
                                    contactActor.velocity = {0, -1, 0};
                                    const auto before = contactActor.field5C;
                                    require(contactActor.resolveActorContact(supportActor, pairCollision) && contactActor.position[1] == 10 &&
                                        contactActor.field9C == 125 && supportActor.field9C == 127 && contactActor.fieldA0 == 125 &&
                                        contactActor.field5C == before,
                                        "Actor support publishes both contact identities while preserving the original ordinary correction bookkeeping.");
                                    contactActor.position = {0, 9, 0};
                                    contactActor.velocity[1] = 0;
                                    contactActor.fieldA0 = -1;
                                    require(contactActor.resolveActorContact(supportActor, pairCollision) && contactActor.fieldA0 == -1,
                                        "A zero vertical velocity does not acquire support through the original falling-only actor contact branch.");
                                    const auto& animations = modelCatalog.type(0).animations;
                                    unsigned falling = 0;
                                    while (falling < animations.size() && !(animations[falling][5] & 0x20)) ++falling;
                                    require(falling < animations.size(), "Original actor table supplies a falling contact transition.");
                                    contactActor.selectAnimation(falling);
                                    contactActor.position = {0, 9, 0};
                                    contactActor.velocity[1] = -1;
                                    require(contactActor.resolveActorContact(supportActor, pairCollision) &&
                                        contactActor.animationState == animations[falling][4] && contactActor.velocity[1] == 0,
                                        "Landing on an actor selects the original table successor and clears vertical velocity.");
                                    contactActor.field788 = nullptr;
                                    contactActor.selectAnimation(0);
                                }
                                const auto* characters = ownerMission.getJmpResource("CharacterInfo");
                                const int row = characters->findEntryByValue(characters->searchItemInfo("name"), "luige");
                                std::array<float, 3> position{};
                                require(row >= 0 && characters->getValue(row, "pos_x", position[0]) &&
                                    characters->getValue(row, "pos_y", position[1]) && characters->getValue(row, "pos_z", position[2]),
                                    "Original character placement supplies the contact check position.");
                                position[1] += 100;
                                require(collision.projectFloor(position), "Luigi's original placement has a collision floor.");
                                position[1] -= 1;
                                contactActor.setPosition(position);
                                for (unsigned i = 0; i < 3; ++i) contactActor.matrix[i * 4 + 3] = position[i];
                                contactActor.fieldsB8.fill(0xa5);
                                std::int16_t wall = 17;
                                require(contactActor.resolveMapContact(collision, collisionCatalog, wall) && contactActor.fieldB2 >= 0 &&
                                    contactActor.field78C > 0.7f && contactActor.position[1] >= position[1],
                                    "Original Luigi collision volumes resolve the placed actor's actual map floor.");
                                require(contactActor.field5C == contactActor.position && contactActor.previousPosition == position &&
                                    contactActor.fieldsB8[8] == 255 && contactActor.fieldsB8[9] == 255 &&
                                    contactActor.fieldsB8[10] == 0xa5 && contactActor.fieldsB8[11] == 0xa5,
                                    "Contact resolution moves both current positions, retains the prior frame and preserves the probe's untouched bytes.");
                                contactActor.setPosition(position);
                                contactActor.updateMapContact(collision, collisionCatalog);
                                require((contactActor.flags & 0x10000) && contactActor.fieldB2 >= 0,
                                    "Actor map-contact stage publishes the original grounded flag and retained floor polygon.");
                            }
                            const auto alpha = firstPlayer.fieldE0;
                            unsigned projectionType = 0;
                            while (projectionType < 381 && !(construction.parameters.parameters(projectionType).entries()[20].bits & 1)) ++projectionType;
                            require(projectionType < 381, "Original parameters supply projected character effects.");
                            firstPlayer.actorType = projectionType;
                            firstPlayer.fieldE0 = 129;
                            owner.updateProjection(projectionFactory, construction.parameters, collision);
                            auto* projected = dynamic_cast<modeleffects::SharedOwner*>(owner.ownership().sharedResource);
                            require(projected && owner.ownership().sharedUsers[2] == projected && owner.ownership().sharedCount == 1 &&
                                (projected->flags & 5) == 5, "Visible owner projection acquires the original shared model pool.");
                            owner.ownership().sharedUsers[0] = owner.acquireShared(projectionFactory, construction.parameters, collision);
                            require(owner.ownership().sharedCount == 2 && owner.ownership().sharedUsers[0] == projected,
                                "A second owner state retains the existing projected resource.");
                            firstPlayer.fieldE0 = 128;
                            owner.updateProjection(projectionFactory, construction.parameters, collision);
                            require(!owner.ownership().sharedUsers[2] && owner.ownership().sharedCount == 1 &&
                                owner.ownership().sharedResource == projected && !(projected->flags & 4),
                                "The original alpha threshold drops one projection reference and hides the retained effect.");
                            owner.ownership().releaseShared(owner.ownership().sharedUsers[0]);
                            require(!owner.ownership().sharedResource && !owner.ownership().sharedCount && !(projected->flags & 1),
                                "Final shared reference returns the effect to its original pool.");
                            firstPlayer.actorType = actorType;
                            firstPlayer.fieldE0 = alpha;
                        }
                        const auto parentFlags = firstPlayer.flags;
                        const auto playerFlag = firstPlayer.view.field895;
                        firstPlayer.flags |= 0xc000;
                        firstPlayer.view.field895 = 1;
                        owner.state().field890 = static_cast<std::int32_t>(firstPlayer.animationState ^ 1);
                        owner.state().actorUsers[1] = firstChild;
                        owner.state().field928 = firstChild;
                        owner.state().actorReferences = 2;
                        owner.ownership().dependentActor = firstChild;
                        owner.updateDependentActors(boundRegistry);
                        require(!owner.state().actorUsers[1] && owner.state().field890 == -1 &&
                            owner.state().actorReferences == 1 && owner.state().field928 == firstChild &&
                            (firstChild->flags & 0xc000) == 0xc000 && firstChild->parent == &firstPlayer,
                            "Animation change drops one reference while a shared child retains ownership and copies inactive flags.");
                        firstPlayer.flags &= 0xffff3fff;
                        firstPlayer.view.field895 = 0;
                        owner.updateDependentActors(boundRegistry);
                        require(!owner.state().field928 && owner.state().actorReferences == 0 &&
                            !owner.ownership().dependentActor && !firstChild->parent &&
                            boundEnemies.slots()[firstChild->actorId].state == 1,
                            "The final player-state reference detaches and returns the shared child to EnManager before clearing its pointer.");
                        firstPlayer.flags = parentFlags;
                        firstPlayer.view.field895 = playerFlag;
                        owner.ownership().deactivate(links);
                        require((firstPlayer.flags & 0xc420) == 0xc420 &&
                            (!firstChild || (!firstChild->parent && boundEnemies.slots()[firstChild->actorId].state == 1)) &&
                            (!secondChild || (!secondChild->parent && boundEnemies.slots()[secondChild->actorId].state == 1)) &&
                            (!thirdChild || (!thirdChild->parent && boundEnemies.slots()[thirdChild->actorId].state == 1)),
                            "Concrete owner links detach and return original equipment actors through EnManager.");
                        firstPlayer.state.record = secondPlayer.state.record = nullptr;
                        manager.controllers[0]->reset(nullptr);
                    }
                }
                require(actor.actorId == 0 && actor.actorType == 0 && actor.keyIndex == 130 &&
                    actor.model->key->name == "wait0.key" && actor.blendFramesRemaining == 0 && actor.blendFramesTotal == 0,
                    "Concrete MoveActor constructs the original initial animation before Player components.");
                require(actor.fieldA4 == 40 && actor.fieldA8 == 200 && actor.field160 == 1 && actor.field78C == 1,
                    "MoveActor retains the original shared constructor settings.");
                const auto key = actor.model->key;
                const auto initialName = modelCatalog.type(actor.actorType).animationNames.at(actor.animationState);
                require(actor.selectAnimation(initialName) && !actor.selectAnimation("POLTERPORT_MISSING_ANIMATION") &&
                    actor.animationState == 0, "Named actor selection preserves the current animation on lookup failure.");
                actor.model->attachment(0).field0 = 7;
                actor.selectAnimation(0);
                require(actor.model->key == key && actor.model->attachment(0).field0 == 7,
                    "Selecting the same actor animation preserves its existing cursor state.");
                unsigned nextId = 1;
                {
                    actors::MoveActor animated(construction, 127, 0, 130);
                    animated.blendFramesRemaining = 2;
                    animated.advanceAnimation();
                    require(animated.blendFramesRemaining == 1 && animated.animationFrameStep > 0 && animated.animationFrame == animated.animationFrameStep,
                        "Original actor clock advances the selected key and decrements blend timing.");
                    auto temporary = std::make_shared<actormodel::Resource>();
                    temporary->bytes = {0, 2};
                    animated.fieldF4 = temporary;
                    animated.fieldF0 = 0;
                    animated.advanceAnimation();
                    require(animated.fieldF4 && animated.fieldF0 == 1, "Retained animation expires at its own duration.");
                    animated.advanceAnimation();
                    require(!animated.fieldF4 && animated.fieldF0 == 2, "Retained animation releases on duration equality.");
                    animated.position = {1, 2, 3};
                    animated.fieldE0 = 0x10203040;
                    animated.fieldE4 = 0x30103140;
                    animated.flags |= 0x40;
                    const auto frame = animated.animationFrame;
                    animated.finishFrame();
                    require(animated.animationFrame == frame && animated.fieldE0 == 0x10203040 && animated.previousPosition != animated.position,
                        "Inactive actors preserve their frame, color and previous-position state.");
                    animated.flags &= ~0x60u;
                    animated.finishFrame();
                    require(animated.fieldE0 == 0x141c3140 && animated.model->color == animated.fieldE0 &&
                        animated.previousPosition == animated.position, "Actor frame completion approaches each color byte and retains the current position.");
                    const auto& animations = modelCatalog.type(0).animations;
                    unsigned looping = 0, ending = 0, reversed = 0, movement = 0;
                    for (unsigned state = 0; state < animations.size(); ++state) {
                        const auto resource = modelResources.key(0, state);
                        if (!resource || resource->bytes.size() < 12) continue;
                        animated.selectAnimation(state);
                        const float duration = float((unsigned(resource->bytes[4]) << 8) | resource->bytes[5]);
                        animated.animationFrame = duration * 0.5f;
                        animated.samplePose();
                        require(animated.model->key == resource && animated.model->frame == animated.animationFrame,
                            "The actor pose stage publishes the retained original key index and animation frame.");
                        actormodel::updateEnvelopes(*animated.model);
                        actormodel::Instance reference(animated.model->model, 0);
                        reference.bindKey(resource);
                        reference.frame = animated.animationFrame;
                        pose.initialize(reference);
                        const auto& modelBytes = reference.model->bytes;
                        const unsigned matrices = ((unsigned(modelBytes[8]) << 8) | modelBytes[9]) +
                            ((unsigned(modelBytes[12]) << 8) | modelBytes[13]);
                        for (unsigned joint = 0; joint < matrices; ++joint) require(reference.matrix(0, joint) == animated.model->matrix(0, joint),
                            "Unmodified live pose sampling agrees with established key sampling and skin envelopes across Luigi's animation table.");
                        animated.animationClock = duration;
                        animated.animationSpeed = 0;
                        animated.flags &= ~0x61000u;
                        animated.speed = 0;
                        animated.advanceAnimation();
                        if (animations[state][5] & 1) {
                            ++movement;
                            require(animated.animationFrameStep == 2 && (animated.flags & 0x20000),
                                "Movement-scaled animation retains the original two-frame minimum even at zero speed.");
                        } else if (resource->bytes[11] & 1) {
                            ++ending;
                            require(animated.animationClock == duration && (animated.flags & 0x40000) && animated.animationState == state,
                                "One-shot completion is latched before selecting the successor on the following tick.");
                            animated.advanceAnimation();
                            require(animated.animationState == animations[state][2], "One-shot successor comes from the original action table.");
                        } else {
                            ++looping;
                            require(animated.animationClock == 0 && (animated.flags & 0x20000), "Loop boundary subtracts the original key duration.");
                        }
                        if ((animations[state][5] & 2) && animated.animationState == state) {
                            ++reversed;
                            require(animated.animationFrame == duration - 1 - animated.animationClock,
                                "Reverse sampling uses the retained key length and the forward clock.");
                        }
                    }
                    require(looping && ending && reversed && movement, "Imported Luigi animations exercise every clock mode.");
                    animated.rotation = {0, 0x7fff, 0};
                    animated.previousRotation = {0x801, static_cast<std::int16_t>(0x8001), static_cast<std::int16_t>(0xf800)};
                    animated.approachRotation();
                    require(animated.rotation == std::array<std::int16_t, 3>{0x800, static_cast<std::int16_t>(0x8001),
                        static_cast<std::int16_t>(0xf800)}, "Actor turning preserves short-angle wrap and the exact negative boundary.");
                    animated.rotation = {0, 0, 0};
                    animated.previousRotation = {0x7ff, static_cast<std::int16_t>(0xf7ff), static_cast<std::int16_t>(0x8000)};
                    animated.approachRotation();
                    require(animated.rotation == std::array<std::int16_t, 3>{0x7ff, -0x800, -0x800},
                        "Actor rotation clamps both directions and retains the original half-turn choice.");
                    actors::MoveActor linked(construction, 126, 0, 130);
                    animated.poseLinkedActor = &linked;
                    animated.matrix[3] = 12;
                    animated.copyLinkedPose();
                    require(linked.matrix == animated.matrix && linked.model->matrix(0, 7) == animated.model->matrix(0, 7),
                        "Linked actor publication copies the world transform and sampled pose.");
                    animated.poseLinkedActor = nullptr;
                    const auto& archive = *modelResources.entry(0).archive;
                    for (const auto& path : archive.auxiliary[1]) {
                        if (!path) continue;
                        animated.fieldF4 = path;
                        animated.field5C = {12, 34, 56};
                        animated.fieldF0 = 0;
                        animated.followPath();
                        require(animated.position == animated.field5C && animated.previousRotation == animated.rotation,
                            "Original path motion is relative to its frame-zero position and publishes both angle fields.");
                        animated.fieldF0 = 0.5f;
                        animated.followPath();
                        std::array<float, 3> origin, current;
                        std::array<std::int16_t, 3> angles;
                        actoranimation::samplePath(*path, 0, origin);
                        actoranimation::samplePath(*path, 0.5f, current, &angles);
                        for (unsigned axis = 0; axis < 3; ++axis)
                            require(animated.position[axis] == animated.field5C[axis] + (current[axis] - origin[axis]),
                                "Retained trajectory sampling uses the original base position and curve displacement.");
                        require(animated.rotation == angles, "Trajectory motion publishes the sampled original angles.");
                    }
                    animated.fieldF4.reset();
                    unsigned morphs = 0, materialAnimations = 0;
                    for (unsigned index = 0; index < archive.morphCount; ++index) {
                        if (!archive.morphs[index][0] || !archive.morphs[index][1]) continue;
                        animated.fieldF8 = static_cast<std::int16_t>(index);
                        animated.updateMorph();
                        const auto& effect = animated.model->effects[0];
                        const auto& keys = archive.morphs[index][1]->bytes;
                        const float duration = float((unsigned(keys.at(4)) << 8) | keys.at(5));
                        animated.model->effects[0].value = duration - 1;
                        animated.updateMorph();
                        require(effect.value == (((unsigned(keys.at(6)) << 8) | keys.at(7)) ? duration : 0),
                            "Morph clock retains the original loop or inclusive terminal frame.");
                        for (const auto& position : animated.surface().positions) for (float value : position)
                            require(std::isfinite(value), "Imported morph updates keep all model vertices finite.");
                        ++morphs;
                    }
                    for (unsigned state = 0; state < animations.size(); ++state) {
                        animated.model->blend = 0.25f;
                        animated.animationState = 0xffffffff;
                        animated.selectAnimation(state);
                        require(animated.model->materialFrame == 0 && animated.model->blend == 0.25f,
                            "Animation selection resets the material clock, not the pose blend.");
                        if (!animated.model->field110) continue;
                        animated.updateMaterials();
                        require(animated.model->materialFrame == 1, "Material sampling precedes the original one-tick advance.");
                        ++materialAnimations;
                    }
                    require(morphs > 0, "Imported Luigi morph resources execute through the actor owner.");
                    std::cout << "Actor model updates: " << morphs << " morph resources, " << materialAnimations << " material animations.\n";
                    require(&animated.surface() == &linked.surface(), "Actors retain original shared model-surface ownership.");
                }
                for (const auto& pool : creation.pools()) {
                    for (unsigned i = 0; i < pool.count; ++i) {
                        actors::MoveActor pooled(construction, nextId++, pool.type, pool.capacity);
                        require(pooled.model->capacity() == pool.capacity && pooled.actorType == pool.type,
                            "Each original actor pool constructs its base with the declared workspace capacity.");
                    }
                }
                require(nextId == 104, "Original single-player construction includes all 103 pooled base actors.");
            }
            require(modelResources.entry(0).state == 3 && modelResources.key(0, 0)->name == "wait0.key" &&
                modelResources.key(0, 0) == modelResources.entry(0).archive->keys.at(130),
                "Original common archive initialization owns stable actor model resources.");
            const auto luigiIndex = common.findPathResource("/model/luige.arc");
            require(luigiIndex >= 0, "Luigi constructor uses the embedded common archive.");
            luigiBytes = common.resource(static_cast<unsigned>(luigiIndex));
            require(dolresources::read(dol, glme01::actorModelTable, 52, modelRecord, error) &&
                dolresources::read(dol, glme01::actorAuxiliaryTable, 64, auxiliaryRecord, error), error);
            const auto readWord = [](const std::vector<std::uint8_t>& bytes, unsigned offset) {
                return (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
                    (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
            };
            actormodel::ArchiveLayout layout;
            layout.keys = readWord(modelRecord, 12);
            const std::array<unsigned, 5> countOffsets{32, 16, 40, 48, 56};
            for (unsigned i = 0; i < countOffsets.size(); ++i) {
                layout.auxiliary[i] = readWord(auxiliaryRecord, countOffsets[i]);
            }
            actormodel::Archive luigi;
            require(luigi.read(luigiBytes, layout, error), error);
            require(luigi.keys.size() == 141 && luigi.keys.at(130)->name == "wait0.key" && luigi.model,
                "Original embedded archive resolves constructor animation 130 without substitution.");
            actormodel::Instance playerModel(luigi.model, creation.capacity(0));
            {
                actormodel::Instance initialModel(luigi.model, creation.capacity(0));
                initialModel.bindKey(luigi.keys.at(130));
                actormodel::InitialPose initialPose(dol);
                initialPose.initialize(initialModel);
                bool translated = false;
                const unsigned joints = (unsigned(luigi.model->bytes[8]) << 8) | luigi.model->bytes[9];
                for (unsigned joint = 0; joint < joints; ++joint) {
                    const auto& matrix = initialModel.matrix(0, joint);
                    require(matrix == initialModel.matrix(3, joint), "Actor initial pose seeds its previous-pose bank after sampling.");
                    for (float value : matrix) require(std::isfinite(value), "Original initial actor pose remains finite.");
                    translated |= matrix[3] != 0 || matrix[7] != 0 || matrix[11] != 0;
                }
                require(translated, "Luigi's original initial key produces a posed skeleton rather than identity matrices.");
            }
            for (const int actorType : {0, 1, 0x27, 0xb3, -1}) {
                playeractions::Actions actions(dol, actorType);
                playeractions::Reactions reactions(dol, actorType);
                require(actions.field10 == 1 && actions.field310 == 1 && reactions.state == 1,
                    "Original player action owners initialize each descriptor family.");
                for (const auto* entry : reactions.entries) {
                    if (entry) require(entry >= reactions.records.data() && entry < reactions.records.data() + reactions.records.size(),
                        "Reaction lookup retains its owned record identity.");
                }
                std::array<unsigned, 3> attachments;
                unsigned singleAttachment;
                {
                    playercomponents::AttachedState attached(dol, actorType, playerModel);
                    playercomponents::AttachedSingle single(dol, actorType, playerModel);
                    std::copy_n(attached.parameters.begin(), 3, attachments.begin());
                    singleAttachment = single.parameters[0];
                    require(playerModel.attachment(singleAttachment).object == &single.effect,
                        "Player component connects its effect to the actor-owned model.");
                    require(attached.setBlend(-0.5, 1.5) == std::array<std::int32_t, 2>{-1, 1} &&
                        attached.field240 == 0 && attached.field244 == 1, "Original attached-state blend reports and clamps both boundaries.");
                    attached.startFirstTransition(2, 0.5);
                    attached.startSecondTransition(2, 0.25);
                    attached.field2C4 = attached.field2CC = attached.field2D4 = 17;
                    attached.startFirstTransition(2, 3);
                    attached.startSecondTransition(2, 4);
                    require(attached.field2C4 == 17 && attached.field2CC == 17 && attached.field2D4 == 17 &&
                        attached.field2C8 == 0.5f && attached.field2D8 == 0.25f,
                        "Repeated attachment transitions retain elapsed time and their original speed.");
                    actions.attached = &attached;
                    actions.field30D = actions.field3F = 1;
                    actions.field11 = 0;
                    actions.updateNormalState();
                    require(!actions.field30D && attached.field2C4 == 17 && attached.field2D4 == 17,
                        "Release through the owned attachment preserves already-active transition clocks.");
                    actions.fields31C[4] = 14;
                    actions.fields40[0] = 0;
                    actions.updateNormalState();
                    require(attached.field2C0 == 6 && attached.field2C4 == 0 && attached.field2CC == 0 &&
                        attached.field2D0 == 2 && attached.field2D4 == 17,
                        "Mode fourteen intentionally invokes the first transition twice and leaves the second untouched.");
                    actions.attached = nullptr;
                }
                for (auto index : attachments) require(!playerModel.attachment(index).object, "Attached-state destruction clears borrowed model links.");
                require(!playerModel.attachment(singleAttachment).object, "Single attachment destruction clears its model link.");
            }
            playercontrols::State controls;
            controls.history[0].value = 7;
            controls.motions[0].value = 9;
            controls.reset();
            require(controls.history[0].value == 7 && controls.motions[0].value == 0,
                "Original control reset preserves history while resetting current motion.");
            controls.settings.configure(2);
            const auto settings = controls.settings;
            controls.settings.configure(-1);
            require(controls.settings.fields == settings.fields && controls.settings.flags == 0xa3,
                "Invalid control mode preserves the existing mapping.");
            const actormodel::Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
            require(playerModel.capacity() == 130 && playerModel.model == luigi.model &&
                playerModel.matrix(0, 129) == identity && playerModel.matrix(3, 129) == identity &&
                !playerModel.attachment(129).object && playerModel.channels[15].frame == 0 &&
                playerModel.effects[7].value == 0 && playerModel.color == 0xffffffff,
                "Original S3DModel construction owns all four matrix banks and attachment state.");
            bool undersized = false;
            try {
                actormodel::Instance invalid(luigi.model, 1);
            } catch (const std::runtime_error&) {
                undersized = true;
            }
            require(undersized, "Original S3DModel constructor rejects undersized actor workspace.");
            playerModel.attachment(0).field0 = 4;
            playerModel.attachment(0).object = &playerModel;
            playerModel.attachment(129).field0 = 9;
            playerModel.bindKey(luigi.keys.at(130));
            require(playerModel.key == luigi.keys.at(130) && playerModel.attachment(0).field0 == 0 &&
                playerModel.attachment(0).object == &playerModel && playerModel.attachment(129).field0 == 9,
                "Key changes reset declared joint cursors only, retaining attachment objects and spare capacity.");
            playerModel.attachment(0).field0 = 5;
            playerModel.bindKey(luigi.keys.at(130));
            require(playerModel.attachment(0).field0 == 5, "Rebinding identical key preserves animation cursors.");
            const auto originalModel = luigi.model;
            auto invalidLayout = layout;
            invalidLayout.keys = 121;
            require(!luigi.read(luigiBytes, invalidLayout, error) && luigi.model == originalModel,
                "Invalid model allocation retains the previous archive and resource identity.");
            require(package.read("files/Map/map2.szp", compressed, error) && roomarchive::decompress(compressed, archive, error) &&
                roomarchive::read(archive, "jmp/characterinfo", characterInfo, error) && table.attach(characterInfo, error), error);
            RuntimeProbe owned;
            {
                gamearchive::Archive mapArchive;
                require(mapArchive.read(archive, error), error);
                gamearchive::Resources effectResources(mapArchive);
                j3dmodel::MaterialDefaults defaults;
                require(defaults.read(dol, error), error);
                const auto definitions = modeleffects::readDefinitions(dol, 2);
                {
                    gamearchive::Resources commonResources(common);
                    particles::EmitterPool emitters(4, 0, 0);
                    particles::EffectFactory particles(dol, commonResources, emitters);
                    playeractor::View player{};
                    elements::Pool elements(16, 4);
                    elements::Projectile ice{}, water{};
                    gameflags::State flags;
                    mission::Resources mission;
                    mapcollision::State collision(mission);
                    std::uint32_t random = 1;
                    modeleffects::Manager manager({commonResources, emitters, particles, player, elements, ice, water,
                        flags, collision, random, [](unsigned, const std::array<float, 3>&, unsigned) {},
                        [] { return std::array<std::uint8_t, 3>{}; }});
                    manager.initialize(dol, 2, effectResources, defaults);
                    const std::array<unsigned, 12> types{0x2d, 0x2b, 0x24, 0x25, 0xa, 0x22, 0xb, 0x20, 0x21, 0xe, 0x27, 0x29};
                    require(manager.registered().size() == 12 + definitions.size(), "Model-effect manager registers specialized pools before ordinary pools.");
                    for (unsigned i = 0; i < types.size(); ++i) require(manager.registered()[i]->definition.type == types[i],
                        "Model-effect manager preserves the original registration order.");
                    require(manager.drawBuffers[0]->packets.size() == 1024 && manager.drawBuffers[1]->packets.size() == 1024 &&
                        manager.drawBuffers[0]->interval == 9999.0f / 1024, "Original model-effect draw buffers retain capacity and depth interval.");
                    auto* river = static_cast<modeleffects::RiverOwner*>(manager.registered()[11]->owners[0].get());
                    river->hits = 17;
                    river->transition = 23;
                    std::weak_ptr<j3dmodel::Animation> texture = static_cast<modeleffects::RiverPool*>(manager.registered()[11])->textures[0];
                    manager.clear();
                    require(texture.expired() && manager.registered().empty() && !manager.drawBuffers[0] && !manager.drawBuffers[1],
                        "Scene teardown releases model resources, registry and both draw buffers.");
                    manager.initialize(dol, 2, effectResources, defaults);
                    require(manager.registered()[11]->owners[0].get() == river && river->hits == 17 && river->transition == 23 && !river->emitter,
                        "Scene reload retains the original river singleton and counters while clearing its scene emitter.");
                    manager.clear();
                    manager.clear();
                }
                {
                    modeleffects::BridgePool bridge;
                    gamearchive::Resources waterfallResources(common);
                    particles::EmitterPool waterfallEmitters(2, 0, 0);
                    gameflags::State waterfallFlags;
                    unsigned waterfallSounds = 0;
                    modeleffects::WaterfallPool waterfall(waterfallResources, waterfallEmitters, waterfallFlags,
                        [&](unsigned id, const std::array<float, 3>& position, unsigned extra) {
                            require(id == 0x108c && position == std::array<float, 3>{1, 2, 3} && extra == 0,
                                "Imported waterfall preserves original positional sound arguments.");
                            ++waterfallSounds;
                        });
                    waterfall.load(effectResources, defaults);
                    modeleffects::Factory waterfallFactory;
                    waterfallFactory.add(waterfall);
                    auto* cascade = waterfallFactory.create(0x27, std::array<float, 3>{1, 2, 3});
                    require(cascade && waterfallEmitters.attempts == 2 && !cascade->model->model->materials.entries.empty(),
                        "Original waterfall model, animation and both common-archive particle resources create together.");
                    waterfall.advance(*cascade, true);
                    require(waterfallSounds == 1 && cascade->frame == 1, "Imported waterfall advances with its original sound event.");
                    cascade->release();
                    particles::EmitterPool iceEmitters(2, 0, 0);
                    modeleffects::PlayerIcePool playerIce(waterfallResources, iceEmitters);
                    playerIce.load(effectResources, defaults);
                    modeleffects::Factory playerIceFactory;
                    playerIceFactory.add(playerIce);
                    auto* icePlayer = playerIceFactory.create(0xb, std::array<float, 3>{4, 5, 6});
                    playerIce.advance(*icePlayer, false);
                    auto& iceOwner = static_cast<modeleffects::PlayerIceOwner&>(*icePlayer);
                    require(iceOwner.emitters[0] && iceOwner.emitters[1] && iceOwner.emitters[0]->position == std::array<float, 3>{4, 5, 6} &&
                        iceOwner.frame == 1 && iceOwner.model->model->jointCalculators[0]->animation == playerIce.transform,
                        "Imported player ice links its transform, texture and common-archive emitters without room visibility gating.");
                    iceOwner.frame = playerIce.transform->frameCount - 1;
                    playerIce.advance(iceOwner, false);
                    require(!(iceOwner.flags & 1) && !iceOwner.emitters[0] && !iceOwner.emitters[1],
                        "Imported player ice retires particle ownership at the original transform animation boundary.");
                    {
                        struct Volume : playermodels::VacuumVolume {
                            bool hit = false;
                            bool intersects(double radius, std::array<float, 3>&, const std::array<float, 3>&) const override {
                                require(radius == 200, "Cobweb uses the original suction query radius.");
                                return hit;
                            }
                        } volume;
                        playeractor::View view{};
                        const std::array<float, 3> nozzle{};
                        std::array<float, 3> direction{0, 0, 1};
                        view.field8A4 = &nozzle;
                        view.field8A8 = &direction;
                        view.field8B4 = &volume;
                        particles::EmitterPool fragments(2, 0, 0);
                        particles::EffectFactory fragmentFactory(dol, waterfallResources, fragments);
                        modeleffects::CobwebPool webs(view, fragmentFactory);
                        webs.load(effectResources, defaults);
                        modeleffects::Factory webFactory;
                        webFactory.add(webs);
                        const j3dmodel::Matrix webMatrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 100};
                        std::array<modeleffects::CobwebOwner*, 10> created;
                        for (auto& web : created) web = modeleffects::createCobweb(webFactory, webMatrix);
                        require(created[0] && created[9] && !modeleffects::createCobweb(webFactory, webMatrix) &&
                            created[0]->model->model == created[1]->model->model &&
                            created[0]->secondary->model != created[1]->secondary->model && created[0]->color != created[1]->color &&
                            created[0]->secondary->matrix == webMatrix,
                            "Cobweb owns ten dual models with shared first geometry and independent secondary material/BRK state.");
                        created[0]->release();
                        auto* web = modeleffects::createCobweb(webFactory, webMatrix);
                        require(web == created[0], "Cobweb allocation ignores the release bit and immediately reuses the first inactive owner.");
                        web->frame = 2;
                        web->flags &= ~2;
                        webs.advance(*web, true);
                        require(web->frame == 1 && web->step == 1, "Cobweb relaxes without suction even when the generic running bit is clear.");
                        web->flags |= 0x10;
                        webs.advance(*web, true);
                        require(web->frame == 1, "Cobweb pause suppresses its entire update.");
                        web->flags &= ~0x10;
                        webs.advance(*web, false);
                        require(web->frame == 1, "Cobweb visibility gate preserves its animation state.");
                        volume.hit = true;
                        web->frame = 54;
                        webs.advance(*web, true);
                        require(web->frame == 55 && web->step == -1 && !(web->flags & 0x40),
                            "Wrong-facing suction oscillates at the original frame-55 limit.");
                        webs.advance(*web, true);
                        require(web->frame == 54, "Cobweb reverses its oscillation after the upper boundary.");
                        web->frame = 1;
                        webs.advance(*web, true);
                        require(web->frame == 0 && web->step == 1, "Cobweb oscillation reverses at zero.");
                        direction = {0, 0, -1};
                        web->frame = 54;
                        webs.advance(*web, true);
                        require(web->frame == 55 && (web->flags & 0x40) && web->anchor == nozzle && web->inverseDistance == 0.0001f,
                            "Correct-facing suction captures the nozzle position and reciprocal squared distance.");
                        web->frame = 0;
                        webs.advance(*web, true);
                        require(web->model->matrix[11] == 80 && web->secondary->matrix == webMatrix && web->frame == 1 &&
                            web->model->scale == std::array<float, 3>{1, 1, 1},
                            "Captured cobweb moves its first model toward the retained anchor while preserving the placed secondary model.");
                        web->frame = webs.transform->frameCount - 1;
                        webs.advance(*web, true);
                        require((web->flags & 0x80) && web->model->scale == std::array<float, 3>{1, 1, 1} &&
                            fragments.attempts == 2 && fragmentFactory.history[0] == 1 && fragmentFactory.history[1] == 0,
                            "Cobweb completion emits the two original particle types in order and restores model scale.");
                        web->flags &= ~1;
                        web->frame = webs.transform->frameCount - 1;
                        webs.advance(*web, true);
                        require(web->frame == webs.transform->frameCount && fragments.attempts == 2,
                            "Terminal cobweb animation advances without the active bit and never repeats its completion particles.");
                        for (auto* entry : created) entry->release();
                        auto placementTable = [&] {
                            const std::array<const char*, 7> names{"name", "pos_x", "pos_y", "pos_z", "dir_x", "dir_y", "dir_z"};
                            std::vector<std::uint8_t> bytes(132);
                            word(bytes, 0, 1); word(bytes, 4, 7); word(bytes, 8, 100); word(bytes, 12, 32);
                            for (unsigned i = 0; i < names.size(); ++i) {
                                word(bytes, 16 + i * 12, hash(names[i]));
                                word(bytes, 20 + i * 12, 0xffffffff);
                                word(bytes, 24 + i * 12, (i ? 4 + i * 4 : 0) << 16);
                            }
                            std::memcpy(bytes.data() + 100, "net", 4);
                            word(bytes, 108, 0x3f800000); word(bytes, 112, 0x40000000); word(bytes, 116, 0x40400000);
                            word(bytes, 124, 0xc2b40000);
                            jmp::ToolData table;
                            require(table.attach(bytes, error), error);
                            return table;
                        };
                        auto placement = placementTable(), different = placementTable();
                        jmp::ToolDataRef placed{&placement, 0}, other{&different, 0};
                        actormodel::InitialPose webPose(dol);
                        modeleffects::CobwebPlacements placements(webFactory, webPose);
                        for (unsigned i = 0; i < 10; ++i) require(placements.create(placed), "Original cobweb placement admits duplicate row indices.");
                        require(!placements.create(placed) && !placements.message(placed, "appear"),
                            "Cobweb placement retains its ten-record limit and empty command handler.");
                        auto expectedMatrix = webPose.rotation({0, 16383, 0});
                        expectedMatrix[3] = 1; expectedMatrix[7] = 2; expectedMatrix[11] = 3;
                        require(created[0]->model->matrix == expectedMatrix && created[0]->secondary->matrix == expectedMatrix,
                            "Negative cobweb angles add the original 180 degrees before conversion, retaining that placement quirk.");
                        require(placements.remove(other) && !(created[0]->flags & 1) && (created[1]->flags & 1),
                            "Cobweb removal matches the first row index even from a different table resource.");
                        require(placements.create(placed) && (created[0]->flags & 1), "Removed cobweb records recycle into the free list.");
                        placements.clear();
                        require(!placements.create(placed) && (created[0]->flags & 1),
                            "Cobweb receiver teardown clears records without taking ownership of model-pool destruction.");
                    }
                    {
                        particles::EmitterPool fragments(2, 0, 0);
                        particles::EffectFactory fragmentFactory(dol, waterfallResources, fragments);
                        std::uint32_t random = 0;
                        std::vector<unsigned> sounds;
                        unsigned scratchReads = 0;
                        modeleffects::CrystalPool crystal(fragmentFactory, random,
                            [&](unsigned sound, const std::array<float, 3>&, unsigned) { sounds.push_back(sound); },
                            [&] { ++scratchReads; return std::array<std::uint8_t, 3>{11, 22, 33}; });
                        crystal.load(effectResources, defaults);
                        modeleffects::Factory factory;
                        factory.add(crystal);
                        auto* owner = dynamic_cast<modeleffects::CrystalOwner*>(factory.create(10, std::array<float, 3>{1, 2, 3}));
                        require(owner && owner->position == std::array<float, 3>{1, 2, 3} && sounds == std::vector<unsigned>{0x18c3} &&
                            fragmentFactory.history[0] == 0x46,
                            "Crystal activation resets its singleton and creates the original particle and positional sound.");
                        owner->flags &= 0xfd;
                        crystal.advance(*owner, true);
                        require(owner->frame == 2 && crystal.transform->frame == 0 && scratchReads == 1 && random != 0 &&
                            sounds.back() == 0x10c4 && owner->model->model->materials.entries.front()->tev->constants[0][0] == 11,
                            "Crystal advances without the running bit and uses explicitly supplied original RGB scratch values.");
                        owner->flags |= 0x10;
                        const auto savedRandom = random;
                        crystal.advance(*owner, true);
                        require(crystal.transform->frame == 2 && owner->frame == 2 && random == savedRandom && scratchReads == 1,
                            "Paused crystal publishes animation clocks but skips sound, RNG and mutation.");
                        owner->flags &= 0xef;
                        owner->release();
                        auto& color = owner->model->model->materials.entries.front()->tev->constants[0];
                        color = {4, 5, 6, 25};
                        crystal.advance(*owner, true);
                        require((owner->flags & 1) && color == std::array<std::uint8_t, 4>{4, 5, 6, 0} && owner->frame == 2 && random == savedRandom,
                            "Crystal fade reaches zero before clearing activation on the following tick.");
                        crystal.advance(*owner, true);
                        require(!(owner->flags & 1), "Crystal fade deactivates below the original alpha threshold.");
                        auto* reused = factory.create(10, std::array<float, 3>{7, 8, 9});
                        require(reused == owner && owner->frame == 0 && owner->position == std::array<float, 3>{7, 8, 9},
                            "Crystal requests always reset and reuse the singleton.");
                        owner->frame = 329;
                        owner->release();
                        crystal.advance(*owner, true);
                        require(owner->frame == 0 && !(owner->flags & 1) && crystal.transform->frame == 329 && scratchReads == 2,
                            "Late crystal release follows the normal final frame rather than the early fade branch.");
                    }
                    {
                        particles::EmitterPool fragments(1, 0, 0);
                        elements::Pool elemental(1500, 15);
                        elements::Projectile ice{{}, 1}, water{{}, 0};
                        gameflags::State flags;
                        mission::Resources mission;
                        mapcollision::State collision(mission);
                        collision.enable(7);
                        unsigned sounds = 0;
                        modeleffects::RiverPool river(waterfallResources, fragments, elemental, ice, water, flags, collision,
                            [&](unsigned sound, const std::array<float, 3>&, unsigned) {
                                require(sound == 0x109f, "Original river transition sound is retained.");
                                ++sounds;
                            });
                        river.load(effectResources, defaults);
                        modeleffects::Factory factory;
                        factory.add(river);
                        auto* owner = dynamic_cast<modeleffects::RiverOwner*>(factory.create(0x29, std::array<float, 3>{}));
                        require(owner && !factory.create(0x29, std::array<float, 3>{}), "River acquisition rejects an already active singleton.");
                        owner->flags |= 0x10;
                        for (unsigned tick = 0; tick < 59; ++tick) river.advance(*owner, true);
                        require(owner->hits == 59 && owner->frame == 0 && !sounds && !(owner->flags & 0x40),
                            "River hit accumulation proceeds while texture animation is paused.");
                        ice.active = 0;
                        river.advance(*owner, true);
                        require(owner->hits == 58, "A missed elemental tick decays river hit accumulation by one.");
                        ice.active = 1;
                        river.advance(*owner, true);
                        river.advance(*owner, true);
                        require(owner->hits == 0 && owner->transition == 1 && (owner->flags & 0x40) && sounds == 1,
                            "The sixtieth hit starts river color transition in the same update.");
                        const auto initialColor = owner->model->model->materials.entries.front()->tev->colors[0];
                        require(initialColor == std::array<std::uint16_t, 4>{8, 105, 8, 255},
                            "River transition starts with the original green TEV color.");
                        for (unsigned tick = 0; tick < 59; ++tick) river.advance(*owner, true);
                        require(owner->transition == 60 && !(owner->flags & 0x80) && !flags.value(0x40),
                            "River remains unfrozen through transition counter sixty.");
                        river.advance(*owner, true);
                        require((owner->flags & 0x80) && !(owner->flags & 0x40) && owner->transition == 60 &&
                            flags.value(0x40) && !(collision.mask() & 0x80) && owner->emitter && sounds == 61 &&
                            owner->emitter->field220 == std::array<float, 3>{1, 0, -1} &&
                            owner->emitter->field22C == std::array<float, 3>{50, 400, 200} &&
                            owner->model->model->materials.entries.front()->tev->colors[0] == std::array<std::uint16_t, 4>{56, 68, 68, 255},
                            "River completion updates the original flag, collision mask, material and cached ice emitter.");
                        auto* retained = owner->emitter;
                        owner->frame = 9;
                        owner->secondFrame = 8;
                        owner->reset();
                        require(owner->emitter == retained && owner->transition == 60 && !owner->frame && !owner->secondFrame,
                            "River reset preserves cached emitter and counters while resetting its texture clocks.");
                        require(factory.create(0x29, std::array<float, 3>{4, 5, 6}) == owner && (owner->flags & 0x80) &&
                            fragments.attempts == 1 && owner->emitter->position == std::array<float, 3>{},
                            "An already-frozen river reuses its retained particle pointer without relocating the emitter.");
                    }
                    bridge.load(effectResources, defaults);
                    modeleffects::Factory factory;
                    factory.add(bridge);
                    auto* first = factory.create(0x22, std::array<float, 3>{1, 2, 3});
                    first->frame = 27;
                    bridge.colorAnimation->frame = 5;
                    auto* second = factory.create(0x22, std::array<float, 3>{4, 5, 6});
                    require(first == second && second->frame == 0 && second->model->matrix[3] == 4 &&
                        bridge.colorAnimation->frame == 5 && bridge.textureAnimation && !second->model->model->materials.entries.empty(),
                        "Original bridge resources bind to a reusable singleton with retained color state and replacement placement.");
                    modeleffects::IceJailPool ice;
                    ice.load(effectResources, defaults);
                    factory.add(ice);
                    auto* frozen = factory.create(0xe, std::array<float, 3>{7, 8, 9});
                    frozen->setFrame(12.5);
                    ice.advance(*frozen, true);
                    require(ice.colorAnimation->frame == 12.5f && frozen->frame == 1 &&
                        !frozen->model->model->materials.entries.empty(),
                        "Original ice-jail model and both animations use the specialized owner frame semantics.");
                    for (unsigned variant = 0; variant < 2; ++variant) {
                        particles::EmitterPool barrierEmitters(0, 0, 0);
                        modeleffects::BarrierPool barrier(variant, barrierEmitters);
                        barrier.load(effectResources, defaults);
                        modeleffects::Factory barrierFactory;
                        barrierFactory.add(barrier);
                        auto* barrierOwner = barrierFactory.create(0x24 + variant, std::array<float, 3>{1, 2, 3});
                        barrier.advance(*barrierOwner, true);
                        auto& wall = static_cast<modeleffects::BarrierOwner&>(*barrierOwner);
                        require(wall.animationFrame == 1 && !wall.particle && !wall.texture &&
                            !wall.model->model->materials.entries.empty(),
                            "Both imported barrier variants initialize their original model/texture animation without inventing particle resource bindings.");
                        modeleffects::IciclePool icicles(variant ? 12 : 10, variant, variant ? 40 : 0, 0x20 + variant);
                        icicles.load(effectResources, defaults);
                        require(icicles.owners.size() == (variant ? 12 : 10) && icicles.transform,
                            "Original icicle pool variants retain their declared capacities and shared transform resource.");
                        const auto& first = dynamic_cast<modeleffects::IcicleOwner&>(*icicles.owners[0]);
                        const auto& second = dynamic_cast<modeleffects::IcicleOwner&>(*icicles.owners[1]);
                        require(first.model->model != second.model->model && first.textureAnimation != second.textureAnimation &&
                            first.model->model->jointCalculators[0]->animation == icicles.transform &&
                            second.model->model->jointCalculators[0]->animation == icicles.transform,
                            "Icicles own distinct mutable models and texture animations while sharing their original transform animation.");
                    }
                }
                modeleffects::SharedPool sharedEffects;
                sharedEffects.load(effectResources, defaults);
                require(sharedEffects.owners.size() == 16 &&
                    sharedEffects.owners[0]->model->model != sharedEffects.owners[1]->model->model,
                    "Original shared effects own separate mutable model data for all sixteen owners.");
                modeleffects::Factory sharedFactory;
                sharedFactory.add(sharedEffects);
                enemytypes::Manager effectParameters;
                require(effectParameters.loadParameters(dol, commonBytes, error), error);
                auto* shadow = modeleffects::createShared(sharedFactory, effectParameters, 0xb, {1, 2, 3});
                require(shadow && shadow->model->scale[0] == shadow->scaleFactor,
                    "Character shared-effect creation reads original type scale and material color parameters.");
                shadow->setPosition({4, 5, 6});
                require(shadow->model->matrix[3] == 4 && shadow->model->matrix[7] == 20 && shadow->model->matrix[11] == 6 &&
                    shadow->model->matrix[0] == shadow->scaleFactor,
                    "Shared-effect position updates preserve original scale and fifteen-unit vertical offset.");
                require(definitions.size() == 32, "Original map-2 model effects use the explicit descriptor count.");
                std::vector<std::unique_ptr<modeleffects::Pool>> modelPools;
                modeleffects::Factory modelFactory;
                for (const auto& definition : definitions) {
                    modeleffects::Pool effects(definition);
                    effects.load(effectResources, defaults);
                    require(effects.owners.size() == definition.capacity && effects.available() && effects.available()->model,
                        "Original model-effect descriptors create their declared owners and shared model resources.");
                    modeleffects::Factory factory;
                    factory.add(effects);
                    auto* first = factory.create(definition.type, std::array<float, 3>{1, 2, 3});
                    require(first == effects.owners.front().get() && first->flags == 7 && first->model->scale == std::array<float, 3>{1, 1, 1} &&
                        first->model->matrix[3] == 1 && first->model->matrix[7] == 2 && first->model->matrix[11] == 3,
                        "Model-effect factory activates the first free owner with unit scale and original translation matrix.");
                    first->flags |= 0x10;
                    const auto retained = first->model->matrix;
                    first->setMatrix({2, 0, 0, 4, 0, 2, 0, 5, 0, 0, 2, 6});
                    require(first->model->matrix == retained, "Paused model-effect attachments retain their existing transform.");
                    first->setPosition({7, 8, 9});
                    require(first->model->matrix[3] == 7 && first->model->matrix[7] == 8 && first->model->matrix[11] == 9,
                        "Direct model-effect position updates are not subject to the attachment matrix gate.");
                    auto pool = std::make_unique<modeleffects::Pool>(definition);
                    pool->load(effectResources, defaults);
                    modelFactory.add(*pool);
                    modelPools.push_back(std::move(pool));
                }
                gamearchive::Resources commonEffects(common);
                particles::EmitterPool emitters(128, 1024, 1024);
                particles::EffectFactory particleFactory(dol, commonEffects, emitters);
                unsigned variant = 0;
                charactereffects::Resources resources(modelFactory, particleFactory, variant);
                for (unsigned type : {0x75u, 0x76u, 0x77u, 0xc2u, 0xc3u, 0xc4u, 0xc5u, 0xc6u, 0xc7u, 0xc8u, 0xc9u, 0xcau, 0xcbu, 0xccu}) {
                    const auto acquired = characterownership::acquire(type, resources);
                    require(acquired.success && acquired.first && acquired.second,
                        "Every original paired character effect resolves through the recovered model and particle factories.");
                    characterownership::Ownership ownership;
                    ownership.attach(acquired);
                    require(ownership.attachmentCount == 2, "Concrete acquired effects enter character ownership through original status queries.");
                    acquired.first->release();
                    acquired.second->release();
                }
            }
            enemies::Manager manager(types, creation, owned, 1, 0);
            manager.attachCharacterInfo(table, 0);
            checkInitialization(manager, owned, creation, 1);
            {
                actormodel::InitialPose pose(dol);
                playercontrols::State controls;
                controls.field2C = 0.75f;
                std::array<float, 3> position{1, 2, 3}, weapon{10, 20, 30}, direction{0, 0, 1};
                playeractor::View view{};
                view.position = &position;
                view.controls = &controls;
                view.angle = static_cast<std::int16_t>(0xc000);
                view.field8A4 = &weapon;
                view.field8A8 = &direction;
                view.field8B0 = 2;
                view.fieldF24 = 7;
                view.fieldF17 = 3;
                view.field8C9 = 4;
                roomtraversal::State visibility;
                visibility.initialize(255);
                std::uint32_t flags = 0;
                std::uint16_t saved = 0;
                visibility.mark(0, 2, manager.playerRoom(0), flags, saved, 0, 0, 0, {}, {});
                particles::SceneContexts contexts;
                contexts.player.distance = 12;
                contexts.player.field5B = 9;
                particles::SecondarySource secondary{true, &position, 8, {3, 4, 0}, {1, 0, 0}};
                contexts.refresh(view, pose, manager, visibility, &secondary);
                require(contexts.player.suction.position == &weapon && contexts.player.suction.direction == &direction &&
                    contexts.player.suction.strength == 2 && contexts.player.endpoint == std::array<float, 3>{10, 20, 42} &&
                    contexts.player.angle == 49152 && contexts.player.orientation[0] == -1 && contexts.player.height == 202 &&
                    contexts.player.visible && contexts.player.field5B == 9 && contexts.player.field59 == 4 && contexts.player.field5C == 3,
                    "Shared particle context retains original pointers, unsigned angle lookup, endpoint distance and untouched flags.");
                require(contexts.secondary.magnitude == 5 && contexts.secondary.height == 152 && contexts.secondary.field48 == 8,
                    "Secondary scene context retains the original velocity magnitude and 150-unit height offset.");
                view.field8B0 = 4;
                require(contexts.player.suction.strength == 2, "Particle strength stays cached until the scene refresh.");
                contexts.refresh(view, pose, manager, visibility, nullptr);
                require(contexts.player.suction.strength == 4 && contexts.secondary.field48 == 0 && contexts.secondary.height == 152 &&
                    contexts.secondary.actorPosition == &position, "Absent secondary source clears only its active magnitude field.");
            }
            require(owned.actors.size() == 104 && creation.capacity(0) == 130, "Owned executable confirms full pool layout.");
            std::cout << "Owned map2 placement and 104 owner lifecycles verified through test dependencies.\n";
        }
        std::cout << "EnManager lifecycle checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
