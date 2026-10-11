#include "resources/dolresources.h"
#include "game/actors/actorcapturemotion.h"
#include "game/startup/startupresources.h"
#include "game/player/playerorientation.h"
#include "game/player/playervacuummodel.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
    using actorcapture::Vector;

    float magnitude(const Vector& v) {
        return std::sqrt(std::fma(v[2], v[2], v[0] * v[0]) + v[1] * v[1]);
    }

    Vector normalized(const Vector& v) {
        const float square = std::fma(v[2], v[2], v[0] * v[0]) + v[1] * v[1];
        const float estimate = 1.0f / std::sqrt(square);
        const float inverse = (3.0f - (estimate * estimate) * square) * (estimate * 0.5f);
        return {v[0] * inverse, v[1] * inverse, v[2] * inverse};
    }

    std::int32_t integer(float value) {
        return !std::isfinite(value) || double(value) >= 2147483648.0 || value < -2147483648.0f ?
            std::numeric_limits<std::int32_t>::min() : static_cast<std::int32_t>(value);
    }

    actormodel::Matrix concatenate(const actormodel::Matrix& a, const actormodel::Matrix& b) {
        actormodel::Matrix result;
        for (unsigned row = 0; row < 3; ++row) for (unsigned column = 0; column < 4; ++column) {
            const unsigned r = row * 4;
            result[r + column] = std::fma(a[r + 2], b[8 + column], std::fma(a[r + 1], b[4 + column], a[r] * b[column]));
            if (column == 3) result[r + column] += a[r + 3];
        }
        return result;
    }
}

actorcapture::State::State(const std::array<std::uint8_t, 0x430>& retained) : bytes(retained) {}
std::uint8_t actorcapture::State::byte(unsigned offset) const { return bytes.at(offset); }
std::uint16_t actorcapture::State::half(unsigned offset) const { return (unsigned(byte(offset)) << 8) | byte(offset + 1); }
std::uint32_t actorcapture::State::word(unsigned offset) const { return (unsigned(half(offset)) << 16) | half(offset + 2); }
float actorcapture::State::number(unsigned offset) const {
    const auto bits = word(offset);
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}
actorcapture::Vector actorcapture::State::vector(unsigned offset) const { return {number(offset), number(offset + 4), number(offset + 8)}; }
actormodel::Matrix actorcapture::State::matrix(unsigned offset) const {
    actormodel::Matrix value;
    for (unsigned i = 0; i < 12; ++i) value[i] = number(offset + i * 4);
    return value;
}
void actorcapture::State::byte(unsigned offset, std::uint8_t value) { bytes.at(offset) = value; }
void actorcapture::State::half(unsigned offset, std::uint16_t value) {
    byte(offset, static_cast<std::uint8_t>(value >> 8));
    byte(offset + 1, static_cast<std::uint8_t>(value));
}
void actorcapture::State::word(unsigned offset, std::uint32_t value) {
    half(offset, static_cast<std::uint16_t>(value >> 16));
    half(offset + 2, static_cast<std::uint16_t>(value));
}
void actorcapture::State::number(unsigned offset, float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, 4);
    word(offset, bits);
}
void actorcapture::State::vector(unsigned offset, const Vector& value) {
    for (unsigned i = 0; i < 3; ++i) number(offset + i * 4, value[i]);
}
void actorcapture::State::matrix(unsigned offset, const actormodel::Matrix& value) {
    for (unsigned i = 0; i < 12; ++i) number(offset + i * 4, value[i]);
}

actorcapture::Motion::Motion(const std::vector<std::uint8_t>& executable, const actormodel::InitialPose& pose,
    std::function<std::uint32_t()> random) : executable(executable), pose(pose), random(std::move(random)) {
    if (!this->random) throw std::invalid_argument("Original capture motion requires the shared random generator.");
}

float actorcapture::Motion::sample() { return static_cast<float>(random()) / 4294967296.0f; }

