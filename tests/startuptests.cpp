#include "resources/dolresources.h"
#include "game/startup/startupscene.h"
#include "game/startup/scenetransition.h"

#include <cmath>
#include <iostream>

namespace {
    void write32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) data[offset + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }

    void enterTitle(startupscene::Scene& scene) {
        scene.reset();
        scene.update(true, false);
        scene.update(true, false);
        scene.update(true, false);
        scene.update(true, false);
        scene.update(true, false);
    }
}

int main(int argc, char** argv) {
    unsigned failures = 0;
    const auto check = [&](bool condition, const char* name) {
        if (!condition) { std::cerr << name << '\n'; ++failures; }
    };
    startupscene::Scene scene;
    scenetransition::OutgoingFade transition;
    check(!transition.active() && !transition.finished(), "No outgoing fade before a scene exit");
    transition.begin();
    for (unsigned tick = 0; tick <= 30; ++tick) {
        check(transition.alpha() == tick * 255 / 30, "Original integer fade alpha");
        check(transition.finished() == (tick == 30), "Fade completes at 30 VI ticks");
        transition.update();
    }
    check(transition.alpha() == 255, "Completed fade remains black");
    transition.reset();
    transition.begin();
    for (unsigned frame = 0; frame < 15; ++frame) transition.update(2);
    check(transition.finished() && transition.alpha() == 255, "30 fps presentation preserves fade duration");
    transition.reset();
    check(!transition.active() && transition.alpha() == 0, "Restart clears outgoing fade");
    startupresources::Resources initialResources;
    initialResources.panes = {{"yski", 0, 0, 640, 480}, {"logo", 160, 131, 320, 160}, {"copy", 235, 415, 240, 16}};
    const auto checkTitleBlack = [&] {
        for (const auto& picture : scene.pictures(initialResources)) check(picture.alpha == 0, "No opaque title panes before the first fade update");
    };
    for (unsigned frame = 0; frame < 1000; ++frame) scene.update(false, true);
    check(scene.isBoot() && scene.requestedScene() == -1, "Boot must wait for audio completion and ignore Start");
    scene.update(true, false);
    check(scene.isBoot(), "Boot completion must advance its state before leaving");
    scene.update(true, false);
    check(!scene.isBoot() && scene.takeSoundRequest() == 0x80000021 && scene.takeSoundRequest() == 0, "Title music request on entry");
    checkTitleBlack();
    scene.update(true, false);
    checkTitleBlack();
    scene.update(true, true, false);
    for (unsigned frame = 0; frame < 100; ++frame) scene.update(true, true, false);
    check(scene.requestedScene() == -1, "Title input must not bypass pending initialization");
    scene.update(true, false, true);
    scene.update(true, true);
    for (unsigned frame = 0; frame < 59; ++frame) scene.update(true, false);
    check(scene.requestedScene() == -1, "Latched Start must wait for all 60 ready updates");
    scene.update(true, false);
    check(scene.requestedScene() == 3 && scene.takeSoundRequest() == 0x3802, "Original selection request and sound ID");

    enterTitle(scene);
    for (unsigned frame = 0; frame < 253; ++frame) scene.update(true, false);
    check(scene.titleState() == 0, "Background fade before its 255th UI update");
    scene.update(true, false);
    check(scene.titleState() == 1, "Background fade boundary");
    for (unsigned frame = 0; frame < 63; ++frame) scene.update(true, false);
    check(scene.titleState() == 1, "Logo reveal has not finished at fade 252");
    scene.update(true, false);
    check(scene.titleState() == 2, "Logo reveal ends on the 64th update");
    for (unsigned frame = 0; frame < 255; ++frame) scene.update(true, false);
    check(scene.titleState() == 3, "Start prompt pulse reverses at 255");
    for (unsigned frame = 0; frame < 255; ++frame) scene.update(true, false);
    check(scene.titleState() == 4, "Start prompt pulse reverses at zero");

    enterTitle(scene);
    for (unsigned frame = 0; frame < 1797; ++frame) scene.update(true, false);
    check(scene.requestedScene() == -1, "Title timeout must not fire early");
    scene.update(true, false);
    check(scene.requestedScene() == 4, "Original title timeout requests scene 4 at update 1800");

    std::string error;
    std::vector<std::uint8_t> data(0x120), output;
    write32(data, 0, 0x100);
    write32(data, 0x48, 0x80200000);
    write32(data, 0x90, 0x20);
    check(dolresources::read(data, 0x80200010, 16, output, error) && output.size() == 16, "DOL range ending exactly at section boundary");
    check(!dolresources::read(data, 0x80200010, 17, output, error) && output.empty(), "Reject DOL section crossing");
    check(!dolresources::read(data, 0x80200020, 1, output, error), "Reject unmapped DOL range");
    data.resize(0x110);
    check(!dolresources::read(data, 0x80200000, 1, output, error), "Reject truncated DOL section");
    roommaterials::Texture texture;
    data.assign(64, 0xff);
    data[0] = 0;
    data[2] = data[4] = data[8] = 0;
    data[3] = data[5] = 8;
    write32(data, 28, 32);
    data[32] = 0x08;
    check(startupresources::decodeTexture(data, texture, error) && texture.pixels[3] == 0 && texture.pixels[7] == 136,
        "I4 startup textures use intensity as alpha");
    data.pop_back();
    check(!startupresources::decodeTexture(data, texture, error), "Reject truncated BTI payload");

    if (argc == 2) {
        startupresources::Resources resources;
        const bool loaded = startupresources::read(argv[1], resources, error);
        check(loaded, error.c_str());
        if (loaded) {
            check(resources.textures.size() == 7 && resources.panes.size() == 6, "Original title picture and texture count");
            check(resources.textures[0].width == 640 && resources.textures[0].height == 480, "Original Nintendo picture dimensions");
            check(resources.logoSamples.size() == 63840, "Original interleaved PCM length");
            check(resources.sine.size() == 4096 && std::abs(resources.sine[1024] - 1.0f) < 0.00001f, "Original sine table quarter turn");
            enterTitle(scene);
            for (unsigned frame = 0; frame < 286; ++frame) scene.update(true, false);
            const auto reveal = scene.pictures(resources);
            bool foundLogo = false;
            for (const auto& picture : reveal) {
                if (picture.texture == resources.panes[2].texture) {
                    foundLogo = true;
                    check(picture.height == 113 && picture.y == 178, "Original half-reveal uses the four-float table stride");
                }
            }
            check(foundLogo, "Logo is visible during reveal");
            enterTitle(scene);
            scene.update(true, true);
            const auto pictures = scene.pictures(resources);
            check(pictures.size() == 4 && std::abs(pictures.back().alpha - 241.0f / 255) < 0.00001f, "Start skips title fades to original alpha 241");
        }
    }
    if (failures == 0) std::cout << "Startup checks passed.\n";
    return failures == 0 ? 0 : 1;
}
