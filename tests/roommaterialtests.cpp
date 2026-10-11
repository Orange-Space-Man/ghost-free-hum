#include "game/world/roommaterials.h"

#include <iostream>

int main() {
    int failures = 0;
    const auto check = [&](bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << '\n';
            ++failures;
        }
    };
    roommaterials::Texture texture;
    std::string error;
    std::vector<std::uint8_t> bytes(32, 0);
    bytes[0] = 0xf8;
    check(roommaterials::decodeTexture(bytes, 4, 4, 4, texture, error), "RGB565 decode");
    check(texture.pixels.size() == 64 && texture.pixels[0] == 255 && texture.pixels[1] == 0 &&
        texture.pixels[3] == 255 && texture.pixels[4] == 0, "RGB565 color and pixel order");
    bytes.assign(32, 0);
    for (unsigned block = 0; block < 4; ++block) {
        bytes[block * 8] = 0xf8;
        bytes[block * 8 + 3] = 0x1f;
        bytes[block * 8 + 4] = 0x1b;
    }
    check(roommaterials::decodeTexture(bytes, 8, 8, 14, texture, error), "CMPR decode");
    check(texture.pixels[0] == 255 && texture.pixels[6] == 255 &&
        texture.pixels[8] == 159 && texture.pixels[10] == 95 &&
        texture.pixels[12] == 95 && texture.pixels[14] == 159, "GameCube CMPR interpolation and selector order");
    check(texture.pixels[16] == 255 && texture.pixels[128] == 255, "CMPR sub-block order");
    bytes.assign(32, 0);
    bytes[3] = 0x1f;
    bytes[4] = 0xc0;
    check(roommaterials::decodeTexture(bytes, 5, 3, 14, texture, error) &&
        texture.pixels.size() == 60 && texture.pixels[3] == 0 && texture.pixels[7] == 255,
        "CMPR transparency and partial tile clipping");
    bytes.resize(31);
    check(!roommaterials::decodeTexture(bytes, 8, 8, 14, texture, error) && texture.pixels.empty(), "Reject truncated tile");
    check(!roommaterials::decodeTexture(bytes, 0, 8, 14, texture, error), "Reject zero dimensions");
    check(!roommaterials::decodeTexture(bytes, 65535, 65535, 14, texture, error), "Reject oversized texture");
    check(!roommaterials::decodeTexture(bytes, 4, 4, 7, texture, error), "Reject unsupported format");
    bytes.assign(64, 0);
    bytes[0] = 0x40;
    bytes[1] = 0xa0;
    bytes[32] = 0x80;
    bytes[33] = 0x30;
    check(roommaterials::decodeTexture(bytes, 5, 3, 3, texture, error) && texture.pixels.size() == 60 &&
        texture.pixels[0] == 0xa0 && texture.pixels[1] == 0xa0 && texture.pixels[2] == 0xa0 && texture.pixels[3] == 0x40 &&
        texture.pixels[16] == 0x30 && texture.pixels[19] == 0x80, "IA8 channel order, tile stride, and partial tile clipping");
    bytes.resize(63);
    check(!roommaterials::decodeTexture(bytes, 5, 3, 3, texture, error), "Reject truncated IA8 tile");
    bytes.assign(64, 0);
    bytes[0] = 0xf1;
    bytes[32] = 0xa0;
    check(roommaterials::decodeTexture(bytes, 16, 8, 0, texture, error) && texture.pixels[0] == 255 &&
        texture.pixels[4] == 17 && texture.pixels[32] == 170 && texture.pixels[3] == 255 && texture.pixels[7] == 17,
        "I4 nibble expansion and tiled order");
    bytes.assign(32, 0);
    bytes[0] = 0xfc;
    bytes[2] = 0x30;
    bytes[3] = 0xf0;
    check(roommaterials::decodeTexture(bytes, 4, 4, 5, texture, error) && texture.pixels[0] == 255 &&
        texture.pixels[1] == 0 && texture.pixels[3] == 255 && texture.pixels[5] == 255 && texture.pixels[7] == 109,
        "RGB5A3 opaque and translucent colors");
    bytes.assign(128, 0);
    bytes[0] = 0xa3;
    bytes[8] = 0x5f;
    bytes[32] = 0x71;
    bytes[64] = 0xc2;
    check(roommaterials::decodeTexture(bytes, 9, 5, 2, texture, error) && texture.pixels[0] == 51 &&
        texture.pixels[3] == 170 && texture.pixels[36] == 255 && texture.pixels[39] == 85 &&
        texture.pixels[32] == 17 && texture.pixels[35] == 119 && texture.pixels[144] == 34,
        "IA4 channels, rectangular tiles, and partial tile clipping");
    bytes.resize(127);
    check(!roommaterials::decodeTexture(bytes, 9, 5, 2, texture, error), "Reject incomplete IA4 tile");
    bytes.assign(64, 0);
    bytes[0] = 97;
    bytes[32] = 201;
    check(roommaterials::decodeTexture(bytes, 8, 8, 1, texture, error) && texture.pixels[0] == 97 &&
        texture.pixels[3] == 97 && texture.pixels[128] == 201 && texture.pixels[131] == 201,
        "I8 intensity, alpha, and vertical tile order");
    bytes.assign(128, 0);
    bytes[0] = 128;
    bytes[1] = 255;
    bytes[32] = 64;
    bytes[33] = 32;
    bytes[64] = 255;
    bytes[65] = 16;
    check(roommaterials::decodeTexture(bytes, 8, 4, 6, texture, error) && texture.pixels[0] == 255 &&
        texture.pixels[1] == 64 && texture.pixels[2] == 32 && texture.pixels[3] == 128 && texture.pixels[16] == 16,
        "RGBA8 split planes and tile stride");
    bytes.resize(32);
    check(!roommaterials::decodeTexture(bytes, 4, 4, 6, texture, error), "Reject incomplete RGBA8 planes");
    roommaterials::Collection materials;
    roomgeometry::Mesh mesh;
    check(!roommaterials::read({}, mesh, materials, error), "Reject missing material header");
    return failures == 0 ? 0 : 1;
}
