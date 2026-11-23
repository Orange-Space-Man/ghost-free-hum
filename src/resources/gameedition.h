#pragma once

#include "resources/glme01addresses.h"
#include <cstdint>
#include <string>
#include <array>
#include <vector>

namespace gameedition {
    struct StartupAddresses {
        std::uint32_t titleArchive = 0;
        std::uint32_t sineTable = 0;
        std::uint32_t initialSaveFlags = 0;
        std::uint32_t memoryCardBanner = 0;
    };

    inline StartupAddresses startup(const std::string& id) {
        if (id == "GLME01") return {glme01::titleArchive, glme01::sineTable,
            glme01::initialSaveFlags, glme01::memoryCardBanner};
        if (id == "GLMJ01") return {0x80213240, 0x8020d518, 0x802012e8, 0x8032aec0};
        if (id == "GLMP01") return {0x80223160, 0x8021d2c0, 0x802111f0, 0x80325a80};
        return {};
    }

    inline std::string identify(const std::vector<std::uint8_t>& dol) {
        if (dol.size() < 0x100) return {};
        const auto section = (std::uint32_t(dol[0x6c]) << 24) | (std::uint32_t(dol[0x6d]) << 16) |
            (std::uint32_t(dol[0x6e]) << 8) | dol[0x6f];
        if (section == 0x8020bac0) return "GLME01";
        if (section == 0x80200c80) return "GLMJ01";
        if (section == 0x80210aa0) return "GLMP01";
        // Focused unit fixtures omit the edition-identifying DOL data section.
        // They contain North American addresses and should retain that legacy
        // interpretation; actual disc executables have a nonzero section here.
        if (section == 0) return "GLME01";
        return {};
    }

    inline std::uint32_t modelEffectAddress(const std::vector<std::uint8_t>& dol,
        std::uint32_t northAmerican) {
        const auto edition = identify(dol);
        if (edition == "GLMJ01") return northAmerican - 0xaf00;
        if (edition == "GLMP01") return northAmerican - 0xef80;
        return northAmerican;
    }

    inline std::uint32_t particleEffectRecord(const std::vector<std::uint8_t>& dol,
        std::uint32_t northAmerican) {
        return modelEffectAddress(dol, northAmerican);
    }

    inline std::uint32_t modelEffectSelector(const std::vector<std::uint8_t>& dol,
        std::uint32_t regional) {
        const auto edition = identify(dol);
        if (edition == "GLMJ01") return regional + 0x1d0c;
        if (edition == "GLMP01") return regional - 0x2570;
        return regional;
    }

    inline std::uint32_t trainingFireworks(const std::vector<std::uint8_t>& dol) {
        const auto edition = identify(dol);
        if (edition == "GLMJ01") return 0x802024a8;
        if (edition == "GLMP01") return 0x80212418;
        return 0x8020d2f0;
    }

    struct HudAddresses {
        std::uint32_t lifeNumbers;
        std::uint32_t floorNames;
        std::uint32_t roomNames;
        std::uint32_t moneyNumbers;
        std::uint32_t mapStick;
        std::uint32_t ghostTypes;
        std::uint32_t saveAreas;
        std::uint32_t panelPositions;
        std::uint32_t alpha;
        std::uint32_t digitScale;
        std::uint32_t ghostScale;
        std::uint32_t slideStep;
    };

    inline HudAddresses hud(const std::string& id) {
        if (id == "GLMJ01") return {0x802fa220, 0x802faaec, 0x802fb118,
            0x8032da90, 0x802f99dc, 0x8032d1ac, 0x80329730,
            0x804990a4, 0x804990ae, 0x804990b8, 0x804990b4, 0x80499060};
        if (id == "GLMP01") return {0x80309f28, 0x8030a7e0, 0x8030ad84,
            0x803286e0, 0x80309904, 0x80327d6c, 0x80324184,
            0x804bdbec, 0x804bdbf6, 0x804bdbfc, 0x804bdbf8, 0x804bdba0};
        return {0x80305020, 0x803058ec, 0x80305f20,
            0x80338950, 0x803047dc, 0x8033804c, 0x80338448,
            0x804d0524, 0x804d052e, 0x804d0538, 0x804d0534, 0x804d04e0};
    }

    struct MessageGraphicsAddresses {
        std::uint32_t portraits;
        std::uint32_t colors;
        std::uint32_t layout;
        std::uint32_t textScale;
        std::uint32_t windowTiming;
    };

    inline MessageGraphicsAddresses messageGraphics(const std::string& id) {
        if (id == "GLMJ01") return {0x802fa6a8, 0x802fa380, 0x804990c0, 0x80499098, 0x804990cc};
        if (id == "GLMP01") return {0x8030a3b0, 0x8030a088, 0x804bdc08, 0x804bdbe0, 0x804bdc14};
        return {0x803054a8, 0x80305180, 0x804d0540, 0x804d0518, 0x804d054c};
    }