void actorcapture::Motion::copy(State& state, unsigned offset, unsigned address, unsigned size) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!dolresources::read(executable, address, size, bytes, error)) throw std::runtime_error(error);
    for (unsigned i = 0; i < size; ++i) state.byte(offset + i, bytes[i]);
}

void actorcapture::Motion::parameters(State& s) {
    const unsigned type = s.half(0x210);
    copy(s, 0x2bc, 0x80379788 + (type >= 120 && type <= 123 ? 0 : type) * 40, 40);
    if (!s.byte(0x2bc)) return;
    copy(s, 0x354, 0x80379904 + type * 20, 20);
    s.number(0x3fc, 0);
}

void actorcapture::Motion::ordinary(State& s) {
    s.word(0x48, s.word(0x3c));
    s.number(0x410, 0);
    if (s.half(0x210) == 6) {
        s.half(0x210, 5);
        s.number(0x410, 1);
    }
    if (s.half(0x210) == 5 || s.half(0x210) == 9) s.number(0x40, 0);
    else s.word(0x40, s.word(0x3c));
    if (s.half(0x210) == 3) {
        s.number(0x44, 1500);
        s.word(0x19c, s.word(0x44));
    }
    for (unsigned offset : {0x1a8u, 0x1acu, 0x198u, 0x190u}) s.number(offset, 1);
    s.number(0x1a0, 10);
    s.number(0x1a4, 0);
    s.number(0x3f8, 0);
}

void actorcapture::Motion::special(State& s, bool alternate) {
    copy(s, 0x308, (alternate ? 0x80379a70 : 0x80379a24) + 8, 76);
    s.word(0x48, s.word(0x328));
    s.word(0x40, s.word(0x328));
    s.number(0x27c, 0.03f);
    s.number(0x41c, alternate ? 180.0f : 150.0f);
    s.number(0x420, alternate ? 9.5f : 6.5f);
    s.number(0x424, alternate ? 0.97f : 0.98f);
    copy(s, 0x368, 0x80379abc + 8, 136);
    s.word(0x39c, 0);
    s.half(0x3a0, 0);
    s.number(0x3a4, 0);
    s.number(0x3a8, 0);
    s.half(0x3ac, 0);
    s.half(0x3b0, 0);
    s.word(0x368, s.word(0x41c));
    s.word(0x36c, s.word(0x420));
    s.half(0x3ae, 0);
    s.number(0x3d4, s.number(0x398) / 30.0f);
    const float first = sample();
    s.number(0x3d8, alternate ? 80.0f + 10.0f * first : 70.0f + 20.0f * (1.0f - 2.0f * first));
    const float second = sample();
    s.number(0x3dc, alternate ? 20.0f + 10.0f * second : 60.0f + 30.0f * second);
    s.number(0x3e8, 1.0f - (alternate ? 0.013f : 0.5f) * sample());
    s.number(0x3ec, alternate ? 0.1f : 1);
    s.number(0x40, 300);
    s.number(0x48, 300);
    s.number(0x1a8, 1);
    s.number(0x330, 0);
}

void actorcapture::Motion::segments(State& s) {
    if (!s.byte(0x2bc)) {
        const float value = (s.number(0x3c) * s.number(0x2c0)) * (0.5f + 0.5f * sample());
        s.byte(0x180, static_cast<std::uint8_t>(integer(value)));
        if (s.byte(0x180) < s.byte(0x2bd)) s.byte(0x180, s.byte(0x2bd));
    } else s.byte(0x180, s.byte(0x2bd));
    s.number(0x184, s.number(0x2c4) * (1.0f + 0.2f * (1.0f - 2.0f * sample())));
    s.number(0x188, 10.0f * (0.8f + 0.2f * sample()));
    s.number(0x2d0, s.number(0x2d0) * (0.01f + (7.0f / s.number(0x40)) * (0.8f + 0.2f * sample())));
    s.number(0x3f8, s.number(0x364) * s.number(0x188));
    s.number(0x54, 1);
}

