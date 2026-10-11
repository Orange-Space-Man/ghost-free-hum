#include "game/scene/gameparameters.h"
#include "game/enemies/enemytypeparameters.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"

#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
    void append(std::vector<std::uint8_t>& data, unsigned value, unsigned width) {
        for (unsigned i = 0; i < width; ++i) data.push_back(static_cast<std::uint8_t>(value >> (8 * (width - i - 1))));
    }
    void entry(std::vector<std::uint8_t>& data, const std::string& name, unsigned declaredSize, unsigned value, unsigned width) {
        append(data, gameparameters::keyCode(name), 2);
        append(data, static_cast<unsigned>(name.size()), 2);
        data.insert(data.end(), name.begin(), name.end());
        append(data, declaredSize, 4);
        append(data, value, width);
    }
}

int main(int argc, char** argv) {
    try {
        require(gameparameters::keyCode("mLife") == 0xa62f, "Original parameter key from owned file header.");
        require(gameparameters::keyCode(std::string(1, static_cast<char>(255))) == 4991, "Signed-byte hash matches PowerPC.");
        gameparameters::Parameters parameters;
        std::string error;
        require(parameters.load({}, error) && !parameters.loaded(), "Empty registration list does not consume stream or set loaded flag.");
        parameters.add("number", 4, 8);
        parameters.add("number", 4, 9);
        parameters.add("short", 2, 10);
        parameters.add("float", 4, 0);
        std::vector<std::uint8_t> data;
        append(data, 5, 4);
        entry(data, "number", 99, 0xffffffff, 4);
        entry(data, "unknown", 2, 0xabcd, 2);
        entry(data, "short", 4, 0x1234, 2);
        entry(data, "float", 0, 0x3fc00000, 4);
        entry(data, "number", 4, 7, 4);
        require(parameters.load(data, error), error.c_str());
        const auto& values = parameters.entries();
        require(values[0].bits == 7 && values[1].bits == 9, "First registered duplicate handles each entry; later stream entries overwrite it.");
        require(values[2].bits == 0x1234 && values[3].bits == 0x3fc00000, "Known loaders ignore declared sizes and read native widths.");
        require(parameters.loaded(), "Complete parameter load marks the object loaded.");
        gameparameters::Parameters byte;
        byte.add("mFlagOnId", 1, 0x123);
        require(byte.entries()[0].bits == 0x23, "Byte parameter defaults retain their low eight bits.");
        std::vector<std::uint8_t> byteData;
        append(byteData, 1, 4);
        entry(byteData, "mFlagOnId", 99, 0x81, 1);
        require(byte.load(byteData, error) && byte.entries()[0].bits == 0x81, "Byte loader consumes one unsigned byte independently of declared size.");
        byte.setBits(0, 0x134);
        require(byte.entries()[0].bits == 0x34, "Byte parameter writes preserve original truncation.");
        gameparameters::Parameters vector;
        vector.addVector("mTrans", {0, 0xc1200000, 0});
        vector.add("after", 4, 0);
        std::vector<std::uint8_t> vectorData;
        append(vectorData, 2, 4);
        entry(vectorData, "mTrans", 99, 0x3f800000, 4);
        append(vectorData, 0xc0000000, 4); append(vectorData, 0x40400000, 4);
        entry(vectorData, "after", 4, 17, 4);
        require(vector.load(vectorData, error) && vector.entries()[0].vectorBits ==
            std::array<std::uint32_t, 3>{0x3f800000, 0xc0000000, 0x40400000} && vector.entries()[1].bits == 17,
            "Original vector loader consumes twelve big-endian bytes independently of declared length.");
        vectorData.resize(vectorData.size() - 20);
        require(!vector.load(vectorData, error), "Truncated vector parameters cannot read beyond the resource.");
        gameparameters::Parameters mismatch;
        mismatch.add("number", 4, 42);
        std::vector<std::uint8_t> wrongKey;
        append(wrongKey, 1, 4);
        entry(wrongKey, "number", 4, 99, 4);
        wrongKey[4] ^= 1;
        require(mismatch.load(wrongKey, error) && mismatch.entries()[0].bits == 42, "Both key and name must match.");
        gameparameters::Parameters negativeCount;
        negativeCount.add("number", 4, 42);
        require(negativeCount.load({255, 255, 255, 255}, error) && negativeCount.loaded(), "Original signed negative count performs no iterations.");
        data.pop_back();
        auto partial = enemytypes::defaults();
        require(!partial.load(data, error), "Truncated unknown payload is rejected.");
        gameparameters::Parameters longNames;
        const std::string truncated(79, 'a');
        longNames.add(truncated, 4, 0);
        std::vector<std::uint8_t> longData;
        append(longData, 1, 4);
        append(longData, gameparameters::keyCode(truncated), 2);
        append(longData, 85, 2);
        longData.insert(longData.end(), 85, 'a');
        append(longData, 4, 4); append(longData, 37, 4);
        require(longNames.load(longData, error) && longNames.entries()[0].bits == 37,
            "Original 80-byte name buffer truncates matching text but consumes the entire stream name.");
        enemytypes::Manager manager;
        for (unsigned type = 0; type < 381; ++type) {
            const auto& defaults = manager.parameters(type).entries();
            require(defaults.size() == 26 && defaults[0].name == "mLife" && defaults[25].name == "mAmbColor", "Original parameter registration order for all 381 types.");
            require(defaults[6].bits == 10 && defaults[7].bits == 0x42c80000 && defaults[8].bits == 0x43c80000 &&
                defaults[13].bits == 0xbf800000 && defaults[20].width == 2 && defaults[24].bits == 0xffffffff &&
                defaults[25].bits == 0x404040ff && !manager.parameters(type).loaded(), "Original constructor defaults.");
        }
        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> executable, compressed, archive;
            require(package.open(argv[1], error) && package.read("sys/main.dol", executable, error) &&
                package.read("files/Game/game_usa.szp", compressed, error) && roomarchive::decompress(compressed, archive, error), error.c_str());
            require(manager.loadParameters(executable, archive, error), error.c_str());
            unsigned loaded = 0, fallback = 0;
            for (unsigned type = 0; type < 381; ++type) {
                const auto& parameters = manager.parameters(type);
                loaded += parameters.loaded();
                fallback += !parameters.loaded() && parameters.entries()[25].bits == 0x323232ff;
            }
            require(loaded == 138 && fallback == 188, "Owned manager file mapping and missing-file ambient fallback.");
            const auto& yapoo = manager.parameters(11).entries();
            require(yapoo[0].bits == 10 && yapoo[1].bits == 5 && yapoo[3].bits == 12 && yapoo[4].bits == 7 &&
                yapoo[6].bits == 25 && yapoo[12].bits == 0x42f00000 && yapoo[20].bits == 1 &&
                yapoo[23].bits == 0x42480000 && yapoo[24].bits == 0xf19726ff,
                "Original yapoo1 integer, float, short and color parameter values.");
            require(manager.parameters(0).entries()[0].bits == 0 && manager.parameters(0).entries()[25].bits == 0x323232ff,
                "Missing Luigi type file retains constructor life and applies the original ambient fallback.");
            std::cout << "381 original type records: " << loaded << " parameter files loaded, " << fallback << " original ambient fallbacks.\n";
        }
        return 0;
    } catch (const std::exception& exception) { std::cerr << exception.what() << '\n'; return 1; }
}