    struct MenuAddresses {
        std::uint32_t fontResource = 0;
        std::uint32_t fontAsciiTable = 0;
        std::uint32_t fileDetailDigits = 0;
        std::uint32_t fileDetailAreas = 0;
        std::uint32_t fileDetailOffsets = 0;
        std::uint32_t helpText = 0;
        unsigned helpTextCount = 0;
        std::array<std::uint32_t, 6> optionTextures{};
    };

    inline MenuAddresses menu(const std::string& id) {
        if (id == "GLME01") return {glme01::fontResource, glme01::fontAsciiTable,
            glme01::fileDetailDigits, glme01::fileDetailAreas, 0x8033456c,
            0x803346d8, 5, {0x80334144,0x804d05c8,0x804d05d0,0x80334208,0x804d05d8,0x804d05e0}};
        if (id == "GLMJ01") return {0x802fb760, 0x803890e8, 0x80329598, 0x80329730, 0x80329740,
            0x80329864, 4, {0x80329334,0x80499150,0x80499158,0x803293e8,0x80499160,0x80499168}};
        if (id == "GLMP01") return {0x8030b340, 0x803852c8, 0x80323fec, 0x80324184, 0x80324194,
            0x80324300, 6, {0x80323c1c,0x804bdc90,0x804bdc98,0x80323d8c,0x804bdca0,0x804bdca8}};
        return {};
    }

    struct CutsceneAddresses {
        std::uint32_t definitions = 0;
        std::uint32_t descriptors = 0;
    };

    inline CutsceneAddresses cutscenes(const std::string& id) {
        if (id == "GLME01") return {0x80373aa8, 0x803727e8};
        if (id == "GLMJ01") return {0x80368bb0, 0x803678f0};
        if (id == "GLMP01") return {0x80364b28, 0x80363868};
        return {};
    }

    struct ActorAddresses {
        std::uint32_t modelTable = 0;
        std::uint32_t auxiliaryTable = 0;
        std::uint32_t keyTable = 0;
        std::uint32_t typeTable = 0;
        std::uint32_t animationTable = 0;
        std::uint32_t creationPools = 0;
        std::uint32_t creationCounts = 0;
        std::uint32_t constructors = 0;
        std::uint32_t strategyTypes = 0;
        std::uint32_t strategyFactories = 0;
        std::uint32_t strategySelector = 0;
        std::uint32_t generatorSelector = 0;
    };

    inline ActorAddresses actors(const std::string& id) {
        if (id == "GLME01") return {glme01::actorModelTable, glme01::actorAuxiliaryTable,
            glme01::actorKeyTable, glme01::actorTypeTable, glme01::actorAnimationTable,
            glme01::enemyCreationPools, glme01::enemyCreationCounts, glme01::enemyConstructors,
            glme01::strategyTypes, glme01::strategyFactories, 0x800db0d8, 0x800db168};
        if (id == "GLMJ01") return {0x803435ac, 0x8037ec70, 0x8020aac8, 0x803306d0,
            0x8033f804, 0x80355eb8, 0x80358c58, 0x80359368, 0x80359360, 0x8035ab68,
            0x800d9838, 0x800d98c8};
        if (id == "GLMP01") return {0x8033f4b4, 0x8037aca0, 0x8021aa50, 0x8032c5d8,
            0x8033b70c, 0x80351e30, 0x80354bd0, 0x803552e0, 0x803552d8, 0x80356ae0,
            0x800de518, 0x800de5a8};
        return {};
    }

    struct MaterialAddresses {
        std::uint32_t formats = 0;
        std::uint32_t registers = 0;
        std::uint32_t colors = 0;
    };

    inline MaterialAddresses materials(const std::string& id) {
        if (id == "GLME01") return {glme01::j3dDefaultFormats, glme01::j3dDefaultRegisters,
            glme01::j3dDefaultColors};
        if (id == "GLMJ01") return {0x80387c70, 0x804a0678, 0x804a6060};
        if (id == "GLMP01") return {0x80383e50, 0x804c51c0, 0x804cad88};
        return {};
    }

    struct ProjectionAddresses {
        std::uint32_t shakeDecay = 0;
        std::uint32_t halfAngle = 0;
        std::uint32_t radians = 0;
    };

    inline ProjectionAddresses projection(const std::string& id) {
        if (id == "GLME01") return {0x804d9c88, 0x804dd708, 0x804dd70c};
        if (id == "GLMJ01") return {0x804a2788, 0x804a61b8, 0x804a61bc};
        if (id == "GLMP01") return {0x804c7400, 0x804caee0, 0x804caee4};
        return {};
    }

    struct CameraFollowAddresses {
        std::uint32_t descriptors = 0;
        std::uint32_t presets = 0;
    };

    inline CameraFollowAddresses cameraFollow(const std::string& id) {
        if (id == "GLME01") return {0x802ff218, 0x802ffc48};
        if (id == "GLMJ01") return {0x802f4420, 0x802f4e50};
        if (id == "GLMP01") return {0x80304340, 0x80304d70};
        return {};
    }

