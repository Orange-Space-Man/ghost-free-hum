#include "game/models/modeldrawmatrices.h"

bool modeldraw::normalMatrix(const dooranimation::Matrix& m, dooranimation::Matrix& output) {
    const float a = m[0] * m[5], b = m[1] * m[6], c = m[2] * m[4];
    const float d = m[8] * m[5], e = m[4] * m[1], f = m[0] * m[9];
    const float positiveA = m[10] * a, positiveB = m[8] * b, positiveC = m[9] * c;
    const float negativeA = m[2] * d, negativeB = m[10] * e, negativeC = m[6] * f;
    const float first = positiveA + positiveB;
    const float second = positiveC + first;
    const float third = second - negativeA;
    const float fourth = third - negativeB;
    const float determinant = fourth - negativeC;
    if (determinant == 0) return false;
    const float reciprocal = 1.0f / determinant;
    const auto cofactor = [&](unsigned a, unsigned b, unsigned c, unsigned d, bool negative) {
        const float left = m[a] * m[b], right = m[c] * m[d];
        const float difference = left - right;
        return reciprocal * (negative ? -difference : difference);
    };
    dooranimation::Matrix result{};
    result[0] = cofactor(5, 10, 9, 6, false);
    result[4] = cofactor(1, 10, 9, 2, true);
    result[8] = cofactor(1, 6, 5, 2, false);
    result[1] = cofactor(4, 10, 8, 6, true);
    result[5] = cofactor(0, 10, 8, 2, false);
    result[9] = cofactor(0, 6, 4, 2, true);
    result[2] = cofactor(4, 9, 8, 5, false);
    result[6] = cofactor(0, 9, 8, 1, true);
    result[10] = cofactor(0, 5, 4, 1, false);
    output = result;
    return true;
}

dooranimation::Matrix modeldraw::textureMatrix(const dooranimation::Matrix& modelView) {
    auto result = modelView;
    result[3] = result[7] = result[11] = 0;
    return result;
}
