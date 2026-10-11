#pragma once

#include "resources/gameassetpackage.h"
#include "jlib/jaudio/jaudioeffectconfig.h"
#include <array>

namespace jaudio {
    struct Section {
        std::uint32_t kind = 0;
        std::uint32_t offset = 0;
        std::uint32_t size = 0;
        std::uint32_t loadFlags = 0;
    };

    struct Sequence {
        std::string name;
        std::uint16_t redirect = 0xffff;
        std::uint32_t offset = 0;
        std::uint32_t size = 0;
    };

    struct Resources {
        std::vector<std::uint8_t> initialization;
        std::vector<Section> sections;
        EffectConfiguration effects;
        std::array<std::vector<std::array<std::uint8_t, 16>>, 18> sounds;
        std::string sequenceArchiveName;
        std::vector<Sequence> sequences;
        std::vector<std::uint8_t> sequenceArchive;
        std::map<std::string, std::vector<std::uint8_t>> bootWaveArchives;
    };

    bool decodeInitialization(const std::vector<std::uint8_t>& data, Resources& resources, std::string& error);
    bool read(gameassets::Package& package, Resources& resources, std::string& error);
    const std::array<std::uint8_t, 16>* soundInfo(const Resources& resources, std::uint32_t id);
    bool readSequence(const Resources& resources, std::uint32_t id, std::vector<std::uint8_t>& sequence, std::string& error);
}
