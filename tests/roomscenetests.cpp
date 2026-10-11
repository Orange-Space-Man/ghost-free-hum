#include "game/world/roomscenereader.h"

#include <iostream>

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[2]) == "--all") {
        unsigned failures = 0;
        for (unsigned room = 0; room < 74; ++room) {
            roomscene::Scene scene;
            std::string error;
            if (!roomscene::read(argv[1], scene, error, room)) {
                std::cout << room << " FAIL " << error << std::endl;
                ++failures;
            } else {
                std::cout << room << " OK " << scene.mesh.triangles.size() << std::endl;
            }
        }
        return failures == 0 ? 0 : 1;
    }
    if (argc != 2) return 1;
    roomscene::Scene scene;
    std::string error;
    if (!roomscene::read(argv[1], scene, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    std::cout << scene.objects << " furniture objects; " << scene.doors << " doors; " << scene.mesh.triangles.size() << " triangles; " <<
        scene.materials.textures.size() << " texture resources.\n";
    if (scene.objects != 15 || scene.doors != 1 || scene.mesh.triangles.size() != 8793) return 1;
    for (std::size_t index = 8737; index < scene.mesh.triangles.size(); ++index) {
        for (const auto& position : scene.mesh.triangles[index].positions) {
            if (position[1] < -1 || position[1] > 301) {
                std::cerr << "Door geometry is outside its closed-pose height range.\n";
                return 1;
            }
        }
    }
    return 0;
}
