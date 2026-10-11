#include "game/models/s3denvironment.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        require(offset <= data.size() && data.size() - offset >= 2, "Truncated S3D environment resource.");
        return (data[offset] << 8) | data[offset + 1];
    }

    int signedWord(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return static_cast<std::int16_t>(word(data, offset));
    }

    unsigned dword(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (word(data, offset) << 16) | word(data, offset + 2);
    }

    float number(const std::vector<std::uint8_t>& data, std::size_t offset) {
        const auto bits = dword(data, offset);
        float value;
        std::memcpy(&value, &bits, 4);
        require(std::isfinite(value), "Non-finite S3D environment value.");
        return value;
    }

    s3denvironment::Curve curve(const std::vector<std::uint8_t>& data, std::size_t table, std::size_t descriptor, bool integer) {
        s3denvironment::Curve result;
        const auto count = word(data, descriptor), index = word(data, descriptor + 2);
        result.stride = word(data, descriptor + 4);
        require(result.stride == 1 || result.stride == 3 || result.stride == 4, "Unsupported S3D environment key stride.");
        require(result.stride == 1 || count > 0, "Empty S3D environment curve.");
        const unsigned elements = result.stride == 1 ? 1 : count * result.stride;
        for (unsigned i = 0; i < elements; ++i) {
            const auto offset = table + std::size_t(index + i) * (integer ? 2 : 4);
            result.keys.push_back(integer ? float(signedWord(data, offset)) : number(data, offset));
        }
        if (result.stride != 1) for (unsigned i = 1; i < count; ++i)
            require(result.keys[i * result.stride] > result.keys[(i - 1) * result.stride], "Unordered S3D environment keys.");
        return result;
    }

    std::array<float, 3> normalize(std::array<float, 3> value) {
        const float length = std::sqrt(value[0]*value[0] + value[1]*value[1] + value[2]*value[2]);
        if (length == 0) return {0, 0, 1};
        for (auto& component : value) component /= length;
        return value;
    }

    float dot(const std::array<float, 3>& a, const std::array<float, 3>& b) {
        return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
    }

    int colorSum(int base, float animation) {
        return std::clamp(int(static_cast<std::int16_t>(base + static_cast<int>(animation))), 0, 255);
    }
}

float s3denvironment::Curve::sample(float frame) const {
    if (keys.empty()) return 0;
    if (stride == 1) return keys[0];
    const auto last = keys.size() - stride;
    if (frame <= keys[0]) return keys[1];
    if (frame >= keys[last]) return keys[last + 1];
    std::size_t index = 0;
    while (frame > keys[index + stride]) index += stride;
    const auto next = index + stride;
    const float time = frame / 30.0f - keys[index] / 30.0f;
    const float inverse = 1.0f / (keys[next] / 30.0f - keys[index] / 30.0f);
    const float twiceSquared = 2.0f * time * time;
    const float threeSquared = inverse * inverse * 3.0f * time * time;
    const float cubic = inverse * inverse * time * time * time;
    return (cubic - inverse * time * time) * keys[next + 2] +
        (time + (cubic - inverse * twiceSquared)) * keys[index + (stride == 4 ? 3 : 2)] +
        (1.0f + (inverse * inverse * inverse * time * twiceSquared - threeSquared)) * keys[index + 1] +
        (inverse * inverse * inverse * time * -2.0f * time * time + threeSquared) * keys[next + 1];
}

