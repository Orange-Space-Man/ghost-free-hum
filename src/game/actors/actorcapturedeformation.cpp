#include "game/actors/actorcapturemotion.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
    using Matrix = actormodel::Matrix;

    Matrix concatenate(const Matrix& a, const Matrix& b) {
        Matrix result;
        for (unsigned row = 0; row < 3; ++row) for (unsigned column = 0; column < 4; ++column) {
            const unsigned r = row * 4;
            result[r + column] = std::fma(a[r + 2], b[8 + column], std::fma(a[r + 1], b[4 + column], a[r] * b[column]));
            if (column == 3) result[r + column] += a[r + 3];
        }
        return result;
    }

    Matrix scale(float x, float y, float z) { return {x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0}; }
    Matrix translate(float x, float y, float z) { return {1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z}; }

    Matrix rotate(float angle, char axis) {
        const float sine = std::sin(angle), cosine = std::cos(angle);
        if (axis == 'x') return {1, 0, 0, 0, 0, cosine, -sine, 0, 0, sine, cosine, 0};
        if (axis == 'y') return {cosine, 0, sine, 0, 0, 1, 0, 0, -sine, 0, cosine, 0};
        return {cosine, -sine, 0, 0, sine, cosine, 0, 0, 0, 0, 1, 0};
    }

    std::int32_t integer(float value) {
        return !std::isfinite(value) || double(value) >= 2147483648.0 || value < -2147483648.0f ?
            std::numeric_limits<std::int32_t>::min() : static_cast<std::int32_t>(value);
    }

    constexpr float radians = 0.01745329238474369f;
}

void actorcapture::Motion::jointParameters(State& s, const Vector& position, bool vertical) const {
    const float offset = position[vertical ? 1 : 2];
    s.number(0x2e8, s.number(0x48) + offset);
    s.number(0x2e4, s.number(0x40) + offset);
    if (vertical) s.number(0x2ec, s.number(0x2e4) / s.number(0x2e8));
    if (s.half(0x210) == 3) s.number(0x2f0, (s.number(0x44) + offset) / (s.number(0x19c) + offset));
    s.number(0x2ec, s.number(0x2e4) / s.number(0x2e8));
}

actormodel::Matrix actorcapture::Motion::basicJointTransform(State& s, const Vector& position) const {
    const unsigned type = s.half(0x210);
    if (type > 1) throw std::invalid_argument("Original basic capture transform expects type zero or one.");
    if (!type) jointParameters(s, position);
    const float amount = s.number(0x54);
    const auto scaling = ::scale(amount * s.number(0x30), amount * s.number(0x34), amount * s.number(0x38));
    const float z = (s.number(0x2e4) - position[2]) / s.number(0x38);
    Matrix transformed;
    if (!type) {
        const auto angle = static_cast<std::uint16_t>(integer(((65535.0f * s.number(0x1a8)) * float(s.byte(0x180))) * s.number(0x2ec)));
        s.number(0x3f0, (s.number(0x21c) * pose.horizontalDirection(angle)[0]) / s.number(0x34));
        const float wave = (s.number(0x3f0) * s.number(0x214)) * s.number(0x2dc);
        const float y = ((wave * (1.0f + 0.5f * (1.0f - amount))) * s.number(0x184)) * s.number(0x34);
        transformed = translate(0, y, z);
    } else {
        const float angle = ((radians * 360.0f) * s.number(0x2ec)) * float(s.byte(0x180));
        transformed = concatenate(rotate(angle, 'z'), translate(0, 0, z));
    }
    return concatenate(scaling, concatenate(transformed, s.matrix(0x120)));
}

