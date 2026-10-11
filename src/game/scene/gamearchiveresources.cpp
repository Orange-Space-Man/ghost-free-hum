#include "game/scene/gamearchiveresources.h"

gamearchive::Resources::Resources(const Archive& archive) : archive(archive) {}

particles::Resource gamearchive::Resources::get(std::uint32_t type, std::string_view name) {
    const auto index = archive.findTypeResource(type, name);
    if (index < 0) return {};
    auto& resource = entries[static_cast<unsigned>(index)];
    if (!resource) resource = std::make_shared<const std::vector<std::uint8_t>>(archive.resource(static_cast<unsigned>(index)));
    return resource;
}