void actorcapture::Motion::endpoints(State& s, const Vector& position, const Vector& target) {
    s.vector(0xc, position);
    s.vector(0, target);
    Vector delta;
    for (unsigned i = 0; i < 3; ++i) delta[i] = position[i] - target[i];
    s.vector(0x18, delta);
    s.number(0x3c, magnitude(delta));
    if (s.number(0x3c) != 0) s.vector(0x24, normalized(delta));
    s.number(0x214, s.number(0x40) / s.number(0x48));
    const auto direction = s.vector(0x24);
    s.number(0x5c, 0.0054932478815317154f * pose.angle(direction[2], direction[0]));
    const float horizontal = std::sqrt(direction[0] * direction[0] + direction[2] * direction[2]);
    s.number(0x58, 0.0054932478815317154f * pose.angle(horizontal, -double(direction[1])));
    const float x = 0.01745329238474369f * s.number(0x58);
    const float y = 0.01745329238474369f * s.number(0x5c);
    const float sx = std::sin(x), cx = std::cos(x), sy = std::sin(y), cy = std::cos(y);
    const actormodel::Matrix rx{1, 0, 0, 0, 0, cx, -sx, 0, 0, sx, cx, 0};
    const actormodel::Matrix ry{cy, 0, sy, 0, 0, 1, 0, 0, -sy, 0, cy, 0};
    const auto rotation = concatenate(ry, rx);
    const actormodel::Matrix translation{1, 0, 0, target[0], 0, 1, 0, target[1], 0, 0, 1, target[2]};
    s.matrix(0xc0, rotation);
    s.matrix(0x90, translation);
    s.matrix(0xf0, concatenate(translation, rotation));
}

void actorcapture::Motion::specialEndpoints(State& s, const Vector& target, const Vector& position) {
    s.vector(0xc, position);
    s.vector(0, target);
    auto middle = position;
    middle[1] += s.half(0x210) == 123 ? s.number(0x348) : s.number(0x348) * (1.0f - 2.0f * sample());
    s.vector(0x254, middle);
    Vector difference;
    for (unsigned i = 0; i < 3; ++i) difference[i] = target[i] - position[i];
    const auto direction = normalized(difference);
    s.vector(0x26c, direction);
    Vector end;
    for (unsigned i = 0; i < 3; ++i) end[i] = direction[i] * s.number(0x338);
    for (unsigned i = 0; i < 3; ++i) end[i] = position[i] + end[i];
    s.vector(0x260, end);
    field8B34 = middle;
    field8B28 = end;
    for (unsigned i = 0; i < 3; ++i) difference[i] = end[i] - middle[i];
    s.number(0x278, magnitude(difference));
}

void actorcapture::Motion::initialize(State& s, const Vector& target, const Vector& position, std::uint16_t type) {
    s.byte(0x288, 1);
    s.vector(0x248, position);
    s.byte(0x220, 1);
    s.byte(0x289, 0);
    s.half(0x210, type);
    s.number(0x18c, 0);
    s.number(0x218, 1000);
    s.number(0x21c, sample() > 0.5f ? 1.0f : -1.0f);
    endpoints(s, position, target);
    parameters(s);
    if (type < 100) ordinary(s);
    else if (type >= 120 && type <= 122) {
        special(s, false);
        specialEndpoints(s, s.vector(0x33c), position);
    } else if (type == 123) special(s, true);
    else {
        copy(s, 0x308, 0x803799d8 + 8, 76);
        s.word(0x48, s.word(0x328));
        s.word(0x40, s.word(0x328));
        s.number(0x27c, 0.03f);
    }
    segments(s);
}

