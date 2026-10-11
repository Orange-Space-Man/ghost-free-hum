#include "game/models/modellighting.h"

#include <cmath>
#include <stdexcept>

namespace {
    std::array<float, 4> transform(const dooranimation::Matrix& matrix, const std::array<float, 3>& value, bool point) {
        std::array<float, 4> result{};
        for (unsigned row = 0; row < 3; ++row)
            result[row] = matrix[row * 4] * value[0] + matrix[row * 4 + 1] * value[1] +
                matrix[row * 4 + 2] * value[2] + (point ? matrix[row * 4 + 3] : 0);
        return result;
    }

    void distance(modellighting::Light& light, float reference, float brightness, bool quadratic) {
        light.distance = {1, 0, 0, 0};
        if (reference < 0 || brightness <= 0 || brightness >= 1) return;
        if (quadratic) light.distance[2] = (1 - brightness) / (reference * (brightness * reference));
        else light.distance[1] = (1 - brightness) / (brightness * reference);
    }

    void spot(modellighting::Light& light, float cutoff, bool flat, const originalmath::FloatTrig& trig) {
        light.angular = {1, 0, 0, 0};
        if (cutoff <= 0 || cutoff > 90) return;
        const auto cosine = trig.sinCos((3.14159274101257324219f * cutoff) / 180.0f)[1];
        if (flat) light.angular = {-1000 * cosine, 1000, 0, 0};
        else light.angular = {0, -cosine / (1 - cosine), 1 / (1 - cosine), 0};
    }

    modellighting::Light point(const dooranimation::Matrix& view, const std::array<float, 3>& position) {
        modellighting::Light result;
        result.position = transform(view, position, true);
        result.direction = {0, 0, -1, 0};
        result.color = {255, 255, 255, 255};
        result.angular = result.distance = {1, 0, 0, 0};
        return result;
    }
}

void modellighting::initialize(State& state, const dooranimation::Matrix& view, const std::vector<RoomLight>& rooms,
    bool useRoomLights, const std::array<std::uint8_t, 4>& ambient, const originalmath::FloatTrig& trig) {
    if (useRoomLights && rooms.size() > 8) throw std::runtime_error("Original light slots exceeded.");
    State result = state;
    result.material.lightMask = 0;
    result.material.attenuation = useRoomLights ? 1 : 2;
    result.material.lightingFlags = 0;
    for (unsigned channel = 0; channel < 4; ++channel) result.ambient[0][channel] = ambient[channel];
    if (!useRoomLights) {
        result.lights[0] = point(view, {0, 10000, 10000});
        result.loadedMask |= 1;
        result.material.lightMask = 1;
    } else {
        for (unsigned i = 0; i < rooms.size(); ++i) {
            const auto& source = rooms[i];
            auto light = point(view, source.position);
            for (unsigned component = 0; component < 4; ++component) light.color[component] = source.color[component];
            if (source.cutoff) {
                light.direction = transform(view, source.direction, false);
                for (unsigned component = 0; component < 3; ++component) light.direction[component] = -light.direction[component];
                spot(light, source.cutoff, false, trig);
            } else light.direction = {0, 0, 1, 0};
            distance(light, source.referenceDistance, static_cast<float>(0.01 * source.brightnessPercent), source.cutoff == 0);
            result.lights[i] = light;
            result.loadedMask |= 1U << i;
            result.material.lightMask += 1U << i;
        }
    }
    state = result;
}

void modellighting::specular(State& state, float shininess) {
    if (!(state.loadedMask & 0x40)) throw std::runtime_error("Specular light direction has not been initialized.");
    auto& light = state.lights[6];
    light.color = {255, 255, 255, 255};
    light.angular = {0, 0, 1, 0};
    light.distance = {shininess * 0.5f, 0, 1 - shininess * 0.5f, 0};
}

