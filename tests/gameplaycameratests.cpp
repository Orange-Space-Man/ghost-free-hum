#include "game/scene/gameplaycamera.h"
#include "game/scene/gameplaycameraprojection.h"
#include "resources/gameassetpackage.h"
#include "resources/glme01addresses.h"

#include <cstring>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
    void word(std::vector<std::uint8_t>& data, unsigned offset, unsigned value) {
        for (unsigned i = 0; i < 4; ++i) data[offset + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }

    void projectionChecks(const std::vector<std::uint8_t>& data) {
        const actormodel::InitialPose pose(data);
        const gameplaycamera::Projection projection(data);
        gameplaycamera::CameraOwner owner;
        owner.position = {10, 20, 30};
        owner.target = {10, 20, 29};
        owner.nearPlane = 16;
        owner.farPlane = 1024;
        const auto frame = projection.publish(owner, pose);
        require(frame.worldProjection == owner.matrix && frame.camera.target == owner.target,
            "Camera publication must retain the original owner and unshaken aim.");
        const auto project = [&](const std::array<float, 3>& point) {
            std::array<float, 4> clip;
            for (unsigned row = 0; row < 4; ++row) clip[row] = owner.matrix[row * 4] * point[0] +
                owner.matrix[row * 4 + 1] * point[1] + owner.matrix[row * 4 + 2] * point[2] + owner.matrix[row * 4 + 3];
            for (unsigned i = 0; i < 3; ++i) clip[i] /= clip[3];
            return clip;
        };
        const auto nearPoint = project({10, 20, 14}), farPoint = project({10, 20, -994});
        require(std::abs(nearPoint[0]) < 0.00001f && std::abs(nearPoint[1]) < 0.00001f &&
            std::abs(nearPoint[2] + 1) < 0.00001f && std::abs(farPoint[2]) < 0.00001f,
            "GX camera depth must map the near plane to minus one and the far plane to zero.");
        require(project({10, 20, 31})[3] < 0 && project({11, 20, 14})[0] > 0,
            "Translated view must retain front/back and screen-right orientation.");
        owner.flags = 2;
        owner.orthographic = {8, -8, -4, 12};
        projection.publish(owner, pose);
        require(std::abs(project({6, 20, 14})[0] + 1) < 0.00001f &&
            std::abs(project({22, 28, 14})[0] - 1) < 0.00001f &&
            std::abs(project({22, 28, 14})[1] - 1) < 0.00001f,
            "Camera flag two selects the original asymmetric perspective frustum, not orthographic projection.");
        owner.flags = 0;
        owner.fieldA8 = 103;
        owner.fieldA4 = 0;
        const auto shaken = projection.publish(owner, pose);
        require(owner.fieldA8 == 100 && owner.fieldA4 == 0x2000 && shaken.camera.target != owner.target,
            "Camera shake must decay before its pitch-table lookup without changing the retained target.");
        owner.fieldA8 = 3;
        projection.publish(owner, pose);
        require(owner.fieldA8 == 0 && owner.fieldA4 == 0x4000,
            "Exactly zero shake amplitude retains the phase until the next negative decay.");
        projection.publish(owner, pose);
        require(owner.fieldA8 == 0 && owner.fieldA4 == 0,
            "Negative shake decay resets both amplitude and phase.");
        owner.fieldA8 = 100;
        owner.fieldA4 = 0xe000;
        const auto wrapped = projection.publish(owner, pose);
        require(owner.fieldA8 == 97 && owner.fieldA4 == 0 && wrapped.camera.target == owner.target,
            "Wrapped shake phase zero bypasses spherical reconstruction even with nonzero amplitude.");
    }
}

int main(int argc, char** argv) {
    try {
        std::vector<std::uint8_t> data(0x300100);
        word(data, 0, 0x100);
        word(data, 0x48, 0x80200000);
        word(data, 0x90, 0x300000);
        const auto set = [&](unsigned address, unsigned value) { word(data, address - 0x80200000 + 0x100, value); };
        const auto scalar = [&](unsigned address, float value) {
            unsigned bits;
            std::memcpy(&bits, &value, 4);
            set(address, bits);
        };
        for (unsigned i = 0; i < 4096; ++i) {
            scalar(glme01::sineTable + i * 4, float(std::sin(i * 6.283185307179586 / 4096)));
            scalar(glme01::cosineTable + i * 4, float(std::cos(i * 6.283185307179586 / 4096)));
        }
        scalar(0x804d9c88, 3);
        scalar(0x804dd708, 0.5f);
        scalar(0x804dd70c, 0.017453292f);
        projectionChecks(data);
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
            projectionChecks(data);
            require(gameplaycamera::initialize(data, spawn, camera, error), error.c_str());
            require(spawn.rotation == std::array<std::int16_t, 3>{0, -32768, 0}, "Original Luigi spawn facing.");
            require(camera.target[1] == 200 && camera.target[2] == 40 && camera.position[1] == 480 && camera.position[2] == 1460,
                "Owned-package initial camera matches recovered calculation.");
            std::cout << "Original initial camera and Luigi spawn facing verified.\n";
        }
        return 0;
    } catch (const std::exception& exception) { std::cerr << exception.what() << '\n'; return 1; }
}
