#include "resources/dolresources.h"
#include "game/startup/openingresources.h"
#include "game/startup/startupresources.h"
#include "game/scene/gamearchive.h"
#include "resources/roomarchivereader.h"

#include <algorithm>
#include <stdexcept>

namespace {
    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (unsigned(data.at(offset)) << 24) | (unsigned(data.at(offset + 1)) << 16) |
            (unsigned(data.at(offset + 2)) << 8) | data.at(offset + 3);
    }

    std::vector<std::uint8_t> range(const std::vector<std::uint8_t>& executable, unsigned address, unsigned size) {
        std::vector<std::uint8_t> result;
        std::string error;
        if (!dolresources::read(executable, address, size, result, error)) throw std::runtime_error(error);
        return result;
    }

    std::string name(const std::vector<std::uint8_t>& executable, unsigned address) {
        if (address == 0) return {};
        std::string result;
        for (unsigned i = 0; i < 128; ++i) {
            const auto value = range(executable, address + i, 1)[0];
            if (value == 0) return result;
            result.push_back(static_cast<char>(value));
        }
        throw std::runtime_error("Unterminated opening resource name.");
    }

    openingscene::Resource resource(const gamearchive::Archive& archive, std::string name, bool optional = false) {
        openingscene::Resource result;
        result.name = std::move(name);
        if (result.name.empty()) return result;
        const auto index = archive.findNameResource(result.name);
        if (index >= 0) result.data = archive.resource(static_cast<unsigned>(index));
        else if (!optional) throw std::runtime_error("Cutscene archive resource not found: " + result.name);
        return result;
    }
}

unsigned openingscene::doorCutscene(bool missingKey, bool front, unsigned type) {
    const std::uint32_t index = (type - 1) * 4 + unsigned(front) * 2 + unsigned(missingKey) + 3;
    return index < 3 || index > 58 ? 3 : index;
}

bool openingscene::read(gameassets::Package& package, unsigned cutscene, Resources& resources, std::string& error) {
    error.clear();
    try {
        if (cutscene != 0 && cutscene != 0x4c && (cutscene < 3 || cutscene > 58)) throw std::runtime_error("Unsupported cutscene ID.");
        std::vector<std::uint8_t> executable;
        if (!package.read("sys/main.dol", executable, error)) throw std::runtime_error(error);
        Resources result;
        result.cutscene = cutscene;
        const auto definition = range(executable, 0x80373aa8 + cutscene * 24, 24);
        result.initialValue = word(definition, 0);
        result.mode = word(definition, 16);
        const auto list = word(definition, 4);
        bool terminated = false;
        for (unsigned index = 0; index < 128; ++index) {
            const auto entry = range(executable, list + index * 2, 2);
            const unsigned id = (entry[0] << 8) | entry[1];
            if (id == 255) { terminated = true; break; }
            if (id >= 255) throw std::runtime_error("Invalid opening segment ID.");
            const auto descriptor = range(executable, 0x803727e8 + id * 20, 20);
            Segment segment;
            segment.id = id;
            segment.archive = name(executable, word(descriptor, 8));
            if (segment.archive.empty() || segment.archive.front() != '/') throw std::runtime_error("Invalid opening archive path.");
            std::vector<std::uint8_t> compressed, archiveBytes;
            gamearchive::Archive archive;
            if (!package.read("files" + segment.archive, compressed, error) || !roomarchive::decompress(compressed, archiveBytes, error) ||
                !archive.read(archiveBytes, error))
                throw std::runtime_error(error);
            segment.scene = resource(archive, name(executable, word(descriptor, 12)));
            segment.settings = resource(archive, name(executable, word(descriptor, 16)));
            const auto models = range(executable, word(descriptor, 4), descriptor[0] * 4);
            for (unsigned i = 0; i < descriptor[0]; ++i) {
                const auto model = range(executable, word(models, i * 4), 28);
                ModelResources loaded;
                loaded.flags = word(model, 0);
                loaded.model = resource(archive, name(executable, word(model, 8)));
                for (unsigned group = 0; group < 4; ++group) {
                    if (!model[4 + group]) continue;
                    const auto names = range(executable, word(model, 12 + group * 4), model[4 + group] * 4);
                    for (unsigned animation = 0; animation < model[4 + group]; ++animation) {
                        auto filename = name(executable, word(names, animation * 4));
                        loaded.animations[group].push_back(resource(archive, filename));
                        if (group == 0) {
                            const auto extension = filename.find(".key");
                            if (extension != std::string::npos) {
                                filename.replace(extension, std::string::npos, ".bas");
                                loaded.soundAnimations.push_back(resource(archive, filename, true));
                            }
                        }
                    }
                }
                segment.models.push_back(std::move(loaded));
            }
            result.segments.push_back(std::move(segment));
        }
        if (!terminated || result.segments.empty()) throw std::runtime_error("Unterminated or empty opening segment list.");
        terminated = false;
        const auto events = word(definition, 8);
        for (unsigned i = 0; i < 4096; ++i) {
            const auto data = range(executable, events + i * 12, 12);
            if (((data[10] << 8) | data[11]) >= 255) { terminated = true; break; }
            std::array<std::uint8_t, 12> event;
            std::copy(data.begin(), data.end(), event.begin());
            result.events.push_back(event);
        }
        if (!terminated) throw std::runtime_error("Unterminated opening event list.");
        resources = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
