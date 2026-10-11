#pragma once

#include "game/models/modelmaterialstate.h"
#include "game/world/dooranimationpose.h"
#include "game/math/originalfloattrig.h"

namespace modellighting {
    struct Light {
        std::array<int, 4> color{};
        std::array<float, 4> position{}, direction{}, angular{}, distance{};
    };

    struct Channel {
        std::array<unsigned, 4> control{}, functions{};
    };

    struct Inputs {
        std::array<Light, 8> lights{};
        std::array<Channel, 4> channels{};
        std::array<std::array<int, 4>, 2> ambient{}, material{};
    };

    struct RoomLight {
        std::array<float, 3> position{}, direction{};
        std::array<std::uint8_t, 4> color{};
        std::uint8_t cutoff = 0;
        std::uint16_t referenceDistance = 0, brightnessPercent = 0;
    };

    struct State {
        std::array<Light, 8> lights{};
        std::array<std::array<int, 4>, 2> ambient{};
        unsigned loadedMask = 0;
        modelmaterial::LightingState material;
    };

    void initialize(State& state, const dooranimation::Matrix& view, const std::vector<RoomLight>& rooms,
        bool useRoomLights, const std::array<std::uint8_t, 4>& ambient, const originalmath::FloatTrig& trig);
    void prepareModel(State& state, const dooranimation::Matrix& view, const std::array<float, 3>& origin,
        const std::array<float, 3>& target, bool spotlight, const originalmath::FloatTrig& trig);
    void specular(State& state, float shininess);
    bool configureMaterial(State& state, const modelmaterial::Material& material, std::uint8_t opacity,
        std::uint8_t lighting, std::uint8_t lightingFlags);
    Inputs prepare(const State& state);
    const char* shaderSource();
}