bool actorcapture::targetDirection(const Vector& target, const Vector& axis, const Vector& position, Vector& output) {
    Vector delta;
    for (unsigned i = 0; i < 3; ++i) delta[i] = position[i] - target[i];
    const float distance = magnitude(delta);
    const auto direction = distance <= 0 ? Vector{0, 1, 0} : normalized(delta);
    const float projection = std::fma(direction[0], axis[0], direction[1] * axis[1]) + direction[2] * axis[2];
    if (!(projection >= 0.3f)) return false;
    const float amount = projection * distance;
    Vector transverse;
    for (unsigned i = 0; i < 3; ++i) transverse[i] = axis[i] * amount;
    for (unsigned i = 0; i < 3; ++i) transverse[i] -= delta[i];
    if (!(magnitude(transverse) >= 25)) return false;
    output = normalized(transverse);
    return true;
}

float actorcapture::Motion::scale(const State& s) {
    float ratio;
    if (!(s.number(0x48) >= 250)) ratio = s.number(0x214);
    else {
        if (s.number(0x40) >= 250) return 1;
        ratio = s.number(0x40) / 250.0f;
    }
    for (unsigned i = 0; i < s.byte(0x2e0); ++i) ratio *= ratio;
    return s.number(0x2d8) + (1.0f - s.number(0x2d8)) * ratio;
}

void actorcapture::Motion::follow(State& s, const Vector& axis) {
    auto velocity = s.vector(0x23c);
    const float speed = axis[1] > 0 ? 20.0f : 10.0f;
    for (auto& value : velocity) value *= speed;
    s.vector(0x23c, velocity);
    auto position = s.vector(0x248);
    for (unsigned i = 0; i < 3; ++i) position[i] += velocity[i];
    s.vector(0x248, position);
}

void actorcapture::Motion::advanceOrdinary(State& s, const Vector& target, const Vector& position,
    const Vector* segmentDirection, const Vector* suctionDirection) {
    if (!segmentDirection) endpoints(s, position, target);
    else {
        if (!suctionDirection) throw std::runtime_error("Capture following requires the original cached suction direction.");
        auto direction = s.vector(0x23c);
        const bool result = targetDirection(target, *suctionDirection, s.vector(0x248), direction);
        s.byte(0x220, result ? 1 : 0);
        if (!result) {
            direction = *segmentDirection;
            for (auto& value : direction) value *= 5;
            s.vector(0x23c, direction);
            auto next = s.vector(0x248);
            for (unsigned i = 0; i < 3; ++i) next[i] += direction[i];
            s.vector(0x248, next);
        } else {
            s.vector(0x23c, direction);
            follow(s, *suctionDirection);
        }
        endpoints(s, s.vector(0x248), target);
    }
    if (s.number(0x188) < s.number(0x2cc)) s.number(0x1a4, s.number(0x1a4) + s.number(0x2d0));
    s.number(0x188, s.number(0x188) + s.number(0x1a4));
    if (s.number(0x188) < 0) s.number(0x188, 0);
    if (s.number(0x1a8) < s.number(0x218)) s.number(0x1a8, s.number(0x1a8) + s.number(0x2d4));
    const auto type = s.half(0x210);
    if (type == 5 || type == 9) {
        if (s.byte(0x360) || double(s.number(0x214)) <= 0.99999) {
            if (s.byte(0x360) == 1) {
                s.number(0x3fc, s.number(0x3fc) + s.number(0x3f8));
                if (s.number(0x3fc) > 1) s.number(0x3fc, 1);
            } else {
                s.number(0x40, s.number(0x40) + s.number(0x188));
                s.number(0x188, 0.3f + s.number(0x2c8) * (1.0f - s.number(0x214)));
            }
        } else s.byte(0x360, 1);
    } else if (type == 3) {
        if (s.number(0x190) <= 0.5f) {
            if (s.number(0x18c) == 0) {
                s.number(0x18c, 1);
                s.number(0x190, s.number(0x44) / s.number(0x19c) - s.number(0x190));
            }
        } else {
            s.word(0x40, s.word(0x3c));
            s.word(0x48, s.word(0x3c));
            s.number(0x190, s.number(0x44) / s.number(0x19c));
            s.number(0x44, static_cast<float>(double(s.number(0x44)) - 0.2 * double(s.number(0x188))));
            if (s.number(0x198) > 0.5f) s.number(0x198, s.number(0x198) - 0.01f);
        }
        if (s.number(0x18c) == 1) {
            s.number(0x1ac, s.number(0x1ac) + s.number(0x190));
            if (s.number(0x190) >= -0.01f) s.number(0x190, s.number(0x190) + 0.01f * s.number(0x190));
        }
        if (s.number(0x190) <= -0.01f) {
            s.number(0x40, s.number(0x40) - s.number(0x1a0));
            s.number(0x1a0, s.number(0x1a0) + 0.03f * s.number(0x1a0));
        }
        if (s.number(0x1ac) <= -100) s.number(0x1ac, -100);
    } else s.number(0x40, s.number(0x40) - s.number(0x188));
    s.number(0x54, scale(s));
    const actormodel::Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    double blend = 5.0f * static_cast<float>(1.0 - double(s.number(0x214)));
    if (blend > 1) blend = 1;
    const float angle = 0.01745329238474369f * (0.0054932478815317154f * pose.angle(s.number(0x20), -double(s.number(0x18))));
    const float sine = std::sin(angle), cosine = std::cos(angle);
    const actormodel::Matrix rotation{cosine, 0, sine, 0, 0, 1, 0, 0, -sine, 0, cosine, 0};
    auto matrix = playerorientation::blend(blend, concatenate(s.matrix(0x28c), rotation), identity);
    matrix[3] = matrix[7] = matrix[11] = 0;
    s.matrix(0x120, matrix);
}