bool actorcapture::Motion::ordinaryJointTransform(State& s, const Vector& p, Matrix& output, const float* originalRegister) const {
    jointParameters(s, p);
    const unsigned type = s.half(0x210);
    const bool complete = (type == 5 || type == 9) && s.number(0x3fc) >= 1;
    if (type != 5 && type != 9 && s.number(0x214) < 0) {
        output = ::scale(0, 0, 0);
        return true;
    }
    const auto trig = [&](float angle) { return pose.horizontalDirection(static_cast<std::uint16_t>(integer(angle))); };
    const float ratio = s.number(0x214), jointRatio = s.number(0x2ec), cycles = float(s.byte(0x180));
    const float size = s.number(0x54);
    const auto scaling = ::scale(size, size, size);
    switch (type) {
    case 0:
    case 1:
        output = basicJointTransform(s, p);
        break;
    case 2:
    case 4: {
        float angle = 90.0f * (1.0f - ratio);
        if (type == 4) {
            angle = -45.0f * (1.0f + 4.0f * (1.0f - ratio));
            if (angle < -90) angle = -90;
        }
        const auto rotation = concatenate(rotate(radians * angle, 'x'), rotate(((radians * 360.0f) * jointRatio) * cycles, 'y'));
        output = concatenate(scaling, concatenate(translate(0, 0, s.number(0x2e4) - p[2] * jointRatio), rotation));
        break;
    }
    case 3: {
        const auto z = rotate((((radians * jointRatio) * 360.0f) * s.number(0x2f0)) * s.number(0x1ac), 'z');
        float angle;
        if (s.number(0x190) <= 0.5f) angle = ((((radians * cycles) * 360.0f) * jointRatio) * s.number(0x2f0)) * s.number(0x1ac);
        else if (s.number(0x190) > 0.5f) angle = ((radians * 360.0f) * s.number(0x2f0)) * cycles;
        else throw std::runtime_error("Original capture type three reads an unwritten rotation matrix for unordered phase values.");
        output = concatenate(scaling, concatenate(translate(0, 0, s.number(0x2e4) - p[2]), concatenate(z, rotate(angle, 'x'))));
        break;
    }
    case 5:
    case 9: {
        if (s.byte(0x360) == 1) {
            const float fade = s.number(0x3fc);
            const auto translation = type == 5 ? translate(0, 0, 0) :
                translate(0, 100.0f * trig((65535.0f * 0.5f) * fade)[0],
                    s.number(0x48) - (s.number(0x40) - fade * (s.number(0x40) - 200.0f)));
            const float y = type == 5 ? static_cast<float>(double(s.number(0x3f4)) * (1.0 + 0.5 * double(fade))) :
                s.number(0x3f4) * (1.0f + 2.0f * fade);
            output = concatenate(translation, concatenate(rotate(y, 'y'), rotate((1.0f - fade) * -trig(2.0f * s.number(0x3f0))[2], 'x')));
            break;
        }
        if (s.byte(0x360) != 0) return complete;
        s.number(0x3f0, (65535.0f * cycles) * ratio);
        s.number(0x3f4, static_cast<float>(3.141592653589793 - double(trig(s.number(0x3f0))[2])));
        float wave = trig(s.number(0x3f0))[0];
        if (type == 5 && s.number(0x410) == 1) {
            wave = -wave;
            s.number(0x3f4, -s.number(0x3f4));
        }
        const float amount = s.number(0x184);
        const float harmonic = trig(((ratio * 65535.0f) * jointRatio) * float(unsigned(s.byte(0x358)) * s.byte(0x180)))[0];
        Matrix translation;
        if (type == 5 && ratio <= 0.5f) {
            s.number(0x400, ratio);
            s.word(0x404, s.word(0x2ec));
            translation = translate(0, ((ratio * amount) * s.number(0x2dc)) * harmonic, s.number(0x48) - s.number(0x40));
        } else {
            float factor = ratio;
            if (type == 5) {
                factor = (ratio - s.number(0x400)) / (1.0f - s.number(0x400));
                s.number(0x408, factor);
                s.number(0x40c, (jointRatio - s.number(0x404)) / (1.0f - s.number(0x404)));
            }
            const float y = ((trig(2.0f * s.number(0x3f0))[0] * factor) * s.number(0x354)) * amount +
                ((ratio * amount) * s.number(0x2dc)) * harmonic;
            translation = translate((wave * amount) * factor, y, s.number(0x48) - s.number(0x40));
        }
        const auto rotation = concatenate(rotate(s.number(0x3f4), 'y'), rotate(-trig(2.0f * s.number(0x3f0))[2], 'x'));
        output = concatenate(translation, concatenate(rotation, type == 5 ? ::scale(ratio, ratio, ratio) : scaling));
        break;
    }
    case 7:
    case 8: {
        s.number(0x3f0, (65535.0f * cycles) * ratio);
        s.number(0x3f4, static_cast<float>(3.141592653589793 + double(trig(s.number(0x3f0))[2])));
        const float amount = s.number(0x184);
        const float harmonic = trig(((ratio * 65535.0f) * jointRatio) * float(unsigned(s.byte(0x358)) * s.byte(0x180)))[0];
        const float y = ((trig(2.0f * s.number(0x3f0))[0] * ratio) * s.number(0x354)) * amount +
            ((ratio * amount) * s.number(0x2dc)) * harmonic;
        const auto translation = translate((trig(s.number(0x3f0))[0] * amount) * ratio, y, s.number(0x40));
        const auto rotation = concatenate(rotate(s.number(0x3f4), 'y'), rotate(trig(2.0f * s.number(0x3f0))[2], 'x'));
        output = concatenate(translation, concatenate(rotation, scaling));
        break;
    }
    case 10: {
        jointParameters(s, p);
        float clamped = s.number(0x2ec);
        if (!(clamped <= 1)) clamped = 1;
        else if (clamped < 0) clamped = 0;
        float squared;
        if (s.byte(0x2e0)) squared = clamped * clamped;
        else {
            if (!originalRegister) throw std::runtime_error("Original capture type ten requires the retained floating-point register.");
            squared = *originalRegister;
        }
        const float factor = 1.0f - squared;
        s.number(0x3f0, 65535.0f * (cycles * clamped));
        const float wave = trig(s.number(0x3f0))[0];
        const auto translation = translate((wave * s.number(0x184)) * (1.0f - clamped), 0, s.number(0x40));
        const auto rotation = concatenate(rotate(s.number(0x42c) + wave, 'y'), rotate(s.number(0x428), 'x'));
        output = concatenate(translation, concatenate(rotation, ::scale(factor, factor, factor)));
        break;
    }
    }
    return complete;
}

