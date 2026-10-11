#include "game/models/s3denvironment.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    void put(std::vector<std::uint8_t>& data, unsigned offset, unsigned value, unsigned size) {
        for (unsigned i = 0; i < size; ++i) data.at(offset + i) = static_cast<std::uint8_t>(value >> ((size - i - 1) * 8));
    }
}

int main(int argc, char** argv) {
    try {
        s3denvironment::Curve cubic{3, {0,0,0, 30,100,0}};
        require(cubic.sample(-1) == 0 && cubic.sample(31) == 100 && cubic.sample(15) == 50, "Scene cubic endpoints and midpoint.");
        s3denvironment::Curve split{4, {0,0,0,8, 30,100,0,0}};
        require(split.sample(15) == 51, "Scene tangent units and split tangents.");
        std::vector<std::uint8_t> scene(32), settings(660);
        put(settings, 0x34, 0xffff, 2);
        put(settings, 0x36, 1, 2);
        put(settings, 0x38, 64, 2);
        put(settings, 0x3a, 128, 2);
        put(settings, 0x3c, 255, 2);
        s3denvironment::Environment environment;
        std::string error;
        s3denvironment::CameraAnimation camera;
        require(camera.read(scene, settings, error), error.c_str());
        require(camera.sample(0).position == std::array<float, 3>{0,300,500}, "Disabled camera defaults.");
        put(settings, 4, 0xffff, 2);
        put(settings, 6, 1, 2);
        put(settings, 32, 0x42b40000, 4);
        put(settings, 36, 0x41400000, 4);
        put(settings, 40, 0x43480000, 4);
        require(camera.read(scene, settings, error), error.c_str());
        auto sampledCamera = camera.sample(0);
        require(sampledCamera.target[2] == 100 && sampledCamera.aspect == 10 && sampledCamera.fieldOfView == 179, "Camera coincident target and projection clamps.");
        require(sampledCamera.nearPlane == 16 && sampledCamera.farPlane == 8192, "Zero camera clip fallback.");
        require(std::abs(sampledCamera.up[0] - 1) < 0.00001f, "Camera roll direction.");
        require(!camera.read({}, settings, error) && camera.sample(0).target[2] == 100, "Camera read failure preserves state.");
        require(environment.read(scene, settings, error), error.c_str());
        s3dmodel::Vertex vertex;
        vertex.normal = {0,0,1};
        s3dmodel::Material material;
        auto lighting = environment.sample(0);
        auto shaded = lighting.shade(vertex, material, true);
        require(std::abs(shaded[0] - 64.0f / 255) < 0.00001f && shaded[2] == 1 && shaded[3] == 1, "Ambient RGB and independent alpha.");
        lighting.ambient = {0,0,0};
        s3denvironment::Light light;
        light.position = {0,0,10};
        light.color = {1,0.5f,0.25f};
        lighting.lights.push_back(light);
        shaded = lighting.shade(vertex, material, true);
        require(shaded[0] == 1 && shaded[1] == 0.5f, "Diffuse light facing normal.");
        vertex.normal = {0,0,-1};
        require(lighting.shade(vertex, material, true)[0] == 0, "Back-facing diffuse clamp.");
        require(lighting.shade(vertex, material, false)[0] == 1, "Unlit material path.");
        vertex.normal = {0,0,1};
        lighting.lights[0].distance = {1,0.1f,0};
        require(lighting.shade(vertex, material, true)[0] == 0.5f, "Distance attenuation.");
        lighting.lights[0].direction = {0,0,-1};
        lighting.lights[0].angular = {0,1,0};
        require(lighting.shade(vertex, material, true)[0] == 0, "Spotlight outside cone.");
        auto invalid = settings;
        invalid.resize(12);
        require(!environment.read(scene, invalid, error), "Silently accepted truncated environment.");
        require(environment.sample(0).ambient[2] == 1, "Failed read changed environment.");
        auto fogSettings = settings;
        put(fogSettings, 0x20c, 0xffff, 2);
        put(fogSettings, 0x20e, 1, 2);
        put(fogSettings, 0x210, 2, 4);
        put(fogSettings, 0x218, 300, 2);
        put(fogSettings, 0x21a, 0xffff, 2);
        put(fogSettings, 0x21e, 255, 2);
        put(fogSettings, 0x220, 0x42c80000, 4);
        put(fogSettings, 0x224, 0x43fa0000, 4);
        require(environment.read(scene, fogSettings, error), error.c_str());
        const auto fog = environment.sampleFog(0);
        require(fog.type == 2 && fog.start == 100 && fog.end == 500 && fog.color[0] == 1 && fog.color[1] == 0, "Fog values and signed color clamps.");
        s3dmodel::Model effectModel;
        effectModel.materials.resize(1);
        effectModel.samplers = {{1,0,1,0},{2,2,0,2}};
        std::vector<std::uint8_t> txp(24);
        put(txp, 0, 1, 2);
        put(txp, 2, 2, 2);
        put(txp, 4, 1, 2);
        put(txp, 6, 2, 2);
        put(txp, 8, 20, 4);
        put(txp, 22, 1, 2);
        s3denvironment::ModelEffect effect;
        require(effect.read(txp, effectModel, error), error.c_str());
        effect.apply(0.9f, effectModel);
        require(effectModel.materials[0].texture == 1 && effectModel.materials[0].wrapT == 1, "Texture effect truncates frame and resolves sampler.");
        effect.apply(1, effectModel);
        require(effectModel.materials[0].texture == 2 && effectModel.materials[0].mode == 2 && effect.advance(1.5f, 0.5f) == 0, "Texture effect mode and loop.");
        put(txp, 14, 1, 2);
        require(!effect.read(txp, effectModel, error), "Unsupported texture stage silently accepted.");
        std::vector<std::uint8_t> clk(58);
        put(clk, 0, 1, 2);
        put(clk, 4, 1, 2);
        put(clk, 6, 30, 2);
        put(clk, 8, 44, 4);
        for (unsigned channel = 0; channel < 4; ++channel) {
            put(clk, 16 + channel * 6, channel == 3 ? 2 : 1, 2);
            put(clk, 18 + channel * 6, channel == 3 ? 1 : 0, 2);
            put(clk, 20 + channel * 6, channel == 3 ? 3 : 1, 2);
        }
        put(clk, 44, 255, 2);
        put(clk, 52, 30, 2);
        put(clk, 54, 255, 2);
        s3denvironment::MaterialAnimation animation;
        require(animation.read(clk, 1, error), error.c_str());
        std::vector<s3dmodel::Material> materials(1);
        animation.apply(15, materials);
        require(materials[0].color[0] == 1 && materials[0].color[3] == 127.0f / 255, "CLK truncates integer channels.");
        animation.apply(30, materials);
        require(materials[0].color[3] == 0, "CLK loop at duration.");
        put(clk, 2, 1, 2);
        require(animation.read(clk, 1, error), error.c_str());
        animation.apply(31, materials);
        const auto held = materials[0].color[3];
        animation.apply(29, materials);
        require(materials[0].color[3] == held, "CLK non-looping last-frame hold.");
        clk.resize(54);
        require(!animation.read(clk, 1, error), "Accepted truncated CLK channel.");
        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> data, archive;
            require(package.open(argv[1], error), error.c_str());
            require(package.read("files/Kawano/ENGLISH/res_slct.szp", data, error), error.c_str());
            require(roomarchive::decompress(data, archive, error), error.c_str());
            for (const auto* name : {"select", "optdemo1", "optdemo2"}) {
                require(roomarchive::read(archive, std::string(name) + ".scd", scene, error), error.c_str());
                require(roomarchive::read(archive, std::string(name) + ".sco", settings, error), error.c_str());
                if (!environment.read(scene, settings, error)) throw std::runtime_error(error);
                for (float frame = 0; frame < 600; frame += 0.5f) {
                    lighting = environment.sample(frame);
                    for (const auto& source : lighting.lights) {
                        for (float value : source.color) require(std::isfinite(value) && value >= 0 && value <= 1, "Invalid sampled light color.");
                        for (float value : source.position) require(std::isfinite(value), "Invalid sampled light position.");
                    }
                }
                std::cout << name << ": " << lighting.lights.size() << " lights\n";
            }
            require(roomarchive::read(archive, "k_select.clk", data, error), error.c_str());
            if (!animation.read(data, 2, error)) throw std::runtime_error(error);
            materials.resize(2);
            for (float frame = 0; frame < 1201; frame += 0.5f) animation.apply(frame, materials);
        }
        std::cout << "S3D environment checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