void actorcapture::Motion::advanceSpecial(State& s, bool alternate) {
    const auto phase = static_cast<std::int32_t>(s.word(0x39c));
    if (phase == 3) s.number(0x330, s.number(0x330) + s.number(0x384));
    else if (phase == 2) {
        s.number(0x3a4, s.number(0x3a4) + s.number(0x37c));
        s.half(0x3ac, static_cast<std::uint16_t>(s.half(0x3ac) + s.half(0x380)));
    } else if (phase == 4) s.number(0x3a4, s.number(0x3a4) - s.number(0x3bc));
    const unsigned tick = s.half(0x3a0);
    const auto rotate = [&] {
        s.half(0x3b0, static_cast<std::uint16_t>(s.half(0x3b0) + s.half(0x3ac)));
        s.half(0x3ae, static_cast<std::uint16_t>(integer(float(s.half(0x3ae)) + float(s.half(0x3ac)) * s.number(0x3e8))));
    };
    if (tick < 101) {
        s.word(0x39c, tick < 31 ? 0 : 1);
        if (tick < 31 && !alternate) {
            s.number(0x3a8, s.number(0x3a8) + 0.03333333507180214f);
            if (s.number(0x3a8) > 1) s.number(0x3a8, 1);
        }
        s.number(0x36c, s.number(0x36c) * s.number(0x424));
        s.number(0x368, s.number(0x368) - s.number(0x36c));
        s.number(0x368, s.number(0x368) + s.number(0x374));
        if (tick < 31) {
            s.number(0x398, s.number(0x398) - s.number(0x3d4));
            if (alternate) s.number(0x3e0, float(tick) / 30.0f);
        } else {
            s.number(0x3e4, s.number(0x3dc) * ((float(tick) - 30.0f) / 70.0f));
            s.number(0x3e0, alternate ? (45.0f * (float(tick) - 30.0f)) / 70.0f :
                s.number(0x3d8) * (-(float(tick) - 30.0f) / 70.0f));
        }
    } else if (tick <= 230) {
        s.word(0x39c, 2);
        rotate();
    } else if (tick <= 380) {
        s.word(0x39c, 3);
        s.word(0x3b8, s.word(0x3a4));
        s.number(0x3bc, s.number(0x3b8) / 60.0f);
        s.half(0x3b2, s.half(0x3ac));
        s.half(0x3c0, static_cast<std::uint16_t>(integer(float(s.half(0x3b2)) / 60.0f)));
        rotate();
        const float progress = (float(tick) - 230.0f) / 150.0f;
        if (alternate) {
            s.number(0x3ec, progress > 1 ? 1 : progress);
            s.number(0x3e0, 25.0f + (s.number(0x3d8) * (float(tick) - 230.0f)) / 150.0f);
            s.number(0x368, s.number(0x368) - 0.1f);
        } else {
            const float amount = 1.0f - progress;
            s.number(0x3ec, amount < 0.1f ? 0.1f : amount);
            s.number(0x368, s.number(0x368) + 0.2f);
        }
    } else {
        s.half(0x3ac, static_cast<std::uint16_t>(s.half(0x3ac) - s.half(0x3c0)));
        rotate();
        s.word(0x39c, 4);
    }
    s.half(0x3a0, static_cast<std::uint16_t>(tick + 1));
    s.number(0x3b4, s.number(0x378) * s.number(0x3a4));
    const int orbitAngle = alternate ? -int(s.half(0x3b0)) : int(s.half(0x3b0));
    const auto direction = pose.horizontalDirection(static_cast<std::uint16_t>(orbitAngle));
    s.number(0x3c4, s.number(0x3b4) * direction[0]);
    s.number(0x3c8, 30.0f * pose.horizontalDirection(static_cast<std::uint16_t>(orbitAngle * 2))[0]);
    s.number(0x3cc, s.number(0x3b4) * direction[2]);
    const float degrees = 0.0054932478815317154f * float(orbitAngle);
    const float angle = 0.01745329238474369f * degrees;
    const float sine = std::sin(angle), cosine = std::cos(angle);
    s.matrix(0xc0, {cosine, 0, sine, 0, 0, 1, 0, 0, -sine, 0, cosine, 0});
    const auto arc = pose.horizontalDirection(s.half(0x3ae));
    const auto normal = normalized({arc[0], arc[2], 0});
    s.vector(0x26c, normal);
    const auto position = s.vector(0xc);
    Vector end;
    for (unsigned i = 0; i < 3; ++i) end[i] = normal[i] * s.number(0x338);
    for (unsigned i = 0; i < 3; ++i) end[i] = position[i] + end[i];
    s.vector(0x260, end);
    field8B34 = s.vector(0x254);
    field8B28 = end;
    Vector delta;
    for (unsigned i = 0; i < 3; ++i) delta[i] = end[i] - field8B34[i];
    s.number(0x278, magnitude(delta));
    s.number(0x27c, s.number(0x27c) + 1);
    if (s.number(0x34c) < s.number(0x27c)) s.word(0x27c, s.word(0x34c));
    if (!s.word(0x39c)) {
        s.number(0x3d0, s.number(0x418) / (float(s.half(0x3a0)) * s.number(0x414)));
        s.number(0x40, s.number(0x40) - s.number(0x48) / 30.0f);
        s.number(0x1a8, s.number(0x1a8) + s.number(0x2d4));
        s.number(0x214, s.number(0x40) / s.number(0x48));
    } else if (s.number(0x324) <= s.number(0x214)) s.number(0x40, s.number(0x40) - s.number(0x27c));
    s.half(0x308, static_cast<std::uint16_t>(s.half(0x308) + s.half(0x31c)));
    s.number(0x184, s.number(0x318) * s.number(0x27c));
    s.number(0x284, s.number(0x350) * s.number(0x27c));
    s.matrix(0xf0, {1, 0, 0, position[0], 0, 1, 0, position[1], 0, 0, 1, position[2]});
    s.matrix(0x60, {0.7f, 0, 0, 0, 0, 0.7f, 0, 0, 0, 0, 0.7f, 0});
}