void modellighting::prepareModel(State& state, const dooranimation::Matrix& view, const std::array<float, 3>& origin,
    const std::array<float, 3>& target, bool spotlight, const originalmath::FloatTrig& trig) {
    auto result = state;
    auto& highlight = result.lights[6];
    highlight.position = {0, 100, 0, 0};
    const float inverse = 1 / std::sqrt(2.0f);
    highlight.direction = {0, inverse, inverse, 0};
    result.loadedMask |= 0x40;
    specular(result, 64);
    result.material.specularMask = 0x40;
    result.material.specularAttenuation = 0;
    if (spotlight) {
        result.lights[7] = point(view, origin);
        std::array<float, 3> direction{};
        float length = 0;
        for (unsigned i = 0; i < 3; ++i) { direction[i] = target[i] - origin[i]; length += direction[i] * direction[i]; }
        length = std::sqrt(length);
        for (auto& component : direction) component /= length;
        result.lights[7].direction = transform(view, direction, false);
        for (unsigned i = 0; i < 3; ++i) result.lights[7].direction[i] = -result.lights[7].direction[i];
        spot(result.lights[7], 30, true, trig);
        result.ambient[1] = {0, 0, 0, 0};
    } else {
        std::array<float, 3> position{};
        for (unsigned i = 0; i < 3; ++i) position[i] = -(origin[i] - target[i]) + target[i];
        result.lights[7] = point(view, position);
        result.ambient[1] = {0, 0, 0, 255};
    }
    result.loadedMask |= 0x80;
    result.material.secondaryColor = {255, 255, 255, 255};
    state = result;
}

modellighting::Inputs modellighting::prepare(const State& state) {
    Inputs result;
    result.lights = state.lights;
    result.ambient = state.ambient;
    for (unsigned i = 0; i < 4; ++i) {
        const auto& source = state.material.channels[i];
        if (source.enabled > 1 || source.ambientSource > 1 || source.materialSource > 1 || source.lightMask > 255 ||
            source.diffuse > 2 || source.attenuation > 2) throw std::runtime_error("Invalid original lighting channel.");
        if (source.enabled && (source.lightMask & ~state.loadedMask)) throw std::runtime_error("Lighting channel references an unloaded light.");
        result.channels[i].control = {source.enabled, source.ambientSource, source.materialSource, source.lightMask};
        result.channels[i].functions = {source.attenuation == 0 ? 0 : source.diffuse, source.attenuation, 0, 0};
        result.material[0][i] = state.material.color[i];
        result.material[1][i] = state.material.secondaryColor[i];
    }
    return result;
}

bool modellighting::configureMaterial(State& state, const modelmaterial::Material& material, std::uint8_t opacity,
    std::uint8_t lighting, std::uint8_t lightingFlags) {
    auto result = state;
    result.material.lightingFlags = lightingFlags;
    const bool appendSpecular = modelmaterial::configureChannels(material, opacity, lighting, result.material,
        [&](float shininess) { specular(result, shininess); });
    state = result;
    return appendSpecular;
}

const char* modellighting::shaderSource() {
    return R"(
struct Light { int4 color; float4 position; float4 direction; float4 angular; float4 distance; };
struct LightChannel { uint4 control; uint4 functions; };
cbuffer Lighting : register(b3) {
    Light lights[8]; LightChannel channels[4]; int4 ambient[2]; int4 material[2];
};
int4 illuminate(uint channel, float3 position, float3 normal, int4 vertex) {
    LightChannel setup = channels[channel];
    int4 surface = setup.control.z != 0 ? vertex : material[channel % 2];
    int4 total = 255;
    if (setup.control.x != 0) {
        total = setup.control.y != 0 ? vertex : ambient[channel % 2];
        for (uint index = 0; index < 8; ++index) {
            if ((setup.control.w & (1U << index)) == 0) continue;
            Light light = lights[index];
            float3 delta = light.position.xyz - position;
            float distanceSquared = dot(delta, delta);
            float distanceValue = sqrt(distanceSquared);
            float3 direction = distanceValue == 0 ? normal : delta / distanceValue;
            float attenuation = 1;
            if (setup.functions.y == 0) {
                float cosine = dot(normal, direction) >= 0 ? max(0, dot(normal, light.direction.xyz)) : 0;
                float3 terms = float3(1, cosine, cosine * cosine);
                attenuation = max(0, dot(light.angular.xyz, terms)) / dot(light.distance.xyz, terms);
            } else if (setup.functions.y == 1) {
                float cosine = max(0, dot(direction, light.direction.xyz));
                attenuation = max(0, dot(light.angular.xyz, float3(1, cosine, cosine * cosine))) /
                    dot(light.distance.xyz, float3(1, distanceValue, distanceSquared));
            }
            float diffuse = setup.functions.x == 0 ? 1 : dot(normal, direction);
            if (setup.functions.x == 2) diffuse = max(0, diffuse);
            total += int4(round(attenuation * diffuse * float4(light.color)));
        }
    }
    total = clamp(total, 0, 255);
    return (surface * (total + (total >> 7))) >> 8;
}
)";
}
