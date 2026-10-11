#include "jlib/j3d/j3dmodelhierarchy.h"
#include "jlib/j3d/j3dmodelgeometry.h"
#include "jlib/j3d/j3dmaterialfactory.h"
#include "jlib/j3d/j3dmodelinstance.h"
#include "game/player/playersharedmodels.h"
#include "game/player/playervacuummodel.h"
#include "game/player/playerresourcepools.h"
#include "game/player/playerscannermodel.h"
#include "game/player/playerlightmodel.h"
#include "game/player/playervacuumeffects.h"
#include "game/player/playercontrollerstate.h"
#include "game/player/playermanager.h"
#include "game/player/playergridmodels.h"
#include "game/player/playercomponentstate.h"
#include "game/scene/gamearchive.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
    void checkPlayerPools() {
        struct Motors : playerinput::MotorOutput {
            void command(unsigned port, unsigned value) override {
                ports.push_back(port);
                commands.push_back(value);
            }
            std::vector<unsigned> ports, commands;
        } output;
        playerinput::MotorState motors(output);
        playerinput::Pad pad;
        pad.port = 2;
        {
            playerinput::ControllerState controller(motors, &pad);
            motors.enabled = 0x20000000;
            motors.running[2] = 1;
            controller.feedback.engaged = true;
            controller.feedback.scripts.slots[0].length = 0xa5;
            controller.stop();
            if (output.ports != std::vector<unsigned>{2} || output.commands != std::vector<unsigned>{2} || motors.running[2] ||
                controller.pad != &pad || controller.feedback.scripts.slots[0].length != 0xa5) {
                throw std::runtime_error("Controller stop must preserve pad identity and unwritten pattern fields while stopping its motor.");
            }
            motors.enabled = 0;
            controller.feedback.engaged = true;
        }
        if (output.commands.size() != 1) throw std::runtime_error("Disabled rumble ports must not emit a hardware command.");
        playerresources::PointPool points(0x300);
        if (points.last != points.records.get() + 0x2ff || points.last->field18 != 0) {
            throw std::runtime_error("Original point pool last-record initialization failed.");
        }
        playerresources::EffectPool80 first(8);
        playerresources::EffectPool20 second(4);
        for (unsigned i = 0; i < first.count; ++i) {
            if (first.records[i].field7C != 0) throw std::runtime_error("Original effect pool 80 initialization failed.");
        }
        for (unsigned i = 0; i < second.count; ++i) {
            if (second.records[i].field18 != 0) throw std::runtime_error("Original effect pool 20 initialization failed.");
        }
        for (const auto count : {0u, 1u, 2u, 3u, 4u, 31u, 32u}) {
            playerresources::CurvePoints curve(count);
            const unsigned expected = count < 3 ? 3 : count + !(count & 1);
            if (curve.points.size() != expected || curve.points.back() != std::array<float, 3>{}) {
                throw std::runtime_error("Original curve constructor must round up to an odd count of at least three.");
            }
        }
        playerresources::CurveOwner owner;
        if (owner.primary->points.size() != 33) throw std::runtime_error("Primary curve ownership failed.");
        for (const auto& curve : owner.secondary) {
            if (curve->points.size() != 33 || curve.get() == owner.primary.get()) throw std::runtime_error("Secondary curve ownership failed.");
        }
    }

    void require(bool value, const std::string& message) {
        if (!value) {
            throw std::runtime_error(message);
        }
    }

    unsigned half(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 8) | bytes.at(offset + 1);
    }

    unsigned word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (half(bytes, offset) << 16) | half(bytes, offset + 2);
    }

    void append(std::vector<std::uint8_t>& bytes, unsigned type, unsigned index = 0) {
        bytes.push_back(static_cast<std::uint8_t>(type >> 8));
        bytes.push_back(static_cast<std::uint8_t>(type));
        bytes.push_back(static_cast<std::uint8_t>(index >> 8));
        bytes.push_back(static_cast<std::uint8_t>(index));
    }

    void putHalf(std::vector<std::uint8_t>& bytes, unsigned offset, unsigned value) {
        bytes.at(offset) = static_cast<std::uint8_t>(value >> 8);
        bytes.at(offset + 1) = static_cast<std::uint8_t>(value);
    }

    void putWord(std::vector<std::uint8_t>& bytes, unsigned offset, unsigned value) {
        putHalf(bytes, offset, value >> 16);
        putHalf(bytes, offset + 2, value);
    }

    void checkGeometry() {
        std::vector<std::uint8_t> bytes(384);
        putWord(bytes, 0, 0x4a334432);
        putWord(bytes, 4, 0x626d6433);
        putWord(bytes, 8, 384);
        putWord(bytes, 12, 4);
        putWord(bytes, 32, 0x494e4631);
        putWord(bytes, 36, 48);
        putHalf(bytes, 40, 2);
        putWord(bytes, 44, 1);
        putWord(bytes, 48, 1);
        putWord(bytes, 52, 24);
        for (const auto [offset, type] : {std::pair{56U, 0x10U}, {60U, 1U}, {64U, 0x11U}, {68U, 0x12U}, {72U, 2U}}) {
            putHalf(bytes, offset, type);
        }
        putWord(bytes, 80, 0x4a4e5431);
        putWord(bytes, 84, 160);
        putHalf(bytes, 88, 1);
        putWord(bytes, 92, 32);
        putWord(bytes, 96, 24);
        putHalf(bytes, 104, 1);
        putHalf(bytes, 176, 0x102);
        bytes[178] = 255;
        putWord(bytes, 180, 0x3f800000);
        putWord(bytes, 184, 0x40000000);
        putWord(bytes, 188, 0x40400000);
        putHalf(bytes, 192, 65535);
        putWord(bytes, 240, 0x4d415433);
        putWord(bytes, 244, 24);
        putHalf(bytes, 248, 1);
        putWord(bytes, 264, 0x53485031);
        putWord(bytes, 268, 120);
        putHalf(bytes, 272, 1);
        putWord(bytes, 276, 56);
        putWord(bytes, 280, 44);
        putWord(bytes, 288, 48);
        putWord(bytes, 292, 112);
        putWord(bytes, 296, 116);
        putWord(bytes, 300, 96);
        putWord(bytes, 304, 104);
        bytes[320] = 3;
        putHalf(bytes, 322, 1);
        putHalf(bytes, 360, 7);
        putHalf(bytes, 362, 2);
        putWord(bytes, 368, 4);
        putHalf(bytes, 376, 65535);
        putWord(bytes, 380, 0x90000000);
        j3dmodel::Geometry geometry;
        std::string error;
        require(j3dmodel::readGeometry(bytes, 0x54100000, geometry, error), error);
        require(geometry.flags == 0x54100002 && geometry.joints.size() == 1 && geometry.joints[0].kind == 2 &&
            geometry.joints[0].scaleCompensation == 0 && geometry.joints[0].scale[2] == 3 && geometry.joints[0].rotation[0] == -1,
            "Joint construction uses the declared remap and original narrowing/default rules.");
        const auto& group = geometry.shapes[0]->groups[0];
        require(group.matrixKind == 3 && group.matrix == 7 && group.matrixTable == std::vector<std::uint16_t>{65535, 0} &&
            group.drawOffset == 380 && group.drawSize == 4, "Shape construction retains matrix reuse sentinels and display-list ownership.");
        const auto retained = geometry.bytes;
        putWord(bytes, 372, 0xfffffffe);
        require(!j3dmodel::readGeometry(bytes, 0, geometry, error) && geometry.bytes == retained,
            "Wrapped display-list offsets fail transactionally.");
    }

    void checkNames() {
        std::vector<std::uint8_t> bytes(25);
        putHalf(bytes, 0, 3);
        for (unsigned i = 0; i < 3; ++i) {
            putHalf(bytes, 4 + i * 4, 388);
            putHalf(bytes, 6 + i * 4, 16 + i * 3);
        }
        bytes[16] = bytes[17] = bytes[22] = bytes[23] = 'a';
        bytes[19] = 'b';
        bytes[20] = '^';
        j3dmodel::NameTable names;
        std::string error;
        require(names.read(bytes, 0, bytes.size(), error) && names.index("aa") == 0 && names.index("b^") == 1 &&
            names.index("missing") == -1 && names.name(3) == nullptr,
            "Original name lookup resolves hash collisions, keeps first matches and returns missing sentinels.");
        putHalf(bytes, 4, 0);
        require(names.read(bytes, 0, bytes.size(), error) && names.index("aa") == 2,
            "Original name lookup compares the stored hash before the string.");
        bytes.pop_back();
        require(!names.read(bytes, 0, bytes.size(), error) && names.index("aa") == 2,
            "Unterminated names fail without replacing the owned table.");
    }

    void checkMaterials() {
        j3dmodel::MaterialDefaults defaults;
        defaults.formats.resize(0x190);
        defaults.registers.resize(0x38);
        defaults.colors.resize(0x20);
        defaults.colors[0] = 19;
        defaults.colors[16] = 23;
        for (const bool old : {false, true}) {
            const unsigned stride = old ? 0x138 : 0x14c;
            const unsigned remap = 0x100 + stride, matrix = remap + 4;
            std::vector<std::uint8_t> bytes(matrix + 100);
            putWord(bytes, 0, old ? 0x4d415432 : 0x4d415433);
            putWord(bytes, 4, static_cast<unsigned>(bytes.size()));
            putHalf(bytes, 8, 2);
            putWord(bytes, 12, 0x100);
            putWord(bytes, 16, remap);
            std::fill(bytes.begin() + 0x100, bytes.begin() + remap, 255);
            bytes[0x100] = 1;
            putWord(bytes, old ? 0x34 : 0x40, matrix);
            putHalf(bytes, 0x100 + (old ? 0x34 : 0x48), 0);
            bytes[matrix + 4] = 42;
            j3dmodel::Materials materials;
            std::string error;
            require(j3dmodel::readMaterials(bytes, 0, 0x54300000, defaults, materials, error), error);
            require(materials.uniqueCount == 1 && materials.originals.size() == 1 && materials.entries.size() == 2 &&
                materials.entries[0]->original == &materials.originals[0] && materials.entries[1]->original == &materials.originals[0] &&
                materials.entries[0]->identity == materials.entries[1]->identity,
                "Original remapped materials retain separate instances and shared original identity.");
            auto& first = *materials.entries[0];
            auto& second = *materials.entries[1];
            require(first.color->material[0][0] == (old ? 23 : 19) && first.tev->capacity == 16 && first.tev->count == 255 &&
                first.texture->matrices[0] && second.texture->matrices[0] &&
                first.texture->matrices[0].get() != second.texture->matrices[0].get(),
                "Both original material versions select their defaults and separately allocate remapped matrices.");
            first.texture->matrices[0]->info[4] = 7;
            require(second.texture->matrices[0]->info[4] == 42 && materials.originals[0].texture->matrices[0]->info[4] == 42,
                "Animated material matrices do not alias remapped source records.");
            require(j3dmodel::readMaterials(bytes, 0, 0x01000000, defaults, materials, error), error);
            require(materials.originals.empty() && !materials.entries[0]->color->lighting && !materials.entries[0]->texture->full &&
                materials.entries[0]->indirect->full && materials.entries[0]->tev->capacity == 1 &&
                materials.entries[0]->pixel->kind == j3dmodel::MaterialPixelKind::Full && materials.entries[0]->pixel->alpha == 65535,
                "Original factory flags select the corresponding blocks and absent-state sentinels.");
        }
    }

    void checkOwned(const gamearchive::Archive& archive, const char* name, const j3dmodel::MaterialDefaults& defaults) {
        const auto resource = archive.findTypeResource(0x4b545f53, name);
        require(resource >= 0, std::string("Missing original KT_S resource: ") + name);
        const auto bytes = archive.resource(static_cast<unsigned>(resource));
        require(word(bytes, 0) == 0x4a334432, "Original shared model uses J3D2.");
        unsigned offset = 32, hierarchyOffset = 0, joints = 0, materials = 0, shapes = 0;
        for (unsigned i = 0; i < word(bytes, 12); ++i) {
            const auto type = word(bytes, offset);
            const auto size = word(bytes, offset + 4);
            require(size >= 8 && size <= bytes.size() - offset, "Owned BMD block range.");
            if (type == 0x494e4631) {
                hierarchyOffset = offset + word(bytes, offset + 20);
            } else if (type == 0x4a4e5431) {
                joints = half(bytes, offset + 8);
            } else if (type == 0x4d415432 || type == 0x4d415433) {
                materials = half(bytes, offset + 8);
            } else if (type == 0x53485031) {
                shapes = half(bytes, offset + 8);
            }
            offset += size;
        }
        require(hierarchyOffset && joints && materials && shapes, "Owned BMD declares model construction counts.");
        j3dmodel::Hierarchy hierarchy;
        std::string error;
        require(j3dmodel::readHierarchy(bytes, hierarchyOffset, joints, materials, shapes, hierarchy, error), error);
        require(hierarchy.root >= 0, "Owned model has its original hierarchy root.");
        j3dmodel::Geometry geometry;
        require(j3dmodel::readGeometry(bytes, 0x54100000, geometry, error), error);
        j3dmodel::Materials materialData;
        require(j3dmodel::readMaterials(bytes, geometry.materialOffset, 0x54100000, defaults, materialData, error), error);
        require(materialData.entries.size() == materials && materialData.originals.empty(), "Shared model material construction count and ownership.");
        for (unsigned i = 0; i < materials; ++i) {
            const auto& material = *materialData.entries[i];
            require(material.index == i && material.identity == 0xc0000000 && !material.original && material.color->lighting &&
                material.texture->full && material.tev->capacity == 16 && !material.indirect->full,
                "Shared model uses the original material factory flags.");
        }
        auto data = std::make_shared<j3dmodel::ModelData>();
        require(data->read(bytes, 0x54100000, defaults, error), error);
        j3dmodel::prepareMaterialAnimations(*data);
        std::vector<const char*> animations;
        const std::string modelName = name;
        if (modelName == "suikom_new1.bmd") animations = {"suikom_new1.btk", "suikom_new1.bpk"};
        if (modelName == "handlight_ef.bmd") animations = {"handlight_ef.btk"};
        if (modelName == "bedcv_1.bmd") animations = {"bedcv_1.bpk"};
        if (modelName == "scana.bmd") animations = {"scana.btk"};
        if (modelName == "warp.bmd") animations = {"warp.bck", "warp01.btk", "warp02.btk", "warp.brk"};
        for (const auto* animationName : animations) {
            const auto animationResource = archive.findTypeResource(0x4b545f53, animationName);
            require(animationResource >= 0, std::string("Missing original shared animation: ") + animationName);
            auto animation = std::make_shared<j3dmodel::Animation>();
            require(animation->read(archive.resource(static_cast<unsigned>(animationResource)), error), error);
            require(j3dmodel::bindAnimation(*data, animation) == 0, "Original animation binds to its prepared material owner.");
        }
        {
            j3dmodel::ModelInstance instance(data, 0x10000);
            require(instance.shapes.size() == shapes && instance.materials.size() == materials && instance.jointMatrices &&
                instance.jointScaleFlags && instance.positions[0] == geometry.vertexOffsets[0],
                "Original model instance owns its matrix workspaces and packets.");
            for (unsigned i = 0; i < materials; ++i) {
                const auto& packet = instance.materials[i];
                require(packet.material == data->materials.entries[i].get() && packet.shapes && packet.shapes->shape &&
                    packet.displayListCapacity == 0 && packet.material->flags == 1,
                    "Original shared model packets attach manager-owned materials and shapes.");
            }
        }
        {
            j3dmodel::ModelInstance instance(data, 0);
            for (const auto& packet : instance.materials) {
                require(packet.displayListCapacity && packet.displayListSize == 0 &&
                    packet.displayLists[0].data() != packet.displayLists[1].data() &&
                    (reinterpret_cast<std::uintptr_t>(packet.displayLists[0].data()) & 31) == 0 &&
                    (reinterpret_cast<std::uintptr_t>(packet.displayLists[1].data()) & 31) == 0,
                    "Original display-list path owns two separately aligned buffers.");
            }
        }
        require(geometry.joints.size() == joints && geometry.shapes.size() == shapes &&
            geometry.hierarchy.materials.size() == materials, "Original geometry construction counts.");
        require(geometry.materialNames && geometry.materialNames->size() == materials,
            "Original materials retain their animation binding names.");
        for (unsigned i = 0; i < materials; ++i) {
            const auto* name = geometry.materialNames->name(static_cast<std::uint16_t>(i));
            require(name && geometry.materialNames->index(*name) >= 0, "Original material names resolve using their stored hash.");
        }
        for (const auto& material : hierarchy.materials) {
            require(material.shape >= 0 && hierarchy.boundShapes.at(material.shape), "Owned material resolves its bound shape.");
            const auto& shape = geometry.shapes.at(material.shape);
            require(shape && !shape->groups.empty(), "Bound original shape owns draw groups.");
            for (const auto& group : shape->groups) {
                require(group.drawOffset <= bytes.size() && group.drawSize <= bytes.size() - group.drawOffset,
                    "Original shape display list remains inside the owned resource.");
            }
        }
        auto truncated = bytes;
        truncated.resize(32);
        const auto retained = geometry.bytes;
        require(!j3dmodel::readGeometry(truncated, 0, geometry, error) && geometry.bytes == retained,
            "Failed geometry construction retains the previous owned resource.");
        std::cout << name << ": " << joints << " joints, " << materials << " materials, " << shapes << " shapes.\n";
    }
}

