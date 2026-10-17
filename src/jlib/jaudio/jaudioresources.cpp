#include "jlib/jaudio/jaudioresources.h"

#include <algorithm>
#include <stdexcept>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    void range(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t size) {
        require(offset <= data.size() && size <= data.size() - offset, "Truncated JAudio resource.");
    }

    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        range(data, offset, 2);
        return (data[offset] << 8) | data[offset + 1];
    }

    std::uint32_t dword(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (word(data, offset) << 16) | word(data, offset + 2);
    }

    std::vector<std::uint8_t> bytes(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t size) {
        range(data, offset, size);
        return {data.begin() + offset, data.begin() + offset + size};
    }

    std::string name(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t size) {
        range(data, offset, size);
        const auto first = data.begin() + offset;
        const auto last = std::find(first, first + size, 0);
        require(last != first + size && last != first, "Invalid JAudio resource name.");
        const std::string result(first, last);
        require(result.find_first_of("/\\:") == std::string::npos && result != "." && result != "..", "Invalid JAudio resource path.");
        return result;
    }

    const jaudio::Section& section(const jaudio::Resources& resources, unsigned kind) {
        const auto found = std::find_if(resources.sections.begin(), resources.sections.end(), [kind](const auto& entry) { return entry.kind == kind; });
        require(found != resources.sections.end(), "Missing JAudio initialization section.");
        return *found;
    }

    std::vector<std::uint8_t> sectionBytes(const jaudio::Resources& resources, const jaudio::Section& entry) {
        return bytes(resources.initialization, entry.offset, entry.size);
    }

    std::size_t resolveSequence(const jaudio::Resources& resources, std::size_t index) {
        for (std::size_t step = 0; step < resources.sequences.size(); ++step) {
            require(index < resources.sequences.size(), "JAudio sequence redirect is outside the archive.");
            const auto redirect = resources.sequences[index].redirect;
            if (redirect == 0xffff) return index;
            index = redirect;
        }
        throw std::runtime_error("Cyclic JAudio sequence redirect.");
    }
}

