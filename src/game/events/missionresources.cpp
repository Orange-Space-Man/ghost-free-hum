#include "game/events/missionresources.h"
#include <stdexcept>

bool mission::Resources::read(const std::vector<std::uint8_t>& archive, std::string& error) {
    error.clear();
    try {
        Resources result;
        if (!result.source.read(archive, error)) return false;
        const auto entries = result.source.directory("/jmp");
        if (entries.size() > 2) {
            for (std::size_t i = 0; i < entries.size() - 2; ++i) {
                const auto index = result.source.findTypeResource(0x4a4d5020, result.source.entry(entries[i]).name);
                if (index < 0) { result.tables.push_back({0, nullptr}); continue; }
                auto data = std::make_unique<jmp::ToolData>();
                const auto bytes = result.source.resource(index);
                if (bytes.empty()) throw std::runtime_error("Empty mission JMP resource.");
                if (!data->attach(bytes, error)) return false;
                result.tables.push_back({result.source.entry(index).offset, std::move(data)});
            }
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}

const jmp::ToolData* mission::Resources::getJmpResource(std::string_view name) const {
    const auto index = source.findTypeResource(0x4a4d5020, name);
    if (index < 0) return nullptr;
    const auto offset = source.entry(index).offset;
    for (const auto& table : tables) if (table.data && table.offset == offset) return table.data.get();
    return nullptr;
}

const gamearchive::Archive& mission::Resources::archive() const { return source; }
const jmp::ToolData* mission::Resources::getPathResource(std::string_view name) const {
    const auto index = source.findTypeResource(0x50415448, name);
    if (index < 0) return nullptr;
    const auto offset = source.entry(index).offset;
    for (const auto& path : paths) if (path.offset == offset) return path.data.get();
    auto data = std::make_unique<jmp::ToolData>();
    std::string error;
    if (!data->attach(source.resource(index), error)) throw std::runtime_error(error);
    paths.push_back({offset, std::move(data)});
    return paths.back().data.get();
}
std::size_t mission::Resources::tableCount() const { return tables.size(); }
