#pragma once

#include "game/models/s3dmodel.h"

namespace s3denvironment {
    struct Curve {
        unsigned stride = 1;
        std::vector<float> keys;
        float sample(float frame) const;
    };

    struct Light {
        std::array<float, 3> position{}, direction{}, color{};
        std::array<float, 3> angular{1, 0, 0}, distance{1, 0, 0};
    };

    struct Camera {
        std::array<float, 3> position{0, 300, 500}, target{0, 300, 0}, up{0, 1, 0};
        float roll = 0, aspect = 1.18f, fieldOfView = 26, nearPlane = 16, farPlane = 8192;
    };

    class CameraAnimation {
    public:
        bool read(const std::vector<std::uint8_t>& scene, const std::vector<std::uint8_t>& settings, std::string& error);
        Camera sample(float frame) const;

    private:
        Camera base;
        std::array<Curve, 10> channels;
    };

    struct Lighting {
        std::array<float, 3> ambient{1, 1, 1};
        std::vector<Light> lights;
        std::array<float, 4> shade(const s3dmodel::Vertex& vertex, const s3dmodel::Material& material, bool enabled) const;
    };

    struct Fog {
        unsigned type = 0, rangeWidth = 0, rangeCenter = 0;
        bool rangeEnabled = false;
        float start = 0, end = 0;
        std::array<float, 4> color{};
    };

    class Environment {
    public:
        bool read(const std::vector<std::uint8_t>& scene, const std::vector<std::uint8_t>& settings, std::string& error);
        Lighting sample(float frame) const;
        Fog sampleFog(float frame) const;

    private:
        struct Source {
            unsigned flags = 0, spot = 0, attenuation = 0;
            std::array<float, 3> position{}, target{};
            std::array<int, 3> color{};
            float angle = 0, distance = 0, brightness = 0;
            std::array<Curve, 12> channels;
        };
        std::array<int, 3> ambient{255, 255, 255};
        std::array<Curve, 3> ambientChannels;
        std::vector<Source> sources;
        Fog fog;
        std::array<int, 4> fogColor{};
        std::array<Curve, 6> fogChannels;
    };

    class MaterialAnimation {
    public:
        bool read(const std::vector<std::uint8_t>& data, std::size_t materials, std::string& error);
        void apply(float frame, std::vector<s3dmodel::Material>& materials) const;

    private:
        struct Entry { unsigned material; std::array<Curve, 4> channels; };
        unsigned frames = 0;
        bool loop = false;
        std::vector<Entry> entries;
    };

    class ModelEffect {
    public:
        bool read(const std::vector<std::uint8_t>& data, const s3dmodel::Model& model, std::string& error);
        void apply(float frame, s3dmodel::Model& model) const;
        float advance(float frame, float step) const;

    private:
        struct Entry {
            unsigned material = 0;
            std::array<Curve, 4> channels;
            std::vector<unsigned> textures;
        };
        unsigned kind = 0, mode = 0, frames = 0;
        std::vector<Entry> entries;
    };
}
