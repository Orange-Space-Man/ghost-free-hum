#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/world/dooranimationpose.h"
#include "game/startup/startupresources.h"

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace {
    template<class T, class U> T number(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        U bits = 0;
        for (unsigned i = 0; i < sizeof(T); ++i) bits = (bits << 8) | bytes[offset + i];
        T result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }
}

bool dooranimation::PoseTables::read(const std::vector<std::uint8_t>& executable, std::string& error) {
    error.clear();
    PoseTables result;
    std::vector<std::uint8_t> bytes;
    if (!dolresources::read(executable, glme01::sineTable, 4096 * 4, bytes, error)) return false;
    for (unsigned i = 0; i < 4096; ++i) result.sine[i] = number<float, std::uint32_t>(bytes, i * 4);
    if (!dolresources::read(executable, glme01::cosineTable, 4096 * 4, bytes, error)) return false;
    for (unsigned i = 0; i < 4096; ++i) result.cosine[i] = number<float, std::uint32_t>(bytes, i * 4);
    if (!dolresources::read(executable, 0x804d9c40, 16, bytes, error)) return false;
    result.bias = number<double, std::uint64_t>(bytes, 0);
    result.angleScale = number<double, std::uint64_t>(bytes, 8);
    if (!dolresources::read(executable, 0x804d9c10, 16, bytes, error)) return false;
    result.staticBias = number<double, std::uint64_t>(bytes, 0);
    result.staticAngleScale = number<double, std::uint64_t>(bytes, 8);
    result.loaded = true;
    *this = std::move(result);
    return true;
}

dooranimation::Matrix dooranimation::PoseTables::local(const std::array<float, 9>& v) const {
    return local(v, bias, angleScale);
}

dooranimation::Matrix dooranimation::PoseTables::staticLocal(const std::array<float, 9>& v) const {
    return local(v, staticBias, staticAngleScale);
}

dooranimation::Matrix dooranimation::PoseTables::world(const std::array<float, 3>& position,
    const std::array<std::int32_t, 3>& angles, const std::array<float, 3>& scale) const {
    if (!loaded) throw std::runtime_error("Door pose tables are not loaded.");
    const auto x = (static_cast<std::uint32_t>(angles[0]) >> 4) & 4095;
    const auto y = (static_cast<std::uint32_t>(angles[1]) >> 4) & 4095;
    const auto z = (static_cast<std::uint32_t>(angles[2]) >> 4) & 4095;
    const float sx = sine[x], sy = sine[y], sz = sine[z];
    const float cx = cosine[x], cy = cosine[y], cz = cosine[z];
    const float sysx = sy * sx, cysx = cy * sx;
    const float a = cy * cz, b = sz * sysx;
    const float c = -cy * sz, d = cz * sysx;
    const float e = -sy * cz, f = sz * cysx;
    const float g = sy * sz, h = cz * cysx;
    return {scale[0] * (a + b), scale[1] * (c + d), scale[2] * (sy * cx), position[0],
        scale[0] * (cx * sz), scale[1] * (cx * cz), -sx * scale[2], position[1],
        scale[0] * (e + f), scale[1] * (g + h), scale[2] * (cy * cx), position[2]};
}

float dooranimation::PoseTables::sineAngle(std::uint32_t angle) const {
    if (!loaded) throw std::runtime_error("Door pose tables are not loaded.");
    return sine[(angle >> 4) & 4095];
}

dooranimation::Matrix dooranimation::PoseTables::local(const std::array<float, 9>& v,
    double conversionBias, double conversionScale) const {
    if (!loaded) throw std::runtime_error("Door pose tables are not loaded.");
    const auto index = [&](float angle) {
        const double scaled = conversionScale * static_cast<double>(angle);
        const double converted = conversionBias + scaled;
        if (std::isnan(converted) || converted <= -2147483648.0) return 0U;
        if (converted >= 2147483648.0) return 4095U;
        return (static_cast<std::uint32_t>(static_cast<std::int32_t>(converted)) >> 4) & 4095;
    };
    const unsigned x = index(v[3]), y = index(v[4]), z = index(v[5]);
    const float sx = sine[x], sy = sine[y], sz = sine[z];
    const float cx = cosine[x], cy = cosine[y], cz = cosine[z];
    const float czsy = cz * sy, szsy = sz * sy;
    const float a = -sz * cx, b = sx * czsy;
    const float c = sz * sx, d = cx * czsy;
    const float e = cz * cx, f = sx * szsy;
    const float g = -cz * sx, h = cx * szsy;
    return {v[0] * (cz * cy), v[1] * (a + b), v[2] * (c + d), v[6],
        v[0] * (sz * cy), v[1] * (e + f), v[2] * (g + h), v[7],
        -sy * v[0], v[1] * (cy * sx), v[2] * (cy * cx), v[8]};
}

dooranimation::Matrix dooranimation::concatenate(const Matrix& parent, const Matrix& local) {
    Matrix result;
    for (unsigned row = 0; row < 3; ++row) {
        for (unsigned column = 0; column < 4; ++column) {
            float value = local[column] * parent[row * 4];
            value = std::fma(local[4 + column], parent[row * 4 + 1], value);
            value = std::fma(local[8 + column], parent[row * 4 + 2], value);
            if (column >= 2) value = std::fma(column == 3 ? 1.0f : 0.0f, parent[row * 4 + 3], value);
            result[row * 4 + column] = value;
        }
    }
    return result;
}

dooranimation::Matrix dooranimation::samplePose(const Resource& resource, const PoseTables& tables,
    unsigned track, float frame, const Matrix& parent) {
    std::array<float, 9> channels;
    for (unsigned channel : {0U, 1U, 2U, 6U, 7U, 8U, 3U, 4U, 5U})
        channels[channel] = sampleChannel(resource, track, channel, frame);
    return concatenate(parent, tables.local(channels));
}