bool actorcapture::invert(const actormodel::Matrix& m, actormodel::Matrix& output) {
    actormodel::Matrix r;
    r[0] = std::fma(m[5], m[10], -(m[9] * m[6]));
    r[4] = std::fma(m[6], m[8], -(m[10] * m[4]));
    r[1] = std::fma(m[9], m[2], -(m[1] * m[10]));
    r[5] = std::fma(m[10], m[0], -(m[2] * m[8]));
    r[2] = std::fma(m[1], m[6], -(m[5] * m[2]));
    r[6] = std::fma(m[2], m[4], -(m[6] * m[0]));
    r[8] = std::fma(m[4], m[9], -(m[5] * m[8]));
    r[9] = std::fma(m[1], m[8], -(m[0] * m[9]));
    r[10] = std::fma(m[0], m[5], -(m[1] * m[4]));
    const float determinant = std::fma(m[8], r[2], std::fma(m[4], r[1], m[0] * r[0]));
    if (determinant == 0) return false;
    float inverse = 1.0f / determinant;
    inverse = std::fma(-determinant, inverse * inverse, inverse + inverse);
    inverse = std::fma(-determinant, inverse * inverse, inverse + inverse);
    for (unsigned row = 0; row < 3; ++row) {
        const unsigned offset = row * 4;
        for (unsigned column = 0; column < 3; ++column) r[offset + column] *= inverse;
        r[offset + 3] = -std::fma(r[offset + 2], m[11], std::fma(r[offset + 1], m[7], r[offset] * m[3]));
    }
    output = r;
    return true;
}

