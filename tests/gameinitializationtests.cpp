#include "jlib/jaudio/jaudioresources.h"
#include "game/save/gamesavestate.h"
#include "game/scene/gameentrystate.h"

#include <algorithm>
#include <iostream>

namespace {
    void write16(std::vector<std::uint8_t>& data, std::size_t offset, unsigned value) {
        data[offset] = static_cast<std::uint8_t>(value >> 8);
        data[offset + 1] = static_cast<std::uint8_t>(value);
    }

    void write32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) data[offset + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }

    void writeName(std::vector<std::uint8_t>& data, std::size_t offset, const std::string& value) {
        std::copy(value.begin(), value.end(), data.begin() + offset);
    }

    std::vector<std::uint8_t> initialization() {
        std::vector<std::uint8_t> result(0x260);
        write32(result, 0, 1);
        write32(result, 4, 0x100);
        write32(result, 8, 0x100);
        write32(result, 16, 4);
        write32(result, 20, 0x200);
        write32(result, 24, 0x60);
        write16(result, 0x100 + 6 + 16 * 4, 2);
        write16(result, 0x100 + 6 + 3 * 4, 3);
        write16(result, 0x100 + 8 + 3 * 4, 2);
        result[0x150] = 0x21;
        result[0x190] = 0x38;
        writeName(result, 0x200, "BARC");
        write32(result, 0x20c, 2);
        writeName(result, 0x210, "test.arc");
        writeName(result, 0x220, "first.bms");
        write16(result, 0x22e, 0xffff);
        write32(result, 0x238, 2);
        write32(result, 0x23c, 4);
        writeName(result, 0x240, "alias.bms");
        write16(result, 0x24e, 0);
        return result;
    }
}

