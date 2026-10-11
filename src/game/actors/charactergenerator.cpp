#include "game/actors/charactergenerator.h"

#include <stdexcept>

charactergenerator::Generator::Generator(const enemytypes::Catalog& types, enemies::Runtime& runtime,
    const mission::Resources& resources) : types(types), runtime(runtime), resources(resources) {
    reset();
}

void charactergenerator::Generator::reset() {
    source = {nullptr, 0};
    manager = nullptr;
    pathData.reset();
    count = 0;
    flags = 0;
    strategy.reset();
    total = 0;
}

std::int32_t charactergenerator::Generator::parameter(std::string_view name) const {
    if (!source.table) throw std::runtime_error("Original character generator has no source table.");
    std::uint32_t value = 0;
    source.table->getValue(source.row, name, value);
    return static_cast<std::int32_t>(value);
}

jmp::ToolData& charactergenerator::Generator::path() {
    if (!pathData.attached() && source.table && source.row >= 0) {
        std::string_view name;
        if (source.table->getValue(source.row, "path_name", name)) {
            if (const auto* path = resources.getPathResource(name)) pathData = *path;
            else pathData.reset();
        }
    }
    return pathData;
}

characterowner::Owner* charactergenerator::Generator::create(unsigned actorType, int selection) {
    if (parameter("max_enemy") != 0 && static_cast<std::int32_t>(total) >= parameter("max_enemy")) return nullptr;
    if (count >= parameter("max_enemy_once")) return nullptr;
    if (!(flags & 2) && runtime.roomCleared(runtime.resolveRoom(position) >> 24)) return nullptr;
    if (!(flags & 1) && !runtime.roomVisible(runtime.resolveRoom(position), 0)) return nullptr;
    if (count >= 15) return nullptr;
    if ((types.flags(actorType) & 1) && runtime.typeCaptured(0, actorType)) return nullptr;
    if (!manager) throw std::runtime_error("Original character generator has no EnManager.");
    if (count < 0) throw std::runtime_error("Original character generator has a negative child count.");
    const auto index = manager->createActor(actorType, selection);
    if (static_cast<std::int32_t>(index) < 0) return nullptr;
    auto* child = dynamic_cast<characterowner::Owner*>(&manager->owner(static_cast<int>(index)));
    if (!child || child->kind() != characterowner::Kind::Ordinary) {
        throw std::runtime_error("Original character generator requires an ordinary character owner.");
    }
    child->state().generator = this;
    children[count++] = child;
    ++total;
    return child;
}

void charactergenerator::Generator::remove(characterowner::Owner& owner) {
    if (count < 0 || count > static_cast<int>(children.size())) throw std::runtime_error("Invalid original generator child count.");
    auto* target = owner.kind() == characterowner::Kind::Ordinary ? &owner : nullptr;
    int index = 0;
    while (index < count && children[index] != target) ++index;
    while (index + 1 < count) {
        children[index] = children[index + 1];
        ++index;
    }
    --count;
    if (auto* observer = dynamic_cast<Observer*>(strategy.get())) {
        Event event{0};
        observer->receive(event);
    }
}
