#include "game/scene/gameplaycamera.h"
#include "resources/gameassetpackage.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
    void word(std::vector<std::uint8_t>& data, unsigned offset, unsigned value) {
        for (unsigned i = 0; i < 4; ++i) data[offset + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }
}

int main(int argc, char** argv) {
    try {
        std::vector<std::uint8_t> data(0x200100);
        word(data, 0, 0x100);
        word(data, 0x48, 0x802ff000);
        word(data, 0x90, 0x200000);
        const auto set = [&](unsigned address, unsigned value) { word(data, address - 0x802ff000 + 0x100, value); };
        const auto scalar = [&](unsigned address, float value) {
            unsigned bits;
            std::memcpy(&bits, &value, 4);
            set(address, bits);
        };
        set(0x802ff220, 0x80300000);
        set(0x80300000, 3 << 24);
        set(0x80300014, 0x80300100);
        set(0x80300160, 0x02000102);
        set(0x80300168, static_cast<unsigned>(-1025));
        set(0x80300174, 1025);
        set(0x8030017c, 340);
        scalar(0x802ffc48, 1120); scalar(0x802ffc4c, 410);
        scalar(0x802ffc50, 200); scalar(0x802ffc54, 280);
        scalar(0x804d0098, 300); scalar(0x804d9cec, 100);
        scalar(0x804d9ab8, 40);
        scalar(0x804d9c80, 4.0f / 3); scalar(0x804d9c84, 26);
        scalar(0x804d9c78, 16); scalar(0x804d9c7c, 8192);
        gameplayspawn::Spawn spawn;
        spawn.roomId = 0x02000102;
        spawn.position = {-7.640747f, 0, 145.1737f};
        s3denvironment::Camera camera;
        std::string error;
        require(gameplaycamera::initialize(data, spawn, camera, error), error.c_str());
        require(camera.target == std::array<float, 3>{spawn.position[0], 200, 40}, "Initial target from room bounds.");
        require(camera.position == std::array<float, 3>{spawn.position[0], 480, 1460}, "Initial camera eye from preset zero.");
        require(camera.aspect == 4.0f / 3 && camera.fieldOfView == 26 && camera.nearPlane == 16 && camera.farPlane == 8192,
            "Original camera projection.");
        spawn.position = {-900, 260, -100};
        require(gameplaycamera::initialize(data, spawn, camera, error), error.c_str());
        require(camera.target == std::array<float, 3>{-615, 200, -100} && camera.position[2] == 1320,
            "Left clamp, depth tracking and strict vertical threshold.");
        spawn.position = {900, 261, 40};
        require(gameplaycamera::initialize(data, spawn, camera, error), error.c_str());
        require(camera.target == std::array<float, 3>{615, 501, 40} && camera.position[1] == 781,
            "Right clamp and raised camera owner.");
        set(0x80300164, 2);
        require(!gameplaycamera::initialize(data, spawn, camera, error), "Reject unsupported door camera flags.");
        require(camera.position[1] == 781, "Failed initialization preserves prior camera.");
        if (argc == 2) {
            gameassets::Package package;
            roomscene::Scene room;
            require(gameplayspawn::load(argv[1], spawn, room, error), error.c_str());
            require(package.open(argv[1], error) && package.read("sys/main.dol", data, error), error.c_str());
            require(gameplaycamera::initialize(data, spawn, camera, error), error.c_str());
            require(spawn.rotation == std::array<std::int16_t, 3>{0, -32768, 0}, "Original Luigi spawn facing.");
            require(camera.target[1] == 200 && camera.target[2] == 40 && camera.position[1] == 480 && camera.position[2] == 1460,
                "Owned-package initial camera matches recovered calculation.");
            std::cout << "Original initial camera and Luigi spawn facing verified.\n";
        }
        return 0;
    } catch (const std::exception& exception) { std::cerr << exception.what() << '\n'; return 1; }
}