int main(int argc, char** argv) {
    unsigned failures = 0;
    const auto check = [&](bool condition, const char* name) {
        if (!condition) { std::cerr << name << '\n'; ++failures; }
    };
    std::string error;
    gamesave::State newSaves;
    check(gamesave::initialize({0xff}, newSaves, error), "Initialize empty slots");
    gameentry::State entry;
    for (unsigned slot = 0; slot < 3; ++slot) {
        const unsigned base = 12 + slot * 0x3b0;
        newSaves.working[base + 2] = slot == 1 ? 0x80 : 0;
        newSaves.working[base + 12] = 0xff;
        newSaves.working[base + 0x14] = static_cast<std::uint8_t>(slot + 1);
        check(gameentry::selectNewFile(newSaves, slot, entry, error), "Select each empty slot");
        check(entry.slot == slot && entry.map == 2 && entry.flags.back() == 0x1f, "Original slot/map and 85-bit flag restore");
        check(entry.cutscene == (slot == 1 ? 0 : 0x4c), "Flag 7 selects original opening variant");
        check(entry.inventory[0] == slot + 1 && entry.player[1] == 100, "Selected slot supplies runtime inventory and player");
    }
    const auto untouched = newSaves.working;
    check(!gameentry::selectNewFile(newSaves, 3, entry, error) && newSaves.working == untouched, "Invalid slot cannot mutate saves");
    newSaves.working[13] = 1;
    check(!gameentry::selectNewFile(newSaves, 0, entry, error), "Used files cannot be initialized as a new game");
    jaudio::Resources resources;
    auto data = initialization();
    check(jaudio::decodeInitialization(data, resources, error), error.c_str());
    check(resources.sequences.size() == 2 && resources.sequenceArchiveName == "test.arc", "Sequence archive directory");
    const auto music = jaudio::soundInfo(resources, 0x80000800);
    const auto effect = jaudio::soundInfo(resources, 0x3802);
    check(music != nullptr && (*music)[0] == 0x21, "Sequence lookup masks to original low ten bits");
    check(effect != nullptr && (*effect)[0] == 0x38, "Sound effect category and entry lookup");
    check(jaudio::soundInfo(resources, 0x40000000) == nullptr && jaudio::soundInfo(resources, 0xff002) == nullptr, "Unsupported sound categories cannot index outside tables");
    resources.sequenceArchive = {0, 1, 2, 3, 4, 5};
    std::vector<std::uint8_t> sequence;
    check(jaudio::readSequence(resources, 0x80000001, sequence, error) && sequence == std::vector<std::uint8_t>({2, 3, 4, 5}), "Sequence redirects resolve original byte range");
    resources.sequenceArchive.pop_back();
    check(!jaudio::readSequence(resources, 0x80000000, sequence, error) && sequence.empty(), "Truncated sequence payload rejected");
    write16(data, 0x22e, 1);
    check(!jaudio::decodeInitialization(data, resources, error), "Cyclic sequence aliases rejected");
    data = initialization();
    write16(data, 0x24e, 2);
    check(!jaudio::decodeInitialization(data, resources, error), "Out-of-range sequence alias rejected");
    data = initialization();
    write32(data, 4, 0xfffffff0);
    check(!jaudio::decodeInitialization(data, resources, error), "Out-of-range AAF section rejected");
    data = initialization();
    write16(data, 0x100 + 8 + 16 * 4, 0xffff);
    check(!jaudio::decodeInitialization(data, resources, error), "Out-of-range sound table rejected");
    data = initialization();
    writeName(data, 0x210, "../bad.arc");
    check(!jaudio::decodeInitialization(data, resources, error), "Archive path cannot escape audio directory");
    for (std::size_t size : {0U, 4U, 20U, 31U, 511U}) {
        data = initialization();
        data.resize(size);
        check(!jaudio::decodeInitialization(data, resources, error), "Truncated AAF rejected");
    }

    gamesave::State saves;
    check(gamesave::initialize({0, 31, 32, 255}, saves, error), "Original save defaults initialize");
    check(saves.working == saves.committed && saves.working[10] == 3 && saves.working[11] == 0xb0, "Save working/committed blocks and big-endian record stride");
    for (unsigned slot = 0; slot < 3; ++slot) {
        const auto start = 12 + slot * 0x3b0;
        check(!gamesave::slotUsed(saves, slot), "New save slots are unoccupied");
        check(saves.working[start + 0x14] == 0x80 && saves.working[start + 0x17] == 1 && saves.working[start + 0x18] == 0x80, "Original inventory flag bit order");
        for (unsigned index = 0; index < 25; ++index) check(saves.working[start + 0x234 + index * 2] == 0 && saves.working[start + 0x235 + index * 2] == 100, "Initial health fields preserve big-endian 100");
        check(saves.working[start + 0x141] == 2 && saves.working[start + 0x307] == 1 && saves.working[start + 0x37c] == 0x20 && saves.working[start + 0x39e] == 1, "Original nonzero default flags");
    }
    check(gamesave::soundMode(saves) == 1 && gamesave::rumbleEnabled(saves), "Original stereo and rumble defaults");
    check(!gamesave::initialize({0, 31}, saves, error) && !saves.initialized, "Unterminated original flag table rejected");

    if (argc == 2) {
        gameassets::Package package;
        const bool loaded = package.open(argv[1], error) && jaudio::read(package, resources, error);
        check(loaded, error.c_str());
        if (loaded) {
            check(resources.sequences.size() == 89 && resources.sequences[33].name == "Tittle.com", "Original title sequence mapping");
            check(jaudio::readSequence(resources, 0x80000021, sequence, error) && sequence.size() == 4992 && sequence[0] == 0xc1, "Original title sequence bytes");
            check(resources.bootWaveArchives.size() == 1 && resources.bootWaveArchives.at("LuiSe2_0.aw").size() == 256608, "Original boot wave loading flags");
            check(resources.sounds[16].size() == 89 && jaudio::soundInfo(resources, 0x3802) != nullptr, "Original title and Start sound metadata");
        }
    }
    if (failures == 0) std::cout << "Audio resource and save initialization checks passed.\n";
    return failures == 0 ? 0 : 1;
}
