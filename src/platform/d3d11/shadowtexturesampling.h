#pragma once

namespace shadowtexturesampling {
    inline constexpr char shader[] = R"(
int2 sampleShadowBytes(Texture2D image, float2 coordinate, int2 size, bool wrapVertical) {
    int2 fixedCoordinate = int2(coordinate * float2(size) * 128) - 64;
    int2 origin = fixedCoordinate >> 7;
    int2 fraction = fixedCoordinate & 127;
    int2 sum = 0;
    [unroll] for (int y = 0; y < 2; ++y) {
        [unroll] for (int x = 0; x < 2; ++x) {
            int2 texel = origin + int2(x, y);
            texel.x = clamp(texel.x, 0, size.x - 1);
            texel.y = wrapVertical ? (texel.y & (size.y - 1)) : clamp(texel.y, 0, size.y - 1);
            int weight = (x ? fraction.x : 128 - fraction.x) * (y ? fraction.y : 128 - fraction.y);
            sum += int2(round(image.Load(int3(texel, 0)).rg * 255)) * weight;
        }
    }
    return sum >> 14;
}
)";
}
