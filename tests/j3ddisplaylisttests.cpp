#include "j3dmaterialloadtarget.h"
#include "jlib/j3d/j3dmaterialdisplaylist.h"
#include "jlib/j3d/j3ddrawbuffer.h"

#include <iostream>
#include <stdexcept>

namespace {
    void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

    j3dmodel::Material makeMaterial() {
        j3dmodel::Material material;
        material.identity = std::uintptr_t(1) << 40;
        material.tev = std::make_unique<j3dmodel::MaterialTevBlock>();
        material.tev->capacity = 1; material.tev->count = 0;
        material.tev->textures = {65535}; material.tev->stages.resize(1);
        material.indirect = std::make_unique<j3dmodel::MaterialIndirectBlock>();
        material.pixel = std::make_unique<j3dmodel::MaterialPixelBlock>();
        material.pixel->blend = {1, 4, 5, 3};
        material.pixel->fog = std::make_unique<std::array<std::uint8_t, 44>>();
        (*material.pixel->fog)[0] = 7;
        material.color = std::make_unique<j3dmodel::MaterialColorBlock>();
        material.texture = std::make_unique<j3dmodel::MaterialTextureBlock>();
        material.texture->matrices[0] = std::make_unique<j3dmodel::TextureMatrix>();
        material.texture->matrices[0]->matrix.fill(3);
        material.texture->matrices[0]->info[0] = 1;
        return material;
    }

    struct Draw : j3dmodel::PacketDrawTarget {
        materialtests::Target& target;
        std::vector<j3dmodel::ShapePacket*> shapes;
        explicit Draw(materialtests::Target& target) : target(target) {}
        void select(j3dmodel::MaterialPacket&) override { target.calls.push_back("select"); }
        void flush() override { target.calls.push_back("flush"); }
        void shape(j3dmodel::ShapePacket& packet) override { target.calls.push_back("shape"); shapes.push_back(&packet); }
    };
}

int main() {
    try {
        auto material = makeMaterial();
        j3dmodel::MaterialPacket packet;
        packet.material = &material;
        packet.displayLists[0] = j3dmodel::PacketBuffer(32);
        packet.displayLists[1] = j3dmodel::PacketBuffer(32);
        const auto firstBuffer = packet.displayLists[0].data();
        j3dmodel::MaterialLight light;
        light.position = {1, 2, 3};
        material.color->lighting = true;
        material.color->lights[0] = &light;
        j3dmodel::updateMaterialPacket(packet);
        auto first = packet.nativeDisplayLists[0];
        check(first && first->commandCount() && packet.displayLists[1].data() == firstBuffer &&
            packet.materialIdentity == material.identity, "Recording swaps owned buffers and preserves full-width material identity.");
        material.pixel->blend = {0, 1, 0, 3};
        (*material.pixel->fog)[0] = 9;
        material.texture->matrices[0]->matrix.fill(8);
        material.texture->nbtScale[0] = 1;
        material.mode = 4;
        light.position[0] = 99;
        materialtests::Target target;
        j3dmodel::loadMaterialPacket(packet, 0, target);
        check(target.calls.front() == "mode:4" && target.calls.back() == "nbt" && target.pixels.blend[0] == 1 &&
            (*target.pixels.fog)[0] == 7 && target.matrix[0] == 3 && target.lights[0].position[0] == 1 &&
            target.scale == &material.texture->nbtScale,
            "Replay uses owned value snapshots while mode and NBT are read at draw time.");
        packet.flags = 1;
        material.identity += 1;
        j3dmodel::updateMaterialPacket(packet);
        check(packet.nativeDisplayLists[0] == first && packet.materialIdentity != material.identity,
            "Locked recording preserves both commands and prior identity.");
        packet.flags = 0;
        j3dmodel::updateMaterialPacket(packet);
        check(packet.nativeDisplayLists[1] == first && packet.displayLists[0].data() == firstBuffer,
            "Second recording swaps back and retains the preceding native list.");
        target.calls.clear();
        j3dmodel::loadMaterialPacket(packet, 0, target);
        check(target.pixels.blend[0] == 0 && (*target.pixels.fog)[0] == 9 && target.matrix[0] == 8,
            "Unlocked rerecording captures current values.");
        j3dmodel::ShapePacket a, b;
        packet.shapes = &a; a.next = &b;
        Draw draw(target);
        target.calls.clear();
        j3dmodel::drawMaterialPacket(packet, 0, 4, target, draw);
        check(target.calls.front() == "select" && target.calls[target.calls.size() - 3] == "flush" &&
            draw.shapes == std::vector<j3dmodel::ShapePacket*>{&a, &b},
            "Packet selection and material replay precede flush and linked shape submission.");
        target.calls.clear(); draw.shapes.clear();
        j3dmodel::drawMaterialPacket(packet, 0, 1, target, draw);
        check(target.calls.back() == "flush" && draw.shapes.empty(), "Mode rejection still loads and flushes material state.");
        target.calls.clear();
        j3dmodel::drawMaterialPacket(packet, 2, 4, target, draw);
        check(target.calls == std::vector<std::string>{"select", "mode:4", "flush", "shape", "shape"},
            "Material suppression does not suppress flush or otherwise admitted shapes.");
        material.flags = 1; packet.flags = 1;
        material.pixel->blend[0] = 3;
        const auto recorded = packet.nativeDisplayLists[0];
        j3dmodel::updateMaterialPacket(packet);
        check(packet.nativeDisplayLists[0] == recorded && packet.materialIdentity == material.identity,
            "Shared-material route refreshes identity without recording even when packet is locked.");
        j3dmodel::loadMaterialPacket(packet, 0, target);
        check(target.pixels.blend[0] == 3, "Shared-material draw observes current values instead of the recorded snapshot.");
        j3dmodel::DrawBuffer buffer(8);
        j3dmodel::MaterialPacket other;
        j3dmodel::ShapePacket otherShape;
        other.shapes = &otherShape; other.identityFlags = packet.identityFlags = 0;
        other.materialIdentity = packet.materialIdentity + (std::uintptr_t(1) << 32);
        check(buffer.entryMaterial(packet, 0) && buffer.entryMaterial(other, 0),
            "Distinct host identities with identical low 32 bits cannot merge.");
        material = makeMaterial();
        target.calls.clear(); first->replay(target);
        check(target.matrix[0] == 3 && (*target.pixels.fog)[0] == 7,
            "Recorded values outlive the source material and its replaced resources.");
        std::cout << "Material recording, replay, locks and packet submission verified.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
