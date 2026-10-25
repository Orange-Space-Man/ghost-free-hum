#include "j3dmaterialloadtarget.h"

#include <iostream>
#include <stdexcept>

namespace {
    void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
}

int main() {
    try {
        materialtests::Target target;
        j3dmodel::MaterialColorBlock color;
        color.count = 2;
        color.channels = {0xffff, 0, 0x400, 0x600};
        j3dmodel::loadColorBlock(color, target);
        check(target.calls == std::vector<std::string>{"material:0", "material:1", "channels:2", "channel:0",
            "channel:2", "channel:1", "channel:3"}, "Unlit color loading preserves original interleaved channel IDs and cull sentinel.");
        const auto& control = target.channels[0];
        check(control.enabled == 1 && control.ambientSource == 1 && control.materialSource == 1 &&
            control.lights == 255 && control.diffuse == 3 && control.attenuation == 1 &&
            target.channels[1].attenuation == 2 && target.channels[2].attenuation == 2,
            "Packed color control and original attenuation table decode without treating 65535 as absent.");
        target.calls.clear();
        j3dmodel::MaterialLight light;
        light.position = {1, 2, 3}; light.direction = {4, 5, 6}; light.attenuation = {1, 2, 3, 4, 5, 6};
        color.lighting = true; color.lights[7] = &light; color.cull = 2;
        j3dmodel::loadColorBlock(color, target);
        check(target.calls[2] == "ambient:0" && target.calls[3] == "ambient:1" &&
            target.calls[target.calls.size() - 2] == "light:7" && target.calls.back() == "cull:2" &&
            target.lights[7].direction == light.direction, "Lit blocks add ambient colors and nonnull light inputs before cull.");
        j3dmodel::MaterialTextureBlock texture;
        texture.count = 2;
        texture.coordinates[0].info = {1, 4, 30};
        texture.matrices[9] = std::make_unique<j3dmodel::TextureMatrix>();
        texture.matrices[9]->matrix.fill(2);
        texture.matrices[9]->info[0] = 1;
        target.calls.clear();
        j3dmodel::loadTextureBlock(texture, target);
        check(target.calls == std::vector<std::string>{"textures:2", "coordinate:0", "coordinate:1", "matrix:57"} &&
            target.coordinates[0] == std::array<unsigned, 5>{1, 4, 30, 0, 125} && target.projection == 1,
            "Basic texture load visits all matrix slots and retains two-row projection information.");
        texture.full = true;
        texture.postCoordinates[0].info = {0, 1, 60}; texture.postCoordinates[0].matrix = 0x0140;
        texture.postMatrices[19] = std::make_unique<j3dmodel::TextureMatrix>();
        texture.postMatrices[19]->matrix.fill(7);
        target.calls.clear();
        j3dmodel::loadTextureBlock(texture, target);
        check(target.calls == std::vector<std::string>{"textures:2", "coordinate:0", "coordinate:1", "coordinate:0",
            "matrix:57", "matrix:121"} && target.coordinates[0] == std::array<unsigned, 5>{0, 1, 60, 1, 64} &&
            target.coordinates[1][4] == 125 && target.matrix[0] == 7,
            "Full load replaces only present post coordinates and loads post matrices after primary matrices.");
        j3dmodel::MaterialIndirectBlock indirect;
        target.calls.clear(); j3dmodel::loadIndirectBlock(indirect, target);
        check(target.calls.empty(), "Null indirect blocks leave retained indirect state untouched.");
        indirect.full = true; indirect.count = 2;
        indirect.matrices[1][0] = 0x3f; indirect.matrices[1][1] = 0x80; indirect.matrices[1][24] = 0xfd;
        j3dmodel::loadIndirectBlock(indirect, target);
        check(target.calls == std::vector<std::string>{"indirect:2", "indirectMatrix:1", "indirectMatrix:2",
            "indirectScale:0", "indirectScale:1", "indirectOrder:0", "indirectOrder:1"} &&
            target.indirect[0] == 1 && target.exponent == -3, "Indirect loads retain matrix/scale/order phases and signed exponent.");
        j3dmodel::Material material;
        material.tev = std::make_unique<j3dmodel::MaterialTevBlock>();
        material.tev->capacity = 1; material.tev->count = 0; material.tev->textures = {65535}; material.tev->stages.resize(1);
        material.indirect = std::make_unique<j3dmodel::MaterialIndirectBlock>(); material.indirect->full = true;
        material.pixel = std::make_unique<j3dmodel::MaterialPixelBlock>();
        material.color = std::make_unique<j3dmodel::MaterialColorBlock>();
        material.texture = std::make_unique<j3dmodel::MaterialTextureBlock>(); material.texture->nbtScale[0] = 1;
        target.calls.clear(); j3dmodel::loadMaterialDirect(material, 0, target);
        check(target.calls == std::vector<std::string>{"mode:1", "tev", "indirect:0", "pixel", "material:0", "material:1",
            "channels:0", "textures:0", "nbt"} && target.scale == &material.texture->nbtScale,
            "Direct material loading preserves original block order and retains the owned NBT pointer.");
        material.texture->nbtScale[0] = 2;
        j3dmodel::loadMaterialDirect(material, 0, target);
        check(!target.scale, "Only NBT enable value one installs the scale pointer.");
        target.calls.clear(); j3dmodel::loadMaterialDirect(material, 2, target);
        check(target.calls == std::vector<std::string>{"mode:1"}, "Original suppression flag writes material mode without changing GPU or NBT state.");
        std::cout << "Original material block loading and submission order verified.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
