#include "../recovered/leafscatterscene.h"
#include "../recovered/scatterattraction.h"
#include "../recovered/moneyscatterframe.h"
#include "game/startup/startupresources.h"
#include "resources/roomarchivereader.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    void check(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
    bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }
    struct Services : leafscatter::UpdateServices {
        int result = 0;
        unsigned sweeps = 0;
        billscatter::Vector player{10000, 0, 0};
        int suction(billscatter::Particle&) override { return result; }
        billscatter::Vector suctionPosition() override { return {}; }
        billscatter::Vector playerPosition() override { return player; }
        float playerTop() override { return 100; }
        mapcollision::SphereContact sweep(const billscatter::Vector&, float, const billscatter::Vector&) override { ++sweeps; return {}; }
    };
    struct Render : leafscatter::RenderServices {
        unsigned draws = 0, views = 0;
        void draw(j3dmodel::ModelInstance&) override { ++draws; }
        void view(j3dmodel::ModelInstance&, const j3dmodel::Matrix& camera) override {
            check(camera[3] == 17, "Leaf view receives current camera.");
            ++views;
        }
    };
    struct Archive : particles::ArchiveResources {
        particles::Resource get(std::uint32_t, std::string_view) override { return std::make_shared<std::vector<std::uint8_t>>(0xc0); }
    };
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Original imported package required.");
        std::string error;
        gameassets::Package package;
        check(package.open(argv[1], error), error);
        std::vector<std::uint8_t> dol;
        check(package.read("sys/main.dol", dol, error), error);
        actormodel::InitialPose pose(dol);
        auto model = std::make_shared<j3dmodel::ModelData>();
        billscatter::Pool pool(model, 50);
        model->materials.originals.resize(1);
        model->materials.originals[0].mode = 3;
        auto material = std::make_unique<j3dmodel::Material>();
        material->original = &model->materials.originals[0];
        material->tev = std::make_unique<j3dmodel::MaterialTevBlock>();
        material->tev->capacity = 1;
        model->materials.entries.push_back(std::move(material));
        auto* request = pool.requests.free.take();
        auto* particle = pool.particles.free.take();
        request->particles.append(*particle);
        particle->sizeRatio = 170;
        particle->scale = 0.1f;
        particle->lifetime = 10;
        particle->forward = {0, -1, 0};
        particle->up = {0, 0, 1};
        particle->acceleration = {20, 0, 0};
        Services services;
        std::uint32_t random = 1, particleRandom = 2;
        auto advance = [&] { return leafscatter::advance(pool, *request, *particle, {100, 200, 250}, 0, particleRandom, random, pose, services); };
        check(advance() && near(particle->velocity[0], 18.9f) && near(particle->velocity[1], -1.75f),
            "Leaf damping and gravity differ from bills and force precedes clamp.");
        check(model->materials.entries[0]->tev->colors[0] == std::array<std::uint16_t, 4>{100, 200, 250, 255} &&
            model->materials.entries[0]->mode == 3 && (model->materials.entries[0]->identityFlags & 0x80000000),
            "Live leaf update changes original material and TEV color before model transform.");
        request->suctionReady = particle->visible = true;
        services.result = 2;
        const auto previous = particle->position;
        check(!advance() && request->particles.count == 1 && particle->position == previous && pool.particles.free.count == 49,
            "Original leaf capture returns false without recycling or updating.");
        services.result = 1;
        particle->position = {75, 0, 0}; particle->velocity = {}; particle->acceleration = {};
        check(advance() && near(particle->scale, 0.325f) && particle->lifetime == 9,
            "Leaf suction uses size-dependent maximum and minimum ratio 0.3 without aging.");
        services.result = 0;
        services.player = particle->position;
        particle->motion = 1;
        check(!advance() && request->particles.count == 0, "Contact removes leaf without money events.");

        particles::SceneContext context;
        billscatter::Vector suctionPosition{}, axis{0, 0, 1};
        context.suction = {&suctionPosition, &axis, 1};
        billscatter::Particle probe;
        probe.position = {0, 0, 100};
        auto beforeRandom = random;
        check(scattraction::apply(probe, 0, context, 200, random) == 0 && random == beforeRandom,
            "Inactive suction has no random side effects.");
        check(scattraction::apply(probe, 1, context, 200, random) == 1 && probe.velocity[2] < 0 && probe.acceleration[2] < 0,
            "Original radial suction applies force toward origin.");
        probe.position = {0, 0, -100}; beforeRandom = random;
        check(scattraction::apply(probe, 1, context, 200, random) == 0 && random == beforeRandom, "Suction rejects rear cone without random draw.");
        probe.position = {0, 0, 20};
        check(scattraction::apply(probe, 1, context, 200, random) == 2, "Capture boundary is inclusive.");

        jaudio::Resources resources;
        check(jaudio::read(package, resources, error), error);
        jaudio::PositionOwners positions;
        jaudio::PositionalPlayback audio(positions, dol, package, resources);
        jaudio::DspMixer mixer;
        check(mixer.configure(resources.effects, error), error);
        roomlocation::Rooms rooms(std::vector<roomlocation::Room>{{0, {-1000, -1000, -1000, 1000, 1000, 1000}, {}}});
        roomtraversal::State visibility; visibility.initialize(1);
        mission::Resources mission;
        std::vector<std::uint8_t> compressed, archive, collisionBytes;
        check(package.read("files/Map/map2.szp", compressed, error) && roomarchive::decompress(compressed, archive, error), error);
        check(mission.read(archive, error) && roomarchive::read(archive, "col.mp", collisionBytes, error), error);
        mapcollision::State collision(mission);
        collision.load(collisionBytes);
        billscatter::Vector player{900, 0, 0};
        context.height = 100;
        unsigned suctionMode = 0; float range = 200, flutter = 0; bool paused = false; std::uint8_t blink = 2;
        leafscatter::Scene scene(pose, {rooms, visibility, collision, player, context, suctionMode, range,
            random, particleRandom, flutter, paused, blink, audio, positions});
        Render render;
        j3dmodel::Matrix camera{1, 0, 0, 17, 0, 1, 0, 0, 0, 0, 1, 0};
        scene.draw(render); scene.view(camera, render); scene.update();
        check(render.draws == 0 && render.views == 0, "Inactive scene gates rendering and updates.");
        auto sceneModel = std::make_shared<j3dmodel::ModelData>();
        scene.initialize(sceneModel);
        scene.scatter({0, 50, 100}, {0, 0, 1}, 0, 0, 2, 100);
        check(scene.storage().storage()->active.count == 1 && positions.categories[1] && !positions.categories[1]->binding,
            "Scene starts real positional leaf sound then releases temporary handle binding.");
        auto* leaf = &scene.storage().storage()->particles.at(49);
        leaf->visible = true;
        scene.draw(render); scene.view(camera, render);
        check(render.draws == 1 && render.views == 1, "Leaf render traversal selects visible models only.");
        paused = true; scene.update(); check(leaf->lifetime == 100, "Scene pause preserves leaf lifetime.");
        paused = false; scene.update(); check(leaf->lifetime == 99, "Scene connects full leaf update.");
        bool audible = false;
        for (unsigned block = 0; block < 5000 && !audio.finished(); ++block) {
            if (block % 8 == 0) audio.update(camera);
            mixer.begin(); check(audio.mix(mixer, error), error);
            std::array<std::int16_t, 160> samples{}; mixer.end(samples, 1);
            audible |= std::any_of(samples.begin(), samples.end(), [](auto sample) { return sample != 0; });
        }
        check(audible && audio.finished(), "Original leaf sequence with count port produces PCM and completes.");
        scene.release(); scene.release();
        check(!scene.storage().active(), "Scene teardown releases conditional storage and audio positions.");
        scene.initialize(sceneModel);
        scene.scatter({0, 50, 100}, {0, 0, 1}, 0, 0, 1, 100);
        scene.release();
        audio.stop();
        {
            std::vector<unsigned> order;
            std::vector<std::uint32_t> roomFlags(1);
            std::array<std::uint16_t, 256> saved{};
            unsigned mapMode = 0;
            float minimumScale = 0.2f;
            Archive archiveResources;
            particles::EmitterPool emitters(4, 0, 0);
            particles::EffectFactory effects(dol, archiveResources, emitters);
            particles::Batch sparkles(dol, archiveResources, emitters, 1, 0);
            player = {15, 50, 100};
            moneyscatter::SceneDependencies coinDependencies{rooms, visibility, roomFlags, saved, mapMode, collision,
                player, context, minimumScale, random, effects, sparkles,
                [&](unsigned event, const playerevents::Payload&) { check(event == 0x26, "Coin route stays distinct."); order.push_back(1); },
                [](float, moneyscatter::Particle&) { return 0; }, [](unsigned, const billscatter::Vector&, unsigned) {}};
            moneyscatter::Owner coins({30, 5, 2}, sceneModel, pose, coinDependencies, 0);
            scene.initialize(sceneModel);
            scene.scatter({10, 50, 100}, {1, 0, 0}, 0, 0, 1, 100);
            auto& leaf = scene.storage().storage()->particles.at(49);
            leaf.motion = 1;
            billscatter::Scene bills({sceneModel, sceneModel, sceneModel}, pose,
                {rooms, visibility, collision, player, context, random, particleRandom, flutter, paused, blink,
                    [&](unsigned event, const playerevents::Payload&) {
                        check(event == 0x27 && !order.empty() && order.front() == 1 && leaf.lifetime == 100,
                            "Bills update after coins and before leaves.");
                        order.push_back(2);
                    }, [](billscatter::Particle&) { return 0; }});
            auto* coinRequest = coins.scatter(player, {0, 0, 1}, 1, 100, 0, 0);
            coinRequest->collectionReady = true;
            coinRequest->particles.sentinel.next->motion = 3;
            bills.scatter({10, 50, 100}, {1, 0, 0}, 0, 0, 3, 100);
            for (auto& billPool : bills.storage().pools) billPool->particles.at(29).motion = 1;
            sparkles.emit({});
            moneyscatter::updateFrame(coins, bills, scene, {}, {0, 0, 1});
            check(order == std::vector<unsigned>{1, 2, 2, 2} && leaf.lifetime == 0 && sparkles.cursor == 0,
                "Original outer frame resets sparkles, updates coins, all bill owners, then leaves.");
            check(scene.storage().storage()->particles.free.count == 50, "Outer frame connects leaf contact recycling.");
            scene.release();
            audio.stop();
        }
        std::cout << "Complete leaf section checks passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}



