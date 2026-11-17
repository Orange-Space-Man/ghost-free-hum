#pragma once

namespace shadowdepthcopy {
    // 8005D8CC copies the EFB depth as Z16 through GXCopyTex (801F5B2C) while the render-mode copy filter installed by
    // 8000766C (801F5778, taps 6,0,20,12,20,0,6 = previous/current/next row weights 6/52/6) and GXSetCopyClamp top+bottom
    // (801F55D8) remain in effect. The copy unit filters the already split middle/high depth bytes per row; the weights
    // sum to 64, so the shift cannot overflow a byte.
    inline constexpr char shader[] = R"(
Texture2D<float> depthImage : register(t0);
uint2 depthBytes(int2 pixel) {
    uint depth = (uint)round(saturate(depthImage.Load(int3(pixel, 0))) * 16777215.0);
    return uint2((depth >> 8) & 255, (depth >> 16) & 255);
}
float2 copyPixel(float4 position : SV_POSITION) : SV_TARGET {
    uint width, height;
    depthImage.GetDimensions(width, height);
    int2 pixel = int2(position.xy);
    int last = int(height) - 1;
    uint2 combined = depthBytes(int2(pixel.x, max(pixel.y - 1, 0))) * 6 + depthBytes(pixel) * 52 +
        depthBytes(int2(pixel.x, min(pixel.y + 1, last))) * 6;
    return float2(min(combined >> 6, 255)) / 255.0;
}
// Enhancement "Disable Copy Filter" (Dolphin): no row filter, the current row's depth bytes are copied unchanged.
float2 copyPixelUnfiltered(float4 position : SV_POSITION) : SV_TARGET {
    return float2(depthBytes(int2(position.xy))) / 255.0;
}
)";
}
