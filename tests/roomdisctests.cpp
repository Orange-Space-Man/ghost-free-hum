#include "resources/roomdiscreader.h"
#include "game/world/roommodelreader.h"
#include "game/world/roomgeometry.h"
#include "game/world/roommaterials.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char* argv[]) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (argc == 3) {
        if (!roomdisc::read(argv[1], bytes, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        std::ifstream sample(argv[2], std::ios::binary);
        const std::vector<std::uint8_t> expected{std::istreambuf_iterator<char>(sample), std::istreambuf_iterator<char>()};
        roommodel::Model model;
        if (bytes != expected || !roommodel::inspect(bytes, model, error) || model.nodes.size() != 9) {
            std::cerr << "ISO bytes or parsed hierarchy differ from the reference sample.\n";
            return 1;
        }
        std::cout << "ISO read matches all " << bytes.size() << " sample bytes; 9 hierarchy records.\n";
        roomgeometry::Mesh mesh;
        if (!roomgeometry::decode(bytes, mesh, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        roommaterials::Collection materials;
        if (!roommaterials::read(bytes, mesh, materials, error) || materials.textures.size() != 27 || materials.materials.size() != 29) {
            std::cerr << "Material decode failed: " << error << '\n';
            return 1;
        }
        std::cout << materials.textures.size() - 1 << " textures; " << materials.materials.size() << " materials.\n";
        std::cout << mesh.batches << " batches; " << mesh.triangles.size() << " triangles; bounds ";
        for (int axis = 0; axis < 3; ++axis) {
            std::cout << mesh.minimum[axis] << ':' << mesh.maximum[axis] << ' ';
        }
        std::cout << '\n';
        return 0;
    }
    const auto path = std::filesystem::temp_directory_path() / ("polterport-disc-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".iso");
    std::vector<std::uint8_t> header(0x440, 0);
    for (int test = 0; test < 4; ++test) {
        if (test == 1) {
            header[0x1c] = 0xc2;
            header[0x1d] = 0x33;
            header[0x1e] = 0x9f;
            header[0x1f] = 0x3d;
        }
        if (test == 2) {
            const std::string id = "GLME01";
            std::copy(id.begin(), id.end(), header.begin());
            header[0x424] = 0xff;
        }
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(header.data()), test == 3 ? 16 : header.size());
        file.close();
        const bool accepted = roomdisc::read(path, bytes, error);
        std::filesystem::remove(path);
        if (accepted || error.empty() || !bytes.empty()) {
            std::cerr << "Malformed ISO was not rejected cleanly.\n";
            return 1;
        }
    }
    if (roomdisc::read(path, bytes, error) || error.empty()) {
        return 1;
    }
    return 0;
}