    inline std::uint32_t cameraFollowConstant(const std::string& id, std::uint32_t original) {
        if (id == "GLME01") return original;
        if (id == "GLMJ01") {
            if (original >= 0x804d9a00 && original < 0x804d9f38) return original - 0x37500;
            if (original >= 0x804d0000 && original < 0x804d00b0) return original - 0x37480;
        }
        if (id == "GLMP01") {
            if (original == 0x804d9f34) return 0x804c76c4;
            if (original >= 0x804d9a00 && original < 0x804d9e30) return original - 0x12888;
            if (original >= 0x804d0000 && original < 0x804d0010) return original - 0x12938;
            if (original >= 0x804d0098 && original < 0x804d00b0) return original - 0x12930;
        }
        return 0;
    }

    struct CollisionAddresses {
        std::uint32_t initial = 0;
        std::uint32_t modelTable = 0;
        std::uint32_t typeTable = 0;
    };

    inline CollisionAddresses collisions(const std::string& id) {
        if (id == "GLME01") return {glme01::collisionInitialPrimary,
            glme01::collisionModelTable, glme01::collisionTypeTable};
        if (id == "GLMJ01") return {0x80201970, 0x80382e08, 0x80383220};
        if (id == "GLMP01") return {0x802118e0, 0x8037ee38, 0x8037f250};
        return {};
    }

    struct DoorAddresses {
        std::uint32_t models = 0;
        std::uint32_t animations = 0;
        std::uint32_t configurations = 0;
        std::uint32_t collisionBoxes = 0;
    };

    inline DoorAddresses doors(const std::string& id) {
        if (id == "GLME01") return {glme01::doorModels, glme01::doorAnimations,
            glme01::doorConfigurations, glme01::doorCollisionBoxes};
        if (id == "GLMJ01") return {0x802f4a70, 0x802f4b4c, 0x802f4b64, 0x802f4c90};
        if (id == "GLMP01") return {0x80304990, 0x80304a6c, 0x80304a84, 0x80304bb0};
        return {};
    }

    struct FloatTrigAddresses {
        std::uint32_t reduction = 0;
        std::uint32_t polynomial = 0;
        std::uint32_t constants = 0;
    };

    inline FloatTrigAddresses floatTrig(const std::string& id) {
        if (id == "GLME01") return {glme01::trigReductionTable, glme01::trigPolynomialTable,
            glme01::trigConstants};
        if (id == "GLMJ01") return {0x8020cef8, 0x8020d310, 0x804a63b0};
        if (id == "GLMP01") return {0x8021cd68, 0x8021d180, 0x804cb0a8};
        return {};
    }

    inline std::uint32_t playerActionDescriptor(const std::string& id, std::uint32_t original) {
        if (id == "GLME01") return original;
        if (id == "GLMJ01") return original - 0xaec0;
        if (id == "GLMP01") return original - 0xefb8;
        return 0;
    }

    inline std::uint32_t furnitureSoundTable(const std::string& id) {
        if (id == "GLME01") return 0x8038eb8c;
        if (id == "GLMJ01") return 0x80383c94;
        if (id == "GLMP01") return 0x8037fcc4;
        return 0;
    }

    struct AudioAddresses {
        std::uint32_t voicePredictor = 0;
        std::uint32_t resampling = 0;
        std::uint32_t pitch = 0;
        std::uint32_t finePitch = 0;
        std::uint32_t vibrato = 0;
        std::uint32_t streamPredictor = 0;
    };

    inline AudioAddresses audio(const std::string& id) {
        if (id == "GLME01") return {glme01::voicePredictorCoefficients, glme01::dspResamplingTable,
            glme01::voicePitchTable, glme01::voiceFinePitchTable, glme01::voiceVibratoParameters,
            glme01::streamPredictorCoefficients};
        if (id == "GLMJ01") return {0x80385ba0, 0x80385be0, 0x80385fe0, 0x80386908,
            0x80386a08, 0x80384028};
        if (id == "GLMP01") return {0x80381d20, 0x80381d60, 0x80382160, 0x80382a88,
            0x80382b88, 0x80382cb0};
        return {};
    }

    inline std::uint32_t thunderPosition(const std::string& id) {
        if (id == "GLME01") return 0x8038e71c;
        if (id == "GLMJ01") return 0x80383824;
        if (id == "GLMP01") return 0x8037f854;
        return 0;
    }

    inline std::uint32_t particleNoisePointers(const std::string& id) {
        if (id == "GLME01") return 0x80381678;
        if (id == "GLMJ01") return 0x80376778;
        if (id == "GLMP01") return 0x803726f8;
        return 0;
    }

    inline std::uint32_t particleBatchNames(const std::string& id) {
        if (id == "GLME01") return 0x80383b1c;
        if (id == "GLMJ01") return 0x80378c1c;
        if (id == "GLMP01") return 0x80374b9c;
        return 0;
    }
}