int main(int argc, char** argv) {
    try {
        checkPlayerPools();
        checkGeometry();
        checkNames();
        checkMaterials();
        require(j3dmodel::NameTable::key(std::string(1, static_cast<char>(0xff))) == 65535,
            "Original name hashing sign-extends bytes before accumulating.");
        std::vector<std::uint8_t> bytes;
        append(bytes, 0x10, 0);
        append(bytes, 1);
        append(bytes, 0x11, 0);
        append(bytes, 0x12, 0);
        append(bytes, 0x11, 1);
        append(bytes, 0x12, 1);
        append(bytes, 0x10, 1);
        append(bytes, 1);
        append(bytes, 0x11, 2);
        append(bytes, 0x12, 2);
        append(bytes, 2);
        append(bytes, 0x10, 2);
        append(bytes, 2);
        append(bytes, 0);
        j3dmodel::Hierarchy hierarchy;
        std::string error;
        require(j3dmodel::readHierarchy(bytes, 0, 3, 3, 3, hierarchy, error), error);
        require(hierarchy.root == 0 && hierarchy.joints[0].child == 1 && hierarchy.joints[1].sibling == 2,
            "Original child insertion appends siblings.");
        require(hierarchy.joints[0].material == 1 && hierarchy.materials[1].next == 0 &&
            hierarchy.materials[0].shape == 0 && hierarchy.materials[1].shape == 1 &&
            hierarchy.joints[1].material == 2 && hierarchy.materials[2].shape == 2,
            "Original materials prepend to the parent joint; shapes bind to the current head material.");
        bytes[0] = 0x7f;
        require(!j3dmodel::readHierarchy(bytes, 0, 3, 3, 3, hierarchy, error) && hierarchy.root == 0,
            "Malformed hierarchy fails without replacing the existing links.");
        bytes.clear();
        for (unsigned i = 0; i < 4096; ++i) {
            append(bytes, 1);
        }
        for (unsigned i = 0; i < 4096; ++i) {
            append(bytes, 2);
        }
        append(bytes, 0);
        require(j3dmodel::readHierarchy(bytes, 0, 0, 0, 0, hierarchy, error),
            "Empty original hierarchy groups do not consume declared nodes or native recursion stack.");
        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> compressed, common, executable;
            gamearchive::Archive archive;
            require(package.open(argv[1], error) && package.read("files/Game/game_usa.szp", compressed, error) &&
                roomarchive::decompress(compressed, common, error) && archive.read(common, error), error);
            j3dmodel::MaterialDefaults defaults;
            require(package.read("sys/main.dol", executable, error) && defaults.read(executable, error), error);
            {
                playerinput::WindowsMotorOutput output;
                playerinput::MotorState motors(output);
                players::Globals globals;
                std::weak_ptr<j3dmodel::ModelData> retained;
                {
                    players::Manager manager(archive, defaults, 2, nullptr, globals, motors);
                    for (unsigned preset : {12u, 13u, 14u}) {
                        auto& controller = *manager.controllers[0];
                        require(controller.play(executable, preset), "Original suction feedback preset must allocate in its manager-owned controller.");
                        require(controller.feedback.scripts.slots[0].script.identity != 0 &&
                            !controller.feedback.scripts.slots[0].script.commands.empty(), "Feedback preset must retain imported identity and commands.");
                        for (unsigned frame = 0; frame < 120; ++frame) controller.update(frame);
                        require(!controller.feedback.scripts.slots[0].active, "Unrefreshed original suction feedback must finish its script.");
                        controller.stop();
                    }
                    require(manager.controllers[0]->play(executable, 1) && manager.controllers[0]->play(executable, 6) &&
                        manager.controllers[0]->feedback.scripts.slots[0].script.identity ==
                        manager.controllers[0]->feedback.scripts.slots[1].script.identity,
                        "Aliased preset table entries must retain shared original identity.");
                    manager.controllers[0]->stop();
                    require(manager.points->count == 0x300 && manager.effects80->count == 8 && manager.effects20->count == 4 &&
                        manager.records->count == 32 && !manager.entries && !manager.auxiliary,
                        "Original manager first stage constructs its pools before later owners.");
                    manager.initializeEntries();
                    require(manager.entries && manager.auxiliaryState && !manager.auxiliary,
                        "Original manager second stage constructs six entries and auxiliary state.");
                    manager.initializeAuxiliary(executable);
                    auto* auxiliary = manager.auxiliary.get();
                    manager.initializeAuxiliary(executable);
                    require(manager.auxiliary.get() == auxiliary && auxiliary->weapon->pipe->count == 15 && auxiliary->weapon->fieldC == 2,
                        "Original post-actor auxiliary owner is constructed only once.");
                    retained = auxiliary->weapon->pipe->instance->model;
                    {
                        auto& pipe = *auxiliary->weapon->pipe;
                        require(pipe.pulse(0, 2) == 0 && pipe.pulse(-1, 2) == 0 && !pipe.sections[0].state,
                            "Nonpositive pipe pulses do not consume a section.");
                        for (unsigned i = 0; i < 4; ++i) require(pipe.pulse(2, 0.5) == 7 && pipe.sections[i].state == 1 &&
                            pipe.sections[i].field4 == std::array<float, 3>{0, 2, 0.5f},
                            "Pipe pulses claim the first free section and truncate the original duration.");
                        require(pipe.pulse(1, 3) == 0, "A full original pipe pulse pool rejects additional pulses.");
                        for (auto& section : pipe.sections) section.state = 0;
                    }
                    {
                        playermodels::GridOwner grids(*manager.models);
                        playercomponents::Vacuum vacuum(*manager.models, globals.vacuum);
                        require(vacuum.model->count == 10 && globals.vacuum.field8C18 == 13 &&
                            globals.vacuum.parameters[0].field64 == 0 && globals.vacuum.parameters[1].field64 == 70,
                            "Player vacuum constructor retains its separate original shared settings.");
                        for (const auto& grid : grids.grids) {
                            require(grid->count == 256 && grid->width == 16 && grid->height == 16 &&
                                grid->points[255].field44 == std::array<float, 3>{0, 1, 0},
                                "Each player grid initializes all 256 original point records.");
                            for (unsigned i = 0; i < 3; ++i) {
                                require(grid->materials[i] && grid->materials[i]->texture->matrices[grid->slots[i]],
                                    "Grid constructor selects an existing original material matrix.");
                            }
                        }
                    }
                    for (unsigned i = 0; i < 15; ++i) require(auxiliary->weapon->pipe->points[i].position == std::array<float, 3>{},
                        "Original weapon constructor initializes the hose between its two initial part positions.");
                    for (const auto actorType : {-1, 0, 1, 2, 3, 4, 0x27, 0x28, 0xb3}) {
                        for (const auto partType : {-1, 5, 6, 7, 8, 9, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9}) {
                            playerweapons::Part part(executable, actorType, partType);
                            require(part.position == std::array<float, 3>{} && part.field610 == std::array<float, 3>{1, 0, 0},
                                "Every original weapon descriptor constructs and resets its part state.");
                        }
                    }
                }
                require(retained.expired(), "Manager teardown releases nested hose model ownership.");
            }
            std::weak_ptr<j3dmodel::ModelData> retainedModel;
            std::weak_ptr<j3dmodel::Animation> retainedAnimation;
            {
                playermodels::SharedModels shared(archive, defaults, 2, nullptr);
                retainedModel = shared.model(playermodels::SharedModel::Warp);
                retainedAnimation = shared.animation(7);
                {
                    playermodels::VacuumEffectGlobals globals;
                    globals.parameters[0].field20 = 731;
                    playermodels::VacuumEntries entries(shared, globals);
                    require(globals.parameters[0].field20 == 731 && globals.parameters[0].field24[0] == 0.36f &&
                        globals.parameters[1].field14 == 4 && globals.parameters[1].field54 == 30,
                        "Nested effect constructors update only the original shared parameter fields.");
                    for (const auto& entry : entries.entries) {
                        require(entry->effect->model->data == shared.model(playermodels::SharedModel::Vacuum) &&
                            entry->tracking.field2C == 40 && entry->tracking.fieldC[2] == 1,
                            "All six entries construct their original nested vacuum owner and tracking state.");
                    }
                    entries.fieldC98 = 42;
                    entries.fieldBB8.fill(7);
                    entries.reset();
                    require(entries.fieldC98 == 42 && entries.fieldBB8[0] == 0 && entries.fieldA38[0] == nullptr,
                        "Original entry reset preserves field C98 outside its reset range.");
                }
                {
                    gameplaycamera::Owners cameras;
                    playermodels::ScannerModels scanner(shared, *cameras.cameras[0]);
                    playermodels::LightModel light(shared, *cameras.cameras[0]);
                    require(scanner.camera == cameras.active && light.camera == cameras.active,
                        "Scanner and light retain the original first camera owner.");
                    require(scanner.scanner->model == shared.model(playermodels::SharedModel::Scanner) &&
                        scanner.warp->model == shared.model(playermodels::SharedModel::Warp), "Scanner owns both original model instances.");
                    unsigned matrixCount = 0;
                    for (unsigned i = 0; i < light.materials.size(); ++i) {
                        if (!light.materials[i]) continue;
                        ++matrixCount;
                        require(light.matrices[i]->info[1] == 3 && std::equal(light.originalMatrices[i].begin(),
                            light.originalMatrices[i].end(), light.matrices[i]->info.begin() + 0x24),
                            "Light preserves each selected material matrix for later restoration.");
                    }
                    require(matrixCount != 0, "Owned light model supplies original projection matrices.");
                }
                {
                    playermodels::VacuumModel vacuum(shared, 9, 0);
                    require(vacuum.count == 9 && vacuum.interval == 0.125f && vacuum.instance && vacuum.segments,
                        "Original vacuum constructor owns its model and eight segments.");
                    require(vacuum.data == shared.model(playermodels::SharedModel::Vacuum) &&
                        vacuum.textureAnimation == shared.animation(0) && vacuum.colorAnimation == shared.animation(1),
                        "Vacuum instance retains the shared owner resources.");
                    require(*vacuum.data->geometry.jointNames->name(vacuum.joint) == "chn15",
                        "Vacuum constructor searches joint names in original order.");
                    for (unsigned i = 0; i < vacuum.count; ++i) {
                        require(vacuum.points[i].position == std::array<float, 3>{} && vacuum.points[i].previous == std::array<float, 3>{}
                            && vacuum.points[i].field18 == 0, "Vacuum base initializes every point.");
                    }
                    require(vacuum.parameters.field20 == 500 && vacuum.parameters.field24[0] == 0.5f &&
                        vacuum.parameters.field64 == 255, "Vacuum parameters retain original defaults.");
                }
                require(!shared.model(playermodels::SharedModel::Koopa) && !shared.animation(9) && !shared.animation(10),
                    "Original non-Koopa mode leaves the three conditional resources absent.");
                for (unsigned i = 0; i < 16; ++i) require(shared.texture(i) && !shared.texture(i)->empty(), "Shared owner retains every original BTI.");
                for (unsigned i = 0; i < 12; ++i) require(shared.buffer(i).entries.size() == 256 && shared.buffer(i).entries[0] == nullptr,
                    "Original shared draw buffers have 256 cleared buckets.");
                const auto& warp = *shared.model(playermodels::SharedModel::Warp);
                require(warp.jointCalculators[0] && warp.jointCalculators[0]->animation == shared.animation(5),
                    "Warp BCK is attached to the original first-joint calculator.");
                bool found = false;
                for (const auto& material : warp.materials.entries) {
                    for (const auto& binding : material->animation->matrices) {
                        if (binding && binding->animation == shared.animation(7)) found = true;
                    }
                }
                require(found, "Original shared owner connects warp02 after warp01.");
                for (unsigned section = 0; section < 2; ++section) {
                    const auto& group = shared.animation(7)->groups[section];
                    for (unsigned i = 0; i < group.materials.size(); ++i) {
                        if (group.slots[i] == 255) continue;
                        const auto& target = *warp.materials.entries.at(group.materials[i])->animation;
                        const auto& binding = section ? target.postMatrices.at(group.slots[i]) : target.matrices.at(group.slots[i]);
                        require(binding && binding->animation == shared.animation(7) && binding->index == i,
                            "Second warp BTK replaces each addressed matrix binding.");
                    }
                }
            }
            require(retainedModel.expired() && retainedAnimation.expired(), "Shared model ownership releases the full dependency graph without cycles.");
            for (const auto* name : {"suikom_new1.bmd", "handlight_ef.bmd", "pipe.bmd", "coin.bmd", "osatu.bmd",
                "osatu2.bmd", "osatu3.bmd", "leaf.bmd", "pile.bmd", "bed.bmd", "bedcv_1.bmd", "scana.bmd", "warp.bmd"}) {
                checkOwned(archive, name, defaults);
            }
        }
        std::cout << "J3D hierarchy construction checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