bool actorcapture::Motion::jointTransform(State& s, double amount, const Vector& p, Matrix& output, const float* originalRegister) const {
    const unsigned type = s.half(0x210);
    if (type < 100) return ordinaryJointTransform(s, p, output, originalRegister);
    jointParameters(s, p, true);
    if (type != 100 && type != 120 && type != 123) return false;
    Vector delta;
    const auto origin = s.vector(0xc), end = s.vector(0x260);
    for (unsigned i = 0; i < 3; ++i) delta[i] = end[i] - (origin[i] + p[i]);
    const float square = std::fma(delta[2], delta[2], delta[0] * delta[0]) + delta[1] * delta[1];
    const float distance = std::sqrt(square);
    if (distance != 0) {
        const float estimate = 1.0f / std::sqrt(square);
        const float inverse = (3.0f - (estimate * estimate) * square) * (estimate * 0.5f);
        for (auto& component : delta) component *= inverse;
        s.vector(0x2fc, delta);
    }
    s.number(0x2f8, s.number(0x278) / distance);
    float influence = type == 100 ? static_cast<float>(double(s.number(0x2f8)) - amount) : s.number(0x2f8) - 0.5f;
    if (influence < 0) influence = 0;
    s.number(0x2f8, influence);
    if (type != 100 && s.half(0x3a0) > 439) {
        output = ::scale(0, 0, 0);
        return true;
    }
    const auto trig = [&](float angle) { return pose.horizontalDirection(static_cast<std::uint16_t>(integer(angle))); };
    if (type == 100) {
        if (s.number(0x27c) < 0.03f) return true;
        float shift = (s.number(0x284) * s.number(0x330)) * influence;
        if (s.number(0x32c) < shift) shift = s.number(0x32c);
        const auto wave = trig((float(s.half(0x308)) * s.number(0x320)) * s.number(0x2ec));
        const float vertical = s.number(0x184) * wave[0];
        output = translate(s.number(0x184) * wave[2] + s.number(0x2fc) * shift,
            vertical + s.number(0x300) * shift, vertical + s.number(0x304) * shift);
        return false;
    }
    const auto phase = s.word(0x39c);
    if (type == 120 && phase == 0) {
        const auto wave = trig(((s.number(0x214) * 65535.0f) * s.number(0x38c)) * s.number(0x2ec));
        const auto translation = translate(0, s.number(0x368), (s.number(0x398) * (1.0f - s.number(0x214))) * wave[0]);
        const float size = s.number(0x3a8);
        output = concatenate(s.matrix(0x60), concatenate(translation, concatenate(rotate(1.5707963705062866f, 'x'), ::scale(size, size, size))));
        return false;
    }
    if (type == 120 && phase == 1) {
        const auto first = concatenate(translate(0, s.number(0x368), 0), rotate(radians * (90.0f - s.number(0x3e4)), 'x'));
        output = concatenate(s.matrix(0x60), concatenate(first, rotate(radians * s.number(0x3e0), 'z')));
        return false;
    }
    if (type == 123 && (phase == 0 || phase == 1)) {
        const auto translation = translate(s.number(0x368), 0.9f * s.number(0x368) - 10.0f, 0);
        const auto scaling = ::scale(1, phase == 0 ? s.number(0x3e0) : 1, s.number(0x3ec));
        const auto rotation = phase == 0 ? concatenate(rotate(2.181661605834961f, 'z'), rotate(1.5707963705062866f, 'y')) :
            concatenate(rotate(radians * (125.0f - s.number(0x3e0)), 'z'), rotate(radians * (90.0f - s.number(0x3e4)), 'y'));
        output = concatenate(s.matrix(0x60), concatenate(translation, concatenate(rotation, scaling)));
        return false;
    }
    float shift = (s.number(0x284) * s.number(0x330)) * influence;
    if (s.number(0x32c) < shift) shift = s.number(0x32c);
    const auto wave = trig((s.number(0x320) * float(s.half(0x3b0))) * s.number(0x2ec));
    const float amplitude = s.number(0x3b4) * s.number(0x388);
    Matrix translation, rotation;
    if (type == 120) {
        translation = translate((amplitude * wave[0] + s.number(0x3c4)) + s.number(0x2fc) * shift,
            ((s.number(0x368) + amplitude * wave[2]) + s.number(0x3c8)) + s.number(0x300) * shift,
            s.number(0x3cc) + s.number(0x304) * shift);
        rotation = concatenate(rotate(radians * (90.0f - s.number(0x3e4)), 'x'), rotate(radians * s.number(0x3e0), 'z'));
    } else {
        translation = translate(((s.number(0x368) + amplitude * wave[0]) + s.number(0x3c4)) + s.number(0x2fc) * shift,
            (((0.9f * s.number(0x368) + amplitude * wave[2]) + s.number(0x3c8)) + s.number(0x300) * shift) - 10.0f,
            s.number(0x3cc) + s.number(0x304) * shift);
        rotation = concatenate(rotate(radians * (125.0f - s.number(0x3e0)), 'z'), rotate(radians * (90.0f - s.number(0x3e4)), 'y'));
    }
    const auto first = concatenate(translation, s.matrix(0xc0));
    const auto second = concatenate(rotation, ::scale(1, 1, s.number(0x3ec)));
    output = concatenate(s.matrix(0x60), concatenate(first, second));
    return false;
}

bool actorcapture::Motion::deform(State& s, double amount, const Vector& position, Matrix& joint,
    Matrix& retainedTransform, const float* originalRegister) const {
    Vector scaled;
    for (unsigned i = 0; i < 3; ++i) scaled[i] = position[i] * s.number(0x34);
    const bool complete = jointTransform(s, amount, scaled, retainedTransform, originalRegister);
    const auto world = concatenate(s.matrix(0xf0), retainedTransform);
    retainedTransform = concatenate(s.matrix(0x1b0), world);
    joint = concatenate(retainedTransform, joint);
    return complete;
}