void actorcapture::Motion::advance(State& s, const Vector& target, const Vector& position, const Vector& scale,
    const actormodel::Matrix& matrix, const playermodels::VacuumPath* path,
    const Vector* suctionDirection, const Vector* originalScratch) {
    s.vector(0x30, scale);
    Vector segment;
    const Vector* segmentDirection = nullptr;
    if (path) {
        if (path->count < 2 || !path->points) throw std::runtime_error("Original capture path has fewer than two points.");
        Vector delta;
        for (unsigned i = 0; i < 3; ++i) delta[i] = path->points[path->count - 2].position[i] - path->points[path->count - 1].position[i];
        if (magnitude(delta) != 0) segment = normalized(delta);
        else {
            if (!originalScratch) throw std::runtime_error("Coincident original capture path points require retained stack direction.");
            segment = *originalScratch;
        }
        segmentDirection = &segment;
    }
    if (!s.byte(0x289)) {
        auto rotation = matrix;
        rotation[3] = rotation[7] = rotation[11] = 0;
        s.matrix(0x28c, rotation);
    }
    s.matrix(0x1e0, matrix);
    auto inverse = s.matrix(0x1b0);
    if (invert(matrix, inverse)) s.matrix(0x1b0, inverse);
    const unsigned type = s.half(0x210);
    if (type < 100) advanceOrdinary(s, target, position, segmentDirection, suctionDirection);
    else {
        specialEndpoints(s, s.vector(0x33c), position);
        if (type >= 120 && type <= 123) advanceSpecial(s, type == 123);
        else {
            s.number(0x214, s.number(0x40) / s.number(0x48));
            s.number(0x280, 0.2f + 0.8f * s.number(0x30c));
            if (s.number(0x30c) > 0) s.number(0x27c, s.number(0x27c) + s.number(0x280) * s.number(0x310));
            else s.number(0x27c, s.number(0x27c) * s.number(0x314));
            if (s.number(0x34c) < s.number(0x27c)) s.word(0x27c, s.word(0x34c));
            s.half(0x308, static_cast<std::uint16_t>(s.half(0x308) + s.half(0x31c)));
            if (s.number(0x324) <= s.number(0x214)) s.number(0x40, s.number(0x40) - s.number(0x27c));
            s.number(0x184, s.number(0x318) * s.number(0x27c));
            s.number(0x284, s.number(0x350) * s.number(0x27c));
            const auto origin = s.vector(0xc);
            s.matrix(0xf0, {1, 0, 0, origin[0], 0, 1, 0, origin[1], 0, 0, 1, origin[2]});
        }
    }
    s.byte(0x289, 1);
    s.byte(0x288, 0);
}
