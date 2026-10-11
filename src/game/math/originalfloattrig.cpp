#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/math/originalfloattrig.h"
#include "game/startup/startupresources.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

bool originalmath::FloatTrig::read(const std::vector<std::uint8_t>& executable, std::string& error) {
    FloatTrig result;
    const auto load = [&](std::uint32_t address, auto& values) {
        std::vector<std::uint8_t> bytes;
        if (!dolresources::read(executable, address, values.size() * 4, bytes, error)) return false;
        for (unsigned i = 0; i < values.size(); ++i) {
            std::uint32_t bits = 0;
            for (unsigned j = 0; j < 4; ++j) bits = (bits << 8) | bytes[i * 4 + j];
            std::memcpy(&values[i], &bits, sizeof(bits));
        }
        return true;
    };
    if (!load(glme01::trigReductionTable, result.reduction) || !load(glme01::trigPolynomialTable, result.polynomial) ||
        !load(glme01::trigConstants, result.constants)) return false;
    result.loaded = true;
    *this = result;
    return true;
}

std::array<float, 2> originalmath::FloatTrig::sinCos(float angle) const {
    if (!loaded) throw std::runtime_error("Original float trigonometric constants are not loaded.");
    const float scaled = constants[0] * angle;
    const float rounded = std::signbit(angle) ? scaled - constants[1] : constants[1] + scaled;
    std::int32_t integer;
    if (std::isnan(rounded) || rounded <= -2147483648.0f) integer = std::numeric_limits<std::int32_t>::min();
    else if (rounded >= 2147483648.0f) integer = std::numeric_limits<std::int32_t>::max();
    else integer = static_cast<std::int32_t>(rounded);
    const auto doubled = static_cast<std::uint32_t>(integer) << 1;
    const auto signedDouble = doubled < 0x80000000 ? std::int64_t(doubled) : std::int64_t(doubled) - 0x100000000LL;
    float reduced = angle - static_cast<float>(signedDouble);
    for (float coefficient : reduction) reduced = std::fma(coefficient, angle, reduced);
    const auto quadrant = static_cast<std::uint32_t>(integer) & 3;
    const float sineSign = polynomial[quadrant * 2], cosineSign = polynomial[quadrant * 2 + 1];
    if (!(std::fabs(reduced) >= constants[2])) {
        const float product = reduced * cosineSign;
        return {std::fma(polynomial[17], product, sineSign), -std::fma(reduced, sineSign, -cosineSign)};
    }
    const float squared = reduced * reduced;
    const auto evaluate = [&](unsigned offset) {
        float value = std::fma(polynomial[8 + offset], squared, polynomial[10 + offset]);
        value = std::fma(squared, value, polynomial[12 + offset]);
        value = std::fma(squared, value, polynomial[14 + offset]);
        return std::fma(squared, value, polynomial[16 + offset]);
    };
    const float even = evaluate(0), odd = evaluate(1);
    if (quadrant & 1) {
        const float negative = reduced * -odd;
        return {even * sineSign, negative * sineSign};
    }
    const float positive = reduced * odd;
    return {positive * cosineSign, even * cosineSign};
}
