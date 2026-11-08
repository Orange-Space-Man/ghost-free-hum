#include "game/effects/modeleffectlighting.h"
#include "resources/dolresources.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    constexpr unsigned effectSpecularColor = 0x804d7278;
    constexpr unsigned effectShininess = 0x804d727c;
    constexpr unsigned effectDirection = 0x804dccf4;
    constexpr unsigned specularLightDistance = 0x804dd7c0;
    constexpr unsigned fixedLightPosition = 0x8020c660;
    constexpr unsigned fixedLightDirection = 0x8020c684;
    constexpr unsigned fixedLightColor = 0x804db454;
    constexpr unsigned effectAmbientColors = 0x804db44c;

    std::uint32_t word(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }

    float number(const std::vector<std::uint8_t>& executable, unsigned address) {
        const auto bits = word(executable, address);
        float result;
        std::memcpy(&result, &bits, 4);
        return result;
    }

    std::array<std::uint8_t, 4> color(std::uint32_t value) {
        return {static_cast<std::uint8_t>(value >> 24), static_cast<std::uint8_t>(value >> 16),
            static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value)};
    }
}

modeleffects::Lighting::Lighting(const std::vector<std::uint8_t>& executable) {
    for (unsigned i = 0; i < 3; ++i) {
        fixed.position[i] = number(executable, fixedLightPosition + i * 16);
        fixed.direction[i] = number(executable, fixedLightDirection + i * 16);
    }
    fixed.color = color(word(executable, fixedLightColor));
    fixed.attenuation = {1, 0, 0, 1, 0, 0};
    for (unsigned i = 0; i < 2; ++i) ambient[i] = color(word(executable, effectAmbientColors + i * 4));
    specular.color = color(word(executable, effectSpecularColor));
    specular.color[3] = 255;
    const float shininess = number(executable, effectShininess) * 0.5f;
    specular.attenuation = {0, 0, 1, shininess, 0, 1 - shininess};
    direction = {number(executable, effectDirection), number(executable, effectDirection), 0};
    specularDistance = number(executable, specularLightDistance);
}

unsigned modeleffects::Lighting::prepare(j3dmodel::MaterialState& state, const j3dmodel::Matrix& view,
    const std::vector<modellighting::RoomLight>& lights, const originalmath::FloatTrig& trig) const {
    for (unsigned i = 0; i < 2; ++i) state.ambientColor(i, ambient[i]);
    state.light(7, fixed);
    auto highlight = specular;
    std::array<float, 3> transformed{};
    for (unsigned row = 0; row < 3; ++row) {
        transformed[row] = view[row * 4] * direction[0] + view[row * 4 + 1] * direction[1] + view[row * 4 + 2] * direction[2];
        highlight.position[row] = -transformed[row] * specularDistance;
    }
    highlight.direction = {-transformed[0], -transformed[1], 1 - transformed[2]};
    const auto& half = highlight.direction;
    const float length = std::sqrt((half[2] * half[2] + half[0] * half[0]) + half[1] * half[1]);
    for (auto& value : highlight.direction) value *= 1 / length;
    state.lights[6] = highlight;
    modellighting::State room;
    modellighting::initialize(room, view, lights, true, {}, trig);
    for (unsigned i = 0; i < lights.size(); ++i) {
        const auto& source = room.lights[i];
        j3dmodel::MaterialLight light;
        std::copy(source.color.begin(), source.color.end(), light.color.begin());
        std::copy_n(source.position.begin(), 3, light.position.begin());
        std::copy_n(source.direction.begin(), 3, light.direction.begin());
        std::copy_n(source.angular.begin(), 3, light.attenuation.begin());
        std::copy_n(source.distance.begin(), 3, light.attenuation.begin() + 3);
        state.lights[i] = light;
    }
    return room.material.lightMask;
}