bool jaudio::decodeInitialization(const std::vector<std::uint8_t>& data, Resources& resources, std::string& error) {
    resources = {};
    error.clear();
    try {
        Resources result;
        result.initialization = data;
        std::size_t cursor = 0;
        std::array<bool, 8> seen{};
        while (true) {
            const auto kind = dword(data, cursor);
            cursor += 4;
            if (kind == 0) break;
            require(kind < seen.size() && !seen[kind], "Unsupported or duplicate JAudio initialization section.");
            seen[kind] = true;
            if (kind == 1 || kind == 6 || kind == 7) {
                const Section entry{kind, dword(data, cursor), dword(data, cursor + 4), dword(data, cursor + 8)};
                require(entry.loadFlags == 0, "Split JAudio information tables are not implemented.");
                range(data, entry.offset, entry.size);
                result.sections.push_back(entry);
                cursor += 12;
            } else {
                while (dword(data, cursor) != 0) {
                    const bool flagged = kind == 2 || kind == 3;
                    const Section entry{kind, dword(data, cursor), dword(data, cursor + 4), flagged ? dword(data, cursor + 8) : 0};
                    range(data, entry.offset, entry.size);
                    result.sections.push_back(entry);
                    cursor += flagged ? 12 : 8;
                }
                cursor += 4;
            }
        }
        for (const auto& entry : result.sections) {
            require(entry.offset >= cursor && entry.size != 0, "JAudio section overlaps its directory.");
            if (entry.kind == 7 && !decodeEffectConfiguration(sectionBytes(result, entry), result.effects, error)) {
                throw std::runtime_error(error);
            }
            if (entry.kind == 6) {
                const auto block = sectionBytes(result, entry);
                const auto count = dword(block, 0);
                range(block, 4, static_cast<std::size_t>(count) * 4);
                result.effectCategories.resize(count);
                for (unsigned mode = 0; mode < count; ++mode) {
                    const auto offset = dword(block, 4 + mode * 4);
                    range(block, offset, 10);
                    for (unsigned category = 0; category < 5; ++category) {
                        result.effectCategories[mode][category] = {block[offset + category * 2], block[offset + category * 2 + 1]};
                    }
                }
            }
            if (entry.kind == 2 || entry.kind == 3) {
                const auto block = sectionBytes(result, entry);
                require(dword(block, 0) == (entry.kind == 2 ? 0x49424e4bU : 0x57535953U), "Invalid JAudio bank signature.");
                require(dword(block, 4) >= 24 && dword(block, 4) <= block.size(), "Invalid JAudio bank size.");
            }
        }
        const auto info = sectionBytes(result, section(result, 1));
        range(info, 0, 0x50);
        std::copy_n(info.begin(), 3, result.soundParameterFlags.begin());
        for (unsigned category = 0; category < result.sounds.size(); ++category) {
            const auto count = word(info, 6 + category * 4);
            const auto offset = 0x50 + word(info, 8 + category * 4) * 16;
            range(info, offset, count * 16);
            auto& sounds = result.sounds[category];
            sounds.resize(count);
            for (unsigned index = 0; index < count; ++index) {
                std::copy_n(info.begin() + offset + index * 16, 16, sounds[index].begin());
            }
        }
        const auto header = sectionBytes(result, section(result, 4));
        require(dword(header, 0) == 0x42415243, "Invalid JAudio sequence archive header.");
        const auto count = dword(header, 12);
        range(header, 32, static_cast<std::size_t>(count) * 32);
        result.sequenceArchiveName = name(header, 16, 16);
        for (unsigned index = 0; index < count; ++index) {
            const std::size_t offset = 32 + index * 32;
            Sequence sequence;
            sequence.name = name(header, offset, 14);
            sequence.redirect = static_cast<std::uint16_t>(word(header, offset + 14));
            sequence.offset = dword(header, offset + 24);
            sequence.size = dword(header, offset + 28);
            result.sequences.push_back(std::move(sequence));
        }
        for (std::size_t index = 0; index < result.sequences.size(); ++index) resolveSequence(result, index);
        resources = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

const std::array<std::uint8_t, 16>* jaudio::soundInfo(const Resources& resources, std::uint32_t id) {
    const auto type = id & 0xc0000000;
    if (type == 0x40000000) return nullptr;
    const auto category = type == 0x80000000 ? 16 : type == 0xc0000000 ? 17 : (id >> 12) & 255;
    const auto index = id & 0x3ff;
    if (category >= resources.sounds.size() || index >= resources.sounds[category].size()) return nullptr;
    return &resources.sounds[category][index];
}

bool jaudio::readSequence(const Resources& resources, std::uint32_t id, std::vector<std::uint8_t>& sequence, std::string& error) {
    sequence.clear();
    error.clear();
    try {
        require((id & 0xc0000000) == 0x80000000 && soundInfo(resources, id) != nullptr, "Invalid JAudio sequence sound ID.");
        const auto& entry = resources.sequences[resolveSequence(resources, id & 0x3ff)];
        sequence = bytes(resources.sequenceArchive, entry.offset, entry.size);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool jaudio::read(gameassets::Package& package, Resources& resources, std::string& error) {
    resources = {};
    error.clear();
    try {
        Resources result;
        std::vector<std::uint8_t> data;
        if (!package.read("files/AudioRes/JaiInit.aaf", data, error) || !decodeInitialization(data, result, error) ||
            !package.read("files/AudioRes/Seqs/" + result.sequenceArchiveName, result.sequenceArchive, error)) throw std::runtime_error(error);
        for (std::size_t index = 0; index < result.sequences.size(); ++index) {
            const auto& entry = result.sequences[resolveSequence(result, index)];
            range(result.sequenceArchive, entry.offset, entry.size);
        }
        for (const auto& entry : result.sections) {
            if (entry.kind != 3 || entry.loadFlags != 0) continue;
            const auto system = sectionBytes(result, entry);
            const auto info = dword(system, 16);
            require(dword(system, info) == 0x57494e46, "Invalid JAudio wave information.");
            const auto count = dword(system, info + 4);
            range(system, info + 8, static_cast<std::size_t>(count) * 4);
            for (unsigned index = 0; index < count; ++index) {
                const auto group = dword(system, info + 8 + index * 4);
                const auto filename = name(system, group, 112);
                if (result.bootWaveArchives.count(filename) != 0) continue;
                if (!package.read("files/AudioRes/Banks/" + filename, data, error)) throw std::runtime_error(error);
                result.bootWaveArchives.emplace(filename, std::move(data));
            }
        }
        resources = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