bool s3denvironment::CameraAnimation::read(const std::vector<std::uint8_t>& scene, const std::vector<std::uint8_t>& settings, std::string& error) {
    error.clear();
    try {
        require(scene.size() >= 32 && settings.size() >= 0x34, "Truncated S3D camera resource.");
        CameraAnimation result;
        if (word(settings, 6) & 1) {
            for (unsigned axis = 0; axis < 3; ++axis) {
                result.base.position[axis] = number(settings, 8 + axis * 4);
                result.base.target[axis] = number(settings, 20 + axis * 4);
            }
            result.base.roll = number(settings, 32);
            result.base.aspect = number(settings, 36);
            result.base.fieldOfView = number(settings, 40);
            result.base.nearPlane = number(settings, 44);
            result.base.farPlane = number(settings, 48);
            const auto record = dword(scene, 8);
            if (signedWord(settings, 4) != -1 && record != 0) {
                result.base.aspect += number(scene, record);
                for (unsigned channel = 0; channel < 10; ++channel)
                    result.channels[channel] = curve(scene, dword(scene, 24), record + 4 + channel * 6, false);
            }
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

s3denvironment::Camera s3denvironment::CameraAnimation::sample(float frame) const {
    require(std::isfinite(frame), "Invalid S3D camera frame.");
    auto result = base;
    for (unsigned axis = 0; axis < 3; ++axis) {
        result.position[axis] += channels[axis].sample(frame);
        result.target[axis] += channels[axis + 3].sample(frame);
    }
    if (result.position == result.target) result.target[2] += 100;
    std::array<float, 3> direction{};
    for (unsigned axis = 0; axis < 3; ++axis) direction[axis] = result.position[axis] - result.target[axis];
    if (direction[0] == 0 && direction[2] == 0) direction = {1, 0, 0};
    direction = normalize(direction);
    const auto cross = [](const std::array<float, 3>& a, const std::array<float, 3>& b) {
        return std::array<float, 3>{a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0]};
    };
    const auto up = cross(direction, cross({0, 1, 0}, direction));
    const auto perpendicular = cross(direction, up);
    result.roll += channels[6].sample(frame);
    const float radians = result.roll * 0.017453292f;
    for (unsigned axis = 0; axis < 3; ++axis)
        result.up[axis] = up[axis] * std::cos(radians) + perpendicular[axis] * std::sin(radians);
    result.up = normalize(result.up);
    result.aspect = std::clamp(result.aspect, 0.1f, 10.0f);
    result.fieldOfView = std::clamp(base.fieldOfView + channels[7].sample(frame), 1.0f, 179.0f);
    result.nearPlane += channels[8].sample(frame);
    result.farPlane += channels[9].sample(frame);
    if (result.nearPlane == 0) result.nearPlane = 16;
    if (result.farPlane == 0) result.farPlane = 8192;
    return result;
}
bool s3denvironment::Environment::read(const std::vector<std::uint8_t>& scene, const std::vector<std::uint8_t>& settings, std::string& error) {
    error.clear();
    try {
        require(scene.size() >= 32 && settings.size() >= 0x22c, "Truncated S3D scene environment.");
        Environment result;
        if (word(settings, 0x20e) & 1) {
            result.fog.type = dword(settings, 0x210);
            result.fog.rangeEnabled = settings.at(0x214) != 0;
            result.fog.rangeWidth = word(settings, 0x228);
            result.fog.rangeCenter = word(settings, 0x22a);
            result.fog.start = number(settings, 0x220);
            result.fog.end = number(settings, 0x224);
            for (unsigned channel = 0; channel < 4; ++channel) result.fogColor[channel] = signedWord(settings, 0x218 + channel * 2);
            require(result.fogColor[3] >= 0, "Indeterminate original fog alpha.");
            const auto record = dword(scene, 20);
            if (signedWord(settings, 0x20c) != -1 && record != 0) {
                for (unsigned channel = 0; channel < 6; ++channel)
                    result.fogChannels[channel] = curve(scene, dword(scene, channel < 4 ? 28 : 24), record + channel * 6, channel < 4);
            }
        }
        result.ambient.fill(0);
        if (word(settings, 0x36) & 1) {
            const int index = signedWord(settings, 0x34);
            for (unsigned axis = 0; axis < 3; ++axis) {
                result.ambient[axis] = signedWord(settings, 0x38 + axis * 2);
                if (index >= 0 && unsigned(index) < word(scene, 2)) result.ambientChannels[axis] =
                    curve(scene, dword(scene, 28), dword(scene, 12) + index * 36 + 16 + axis * 6, true);
            }
        }
        require(!(word(settings, 0x36) & 2), "S3D vertex ambient color is not implemented.");
        for (unsigned i = 0; i < 8; ++i) {
            const std::size_t offset = 0x4c + i * 56;
            Source source;
            source.flags = word(settings, offset + 2);
            if (!(source.flags & 1)) continue;
            require(!(source.flags & 8), "S3D specular light is not implemented.");
            source.spot = dword(settings, offset + 4);
            source.attenuation = dword(settings, offset + 8);
            require(source.spot == 0 || source.spot == 2, "Unsupported S3D spotlight function.");
            require(source.attenuation <= 3, "Unsupported S3D distance attenuation.");
            for (unsigned axis = 0; axis < 3; ++axis) {
                source.position[axis] = number(settings, offset + 12 + axis * 4);
                source.target[axis] = number(settings, offset + 24 + axis * 4);
                source.color[axis] = signedWord(settings, offset + 36 + axis * 2);
            }
            source.angle = number(settings, offset + 44);
            source.distance = number(settings, offset + 48);
            source.brightness = number(settings, offset + 52);
            const int index = signedWord(settings, offset);
            if (index >= 0 && unsigned(index) < word(scene, 4)) {
                const std::size_t record = dword(scene, 16) + index * 104;
                const unsigned offsets[]{20,26,32,38,44,50,56,68,74,80,86,92};
                for (unsigned channel = 0; channel < 12; ++channel)
                    source.channels[channel] = curve(scene, dword(scene, channel < 9 ? 24 : 28), record + offsets[channel], channel >= 9);
            }
            result.sources.push_back(std::move(source));
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

s3denvironment::Fog s3denvironment::Environment::sampleFog(float frame) const {
    require(std::isfinite(frame), "Invalid S3D fog frame.");
    auto result = fog;
    if (result.type == 0) return result;
    for (unsigned channel = 0; channel < 4; ++channel)
        result.color[channel] = colorSum(fogColor[channel], fogChannels[channel].sample(frame)) / 255.0f;
    result.start += fogChannels[4].sample(frame);
    result.end += fogChannels[5].sample(frame);
    return result;
}

s3denvironment::Lighting s3denvironment::Environment::sample(float frame) const {
    require(std::isfinite(frame), "Invalid S3D environment frame.");
    Lighting result;
    for (unsigned axis = 0; axis < 3; ++axis) result.ambient[axis] = colorSum(ambient[axis], ambientChannels[axis].sample(frame)) / 255.0f;
    for (const auto& source : sources) {
        Light light;
        for (unsigned axis = 0; axis < 3; ++axis) {
            light.position[axis] = source.position[axis] + source.channels[axis].sample(frame);
            light.direction[axis] = light.position[axis] - (source.target[axis] + source.channels[axis + 3].sample(frame));
            light.color[axis] = colorSum(source.color[axis], source.channels[axis + 9].sample(frame)) / 255.0f;
        }
        light.direction = normalize(light.direction);
        if ((source.flags & 4) && source.spot == 2) {
            const float angle = std::clamp(source.angle + 0.5f * source.channels[6].sample(frame), 0.000001f, 90.0f);
            const float cosine = std::cos(angle * 3.141592654f / 180.0f);
            require(cosine < 1, "S3D spotlight cutoff is too small.");
            light.angular = {-cosine / (1 - cosine), 1 / (1 - cosine), 0};
        }
        if (source.flags & 2) {
            const float distance = std::max(1.0f, source.distance + 0.5f * (source.channels[7].sample(frame) + source.channels[8].sample(frame)));
            const float brightness = std::clamp(0.5f + source.brightness, 0.000001f, 0.999999f);
            const float linear = (1 - brightness) / (brightness * distance);
            const float quadratic = (1 - brightness) / (distance * brightness * distance);
            if (source.attenuation == 1) light.distance[1] = linear;
            if (source.attenuation == 2) { light.distance[1] = linear * 0.5f; light.distance[2] = quadratic * 0.5f; }
            if (source.attenuation == 3) light.distance[2] = quadratic;
        }
        result.lights.push_back(light);
    }
    return result;
}

std::array<float, 4> s3denvironment::Lighting::shade(const s3dmodel::Vertex& vertex, const s3dmodel::Material& material, bool enabled) const {
    auto color = material.vertexColor ? vertex.color : material.color;
    color[3] = material.vertexAlpha ? vertex.color[3] : material.color[3];
    if (!enabled) return color;
    auto illumination = ambient;
    for (const auto& light : lights) {
        std::array<float, 3> delta{};
        for (unsigned axis = 0; axis < 3; ++axis) delta[axis] = light.position[axis] - vertex.position[axis];
        const float distanceSquared = dot(delta, delta), distance = std::sqrt(distanceSquared);
        const auto direction = normalize(delta);
        const float cosine = dot(direction, light.direction);
        const float angular = std::max(0.0f, light.angular[0] + light.angular[1] * cosine + light.angular[2] * cosine * cosine);
        const float attenuation = angular / (light.distance[0] + light.distance[1] * distance + light.distance[2] * distanceSquared);
        const float diffuse = std::max(0.0f, dot(direction, vertex.normal));
        for (unsigned axis = 0; axis < 3; ++axis) illumination[axis] += light.color[axis] * diffuse * attenuation;
    }
    for (unsigned axis = 0; axis < 3; ++axis) color[axis] *= std::clamp(illumination[axis], 0.0f, 1.0f);
    return color;
}

bool s3denvironment::MaterialAnimation::read(const std::vector<std::uint8_t>& data, std::size_t materials, std::string& error) {
    error.clear();
    try {
        require(word(data, 0) == 1, "Unsupported S3D material animation.");
        MaterialAnimation result;
        result.frames = word(data, 6);
        result.loop = word(data, 2) == 0;
        require(result.frames > 0, "Empty S3D material animation.");
        for (unsigned i = 0; i < word(data, 4); ++i) {
            const std::size_t record = 12 + i * 32;
            Entry entry;
            entry.material = word(data, record + 2);
            require(entry.material < materials, "Invalid animated S3D material index.");
            for (unsigned channel = 0; channel < 4; ++channel) entry.channels[channel] = curve(data, dword(data, 8), record + 4 + channel * 6, true);
            result.entries.push_back(std::move(entry));
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

void s3denvironment::MaterialAnimation::apply(float frame, std::vector<s3dmodel::Material>& materials) const {
    require(std::isfinite(frame) && frame >= 0, "Invalid S3D material animation frame.");
    if (!frames) return;
    frame = loop ? std::fmod(frame, float(frames)) : std::min(frame, float(frames - 1));
    for (const auto& entry : entries) {
        auto& material = materials.at(entry.material);
        for (unsigned channel = 0; channel < 4; ++channel) material.color[channel] =
            static_cast<std::uint8_t>(static_cast<int>(entry.channels[channel].sample(frame))) / 255.0f;
    }
}

bool s3denvironment::ModelEffect::read(const std::vector<std::uint8_t>& data, const s3dmodel::Model& model, std::string& error) {
    error.clear();
    try {
        ModelEffect result;
        result.kind = word(data, 0);
        result.mode = word(data, 2);
        result.frames = word(data, 6);
        require((result.kind == 1 || result.kind == 2) && (result.frames > 0 || word(data, 4) == 0), "Unsupported S3D model effect.");
        const auto table = dword(data, 8);
        for (unsigned i = 0; i < word(data, 4); ++i) {
            const std::size_t record = 12 + i * (result.kind == 1 ? 8 : 28);
            Entry entry;
            entry.material = word(data, record + (result.kind == 1 ? 0 : 2));
            require(entry.material < model.materials.size(), "Invalid S3D effect material.");
            if (result.kind == 1) {
                require(word(data, record + 2) == 0, "Additional S3D texture stages are not implemented.");
                const auto first = dword(data, record + 4);
                for (unsigned frame = 0; frame < result.frames; ++frame) {
                    const auto texture = word(data, std::size_t(table) + (std::size_t(first) + frame) * 2);
                    require(texture < model.samplers.size(), "Invalid S3D effect sampler.");
                    entry.textures.push_back(texture);
                }
            } else {
                for (unsigned channel = 0; channel < 4; ++channel)
                    entry.channels[channel] = curve(data, table, record + 4 + channel * 6, true);
            }
            result.entries.push_back(std::move(entry));
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

void s3denvironment::ModelEffect::apply(float frame, s3dmodel::Model& model) const {
    require(std::isfinite(frame), "Invalid S3D effect frame.");
    for (const auto& entry : entries) {
        auto& material = model.materials.at(entry.material);
        if (kind == 1) {
            const auto& sampler = model.samplers.at(entry.textures.at(unsigned(std::clamp(frame, 0.0f, float(frames - 1)))));
            material.texture = sampler.texture;
            material.wrapS = sampler.wrapS;
            material.wrapT = sampler.wrapT;
            material.mode = (sampler.mode & 4) ? material.sourceMode : std::max(material.sourceMode, sampler.mode & 3);
        }
        else for (unsigned channel = 0; channel < 4; ++channel)
            material.color[channel] = static_cast<std::uint8_t>(static_cast<int>(entry.channels[channel].sample(frame))) / 255.0f;
    }
}

float s3denvironment::ModelEffect::advance(float frame, float step) const {
    require(std::isfinite(frame) && std::isfinite(step), "Invalid S3D effect playback time.");
    frame += step;
    if (frame >= frames) {
        if (mode == 2) frame -= frames;
        else if (mode == 1) frame = float(frames);
    }
    return frame;
}
