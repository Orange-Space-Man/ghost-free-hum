#include "../recovered/billscatterscene.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    void check(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Original executable required.");
        std::ifstream input(argv[1], std::ios::binary);
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
        actormodel::InitialPose pose(bytes);
        roomlocation::Rooms rooms(std::vector<roomlocation::Room>{{0, {0, 0, 0, 100, 100, 100}, {}}});
        roomtraversal::State visibility;
        visibility.initialize(1);
        mission::Resources mission;
        mapcollision::State collision(mission);
        billscatter::Vector player{1000, 0, 0}, suctionPosition{};
        particles::SceneContext context;
        context.height = 100;
        context.suction.position = &suctionPosition;
        std::uint32_t gameplayRandom = 7, particleRandom = 11;
        float flutterStrength = 1;
        bool paused = false;
        std::uint8_t blinkThreshold = 2;
        unsigned events = 0;
        billscatter::SceneDependencies dependencies{rooms, visibility, collision, player, context,
            gameplayRandom, particleRandom, flutterStrength, paused, blinkThreshold,
            [&](unsigned event, const playerevents::Payload& payload) {
                check(event == 0x27 && payload.word(0x98) == 1, "Scene preserves bill collection event.");
                ++events;
            }, [](billscatter::Particle&) { return 0; }};
        std::array<std::shared_ptr<j3dmodel::ModelData>, 3> models;
        for (auto& model : models) model = std::make_shared<j3dmodel::ModelData>();
        billscatter::Scene scene(models, pose, dependencies);
        scene.scatter({10, 20, 30}, {1, 0, 0}, 0, 0, 3, 2);
        auto expected = 7u;
        for (unsigned i = 0; i < 6; ++i) expected = expected * 0x19660du + 0x3c6ef35fu;
        check(gameplayRandom == expected && particleRandom == 11, "Admission consumes only shared gameplay random state.");
        for (auto& pool : scene.storage().pools) {
            auto& particle = pool->particles.at(29);
            check(particle.room == 0 && particle.position == billscatter::Vector{15, 20, 30} && particle.model->matrix[3] == 15,
                "Scene admission binds room resolution and real model transforms.");
            particle.motion = 1;
        }
        paused = true;
        scene.update();
        check(scene.storage().pools[0]->particles.at(29).lifetime == 2, "Shared pause freezes owner updates.");
        paused = false;
        scene.update();
        for (auto& pool : scene.storage().pools)
            check(pool->particles.at(29).lifetime == 1 && pool->active.count == 1, "All owners update through assembled particle lifecycle.");
        scene.update();
        for (auto& pool : scene.storage().pools)
            check(pool->particles.free.count == 30 && pool->active.count == 1, "Expired particles recycle before empty request retirement.");
        scene.update();
        for (auto& pool : scene.storage().pools)
            check(pool->requests.free.count == 15 && pool->active.count == 0, "Next frame retires empty requests.");
        player = {15, 20, 30};
        scene.scatter({10, 20, 30}, {1, 0, 0}, 0, 0, 3, 100);
        for (auto& pool : scene.storage().pools) pool->particles.at(29).motion = 1;
        scene.update();
        check(events == 3, "Referenced player context and event route reach all three owners.");
        dependencies.attraction = {};
        bool rejected = false;
        try { billscatter::Scene invalid(models, pose, dependencies); }
        catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "Missing original attraction route is rejected.");
        std::cout << "Bill scene checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
