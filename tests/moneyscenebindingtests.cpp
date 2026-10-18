#include "../recovered/moneyscatterscenebindings.h"
#include "../recovered/moneyscattermodels.h"
#include "../recovered/moneyscatterowner.h"
#include "../recovered/moneyscatterplacements.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <cstring>

namespace {
    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
    struct Archive : particles::ArchiveResources {
        particles::Resource get(std::uint32_t, std::string_view) override {
            return std::make_shared<std::vector<std::uint8_t>>(0xc0);
        }
    };

    jmp::ToolData placements() {
        std::vector<std::uint8_t> bytes(76 + 2 * 24);
        const auto word = [&](unsigned offset, unsigned value) {
            for (unsigned i = 0; i < 4; ++i) bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (24 - i * 8));
        };
        word(0, 2); word(4, 5); word(8, 76); word(12, 24);
        const char* names[]{"name", "room_no", "pos_x", "pos_y", "pos_z"};
        const unsigned offsets[]{0, 8, 12, 16, 20};
        for (unsigned i = 0; i < 5; ++i) {
            unsigned hash = 0;
            for (auto* name = names[i]; *name; ++name) hash = (hash * 256 + static_cast<unsigned char>(*name)) % 33554393;
            word(16 + 12 * i, hash); word(20 + 12 * i, 0xffffffff); word(24 + 12 * i, offsets[i] << 16);
        }
        for (unsigned row = 0; row < 2; ++row) {
            const unsigned base = 76 + row * 24;
            std::memcpy(bytes.data() + base, row ? "bill" : "coin", 5);
            word(base + 8, 0x105);
            word(base + 12, 0x42f00000); word(base + 16, 0x42200000); word(base + 20, 0x41f00000);
        }
        jmp::ToolData table;
        std::string error;
        check(table.attach(bytes, error), error.c_str());
        return table;
    }
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Pass the local executable for particle resource tables.");
        std::ifstream stream(argv[1], std::ios::binary);
        check(bool(stream), "Cannot open original executable.");
        const std::vector<std::uint8_t> executable{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        roomlocation::Rooms rooms(std::vector<roomlocation::Room>{
            {3, {0, 0, 0, 100, 100, 100}, {}}, {0x01000007, {100, 0, 0, 200, 100, 100}, {}}});
        roomtraversal::State visibility;
        visibility.initialize(2);
        std::vector<std::uint32_t> flags{0x100, 0x200};
        std::array<std::uint16_t, 256> saved{};
        unsigned mode = 0;
        mission::Resources mission;
        mapcollision::State collision(mission);
        moneyscatter::Vector player{120, 10, 30}, suctionPosition{1, 2, 3};
        particles::SceneContext context;
        context.height = 80;
        context.suction.position = &suctionPosition;
        float minimumScale = 0.2f;
        std::uint32_t random = 1;
        Archive archive;
        particles::EmitterPool emitters(3, 0, 0);
        particles::EffectFactory factory(executable, archive, emitters);
        particles::Batch sparkles(executable, archive, emitters, 1, 0);
        unsigned eventCount = 0, soundCount = 0, attractionCount = 0;
        moneyscatter::SceneDependencies dependencies{rooms, visibility, flags, saved, mode, collision, player, context,
            minimumScale, random, factory, sparkles,
            [&](unsigned event, const playerevents::Payload& payload) {
                check(event == 0x26 && payload.word(0x94) == 1, "Scene dispatch must preserve collection payload.");
                ++eventCount;
            },
            [&](float radius, moneyscatter::Particle& particle) {
                check(radius == 100, "Scene attraction retains the requested radius.");
                particle.acceleration = {1, 2, 3};
                ++attractionCount;
                return 1;
            },
            [&](unsigned sound, const moneyscatter::Vector& position, unsigned extra) {
                check(sound == 0x1841 && position == player && extra == 0, "Bounce routing must preserve sound, position and argument.");
                ++soundCount;
            }};
        moneyscatter::SceneBindings bindings(dependencies);
        check(bindings.roomAt(player, 0xffffffff) == 0x01000007 && bindings.roomFlags(0x01000007) == 0x200,
            "Room resolution and runtime flags use original encoded room IDs.");
        visibility.mark(0, mode, 0x01000007, flags[1], saved[7], 0, -1, 0, {}, {});
        check(bindings.visibilityFlags(0x01000007) == 1 && bindings.roomFlags(0x01000007) == 0x202,
            "Visibility flags must remain distinct from persistent room flags.");
        bindings.notifyPlacedCollection(0x01000007);
        check(flags[1] == 0x242 && saved[7] == 0, "Ordinary mode sets collection state without modifying saved-room flags.");
        mode = 2;
        saved[7] = 0x100;
        bindings.notifyPlacedCollection(0x010000ff);
        check(saved[7] == 0x108 && saved[1] == 0 && saved[255] == 0,
            "Saved flags use the selected room record's low byte, not its index or the query's low byte.");
        check(bindings.random() == 1015568748 && random == 1015568748, "Scene random draws advance the shared gameplay generator.");
        check(&bindings.playerPosition() == &player && bindings.playerTop() == 80 && bindings.suctionPosition() == suctionPosition,
            "Scene bindings must use retained player position and refreshed context values.");
        minimumScale = 0.4f;
        check(bindings.suctionMinimumScale() == 0.4f, "Suction scale must remain bound to scene state.");
        moneyscatter::Particle particle;
        check(bindings.suction(100, particle) == 1 && attractionCount == 1 && particle.acceleration == moneyscatter::Vector{1, 2, 3},
            "Attraction uses the supplied original route and retains particle mutations.");
        bindings.bounceSound(player);
        check(soundCount == 1, "Bounce sound must reach positional playback.");
        bindings.sparkle(player);
        check(sparkles.cursor == 1 && sparkles.entries()[0]->position == player, "Scene sparkle reaches the retained emitter batch.");
        moneyscatter::Request request;
        request.placedMoney = true;
        particle.position = player;
        particle.currentRoom = 0x01000007;
        flags[1] &= ~0x40u;
        check(moneyscatter::collect(request, particle, false, bindings) && eventCount == 1 && factory.history[0] == 310 &&
            (flags[1] & 0x40) && particle.lifetime == 0, "Collection must connect dispatch, real effect allocation and room mutation.");
        particle.model = std::make_unique<j3dmodel::ModelInstance>(std::make_shared<j3dmodel::ModelData>(), 0x10000);
        particle.forward = {0, 1, 0};
        particle.up = {0, 0, 1};
        particle.scale = 1;
        bindings.updateModel(particle);
        check(particle.model->matrix[3] == particle.position[0], "Scene model callback writes the owned J3D instance.");
        {
            moneyscatter::Pool storage({30, 5, 2}, false);
            check(storage.requests.free.count == 35 && storage.active.count == 0,
                "Deferred pool construction must leave all requests available until model initialization finishes.");
            moneyscatter::Models models(storage, std::make_shared<j3dmodel::ModelData>(), 0);
            check(storage.requests.free.count == 35 && storage.particles.at(199).model,
                "Original owner creates model storage before reserving the persistent request.");
            storage.initializePersistent();
            check(&storage.persistent() == &storage.requests.at(34) && storage.active.count == 1,
                "Deferred initialization retains the original last-slot reservation.");
        }
        {
            const actormodel::InitialPose pose(executable);
            moneyscatter::Owner owner({30, 5, 2}, std::make_shared<j3dmodel::ModelData>(), pose, dependencies, 357);
            check(owner.storage().particles.free.count == 200 && owner.storage().requests.free.count == 34 &&
                owner.modelStorage().auxiliary.model, "Scene owner must finish both model and request initialization.");
            auto* request = owner.scatter({120, 50, 30}, {0, 0, 1}, 1, 2, 4, 0);
            auto* coin = request->particles.sentinel.next;
            check(coin->model && coin->model->matrix[7] == 50 && owner.storage().particles.free.count == 199,
                "Owner admission connects real initialization to its retained J3D instance.");
            coin->motion = 3;
            coin->sparkleCounter = 80;
            owner.updateCoins({10, 20, 30}, {0, 0, 1});
            check(coin->lifetime == 1 && sparkles.cursor == 1 && sparkles.entries()[0]->flags == 0xe &&
                owner.modelStorage().auxiliary.spin == 360 && owner.modelStorage().auxiliary.position[2] == -9970,
                "Coin frame resets the sparkle batch before particle emission, then updates the auxiliary model.");
            owner.storage().paused = true;
            owner.updateCoins({99, 98, 97}, {1, 0, 0});
            check(coin->lifetime == 1 && sparkles.cursor == 0 && sparkles.entries()[0]->flags == 0xc &&
                owner.modelStorage().auxiliary.spin == 360 && owner.modelStorage().auxiliary.position[2] == -9970,
                "Pause leaves coins and auxiliary transform untouched but still resets the outer sparkle batch.");
            owner.storage().paused = false;
            owner.updateCoins({1, 2, 3}, {});
            check(request->particles.count == 0 && owner.storage().particles.free.count == 200 &&
                owner.storage().active.count == 2 && owner.modelStorage().auxiliary.spin == 3,
                "Expiration returns the particle without removing its request during the same frame.");
            owner.updateCoins({1, 2, 3}, {});
            check(owner.storage().active.count == 1 && owner.storage().requests.free.count == 34,
                "The original persistent request survives ordinary request retirement.");
            auto table = placements();
            jmp::ToolDataRef coinEntry{&table, 0}, billEntry{&table, 1};
            std::array<std::int16_t, 256> indices{};
            indices[5] = 0x101;
            unsigned scratchReads = 0;
            moneyscatter::Placements receiver(owner, rooms, indices, flags, [&](int row) {
                check(row == 0, "Placed initializer retains its JMP row identity.");
                ++scratchReads;
                return moneyscatter::Vector{1, 2, 3};
            });
            jmp::MessageSender sender;
            check(sender.addReceiver(&receiver), "Coin receiver must register with the original message sender.");
            flags[1] &= ~0x40u;
            check(!sender.dispatch(&jmp::MessageReceiver::create, billEntry) &&
                sender.dispatch(&jmp::MessageReceiver::create, coinEntry), "Coin receiver must recognize only coin entries.");
            auto& retained = owner.storage().persistent();
            auto* placed = retained.particles.sentinel.next;
            check(placed == &owner.storage().particles.at(199) && placed->position == moneyscatter::Vector{120, 50, 30} &&
                placed->placementRow == 0 && placed->room == 0x01000007 && placed->currentRoom == 0x01000007 &&
                placed->lifetime == 1000 && placed->model && scratchReads == 1,
                "Placement masks room indices, raises height, retains row/room identity and initializes through the persistent owner.");
            check(sender.dispatch(&jmp::MessageReceiver::create, coinEntry) && retained.particles.count == 2,
                "Original placement admits duplicate rows rather than deduplicating them.");
            check(sender.dispatch(&jmp::MessageReceiver::remove, coinEntry) && retained.particles.count == 1 &&
                retained.particles.sentinel.next == &owner.storage().particles.at(198),
                "Removal recycles only the first matching row.");
            flags[1] |= 0x40;
            check(sender.dispatch(&jmp::MessageReceiver::create, coinEntry) && retained.particles.count == 1 && scratchReads == 2,
                "Suppressed placement remains handled without reading initializer scratch or allocating.");
            flags[1] &= ~0x40u;
            for (unsigned i = 0; i < 199; ++i) check(owner.place({120, 50, 30}, 1000 + i, 0x01000007, {1, 2, 3}),
                "Persistent placement should fill available particle slots.");
            check(sender.dispatch(&jmp::MessageReceiver::create, coinEntry) && retained.particles.count == 200 && scratchReads == 2,
                "Exhausted placement remains handled without invoking initialization.");
            check(!sender.dispatch(&jmp::MessageReceiver::message, coinEntry, "hide"), "Coin receiver has no command handler.");
        }
        dependencies.dispatch = {};
        bool rejected = false;
        try { moneyscatter::SceneBindings missing(dependencies); }
        catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "Missing original dispatch must not silently turn collection into a no-op.");
        std::cout << "Money scene binding checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
