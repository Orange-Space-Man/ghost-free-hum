#include "game/enemies/enemygeneratormanager.h"

#include <cstring>
#include <stdexcept>

namespace {
    bool isGenerator(const jmp::ToolDataRef& entry) {
        std::string_view name;
        return entry.table && entry.row >= 0 && entry.table->getValue(entry.row, "name", name) && name == "generator";
    }
}

charactergenerator::Manager::Manager(const enemytypes::Catalog& types, const enemycreation::Catalog& actors,
    const enemystrategy::Catalog& strategies, const enemystrategy::Factory& factory, enemies::Manager& enemies,
    enemies::Runtime& runtime, const mission::Resources& resources) : types(types), actors(actors), strategies(strategies),
    factory(factory), enemies(enemies), runtime(runtime) {
    for (auto& generator : generators) generator = std::make_unique<Generator>(types, runtime, resources);
}

charactergenerator::Manager::~Manager() {
    for (unsigned i = static_cast<unsigned>(generators.size()); i > 0; --i) generators[i - 1].reset();
}

charactergenerator::Generator* charactergenerator::Manager::find(const jmp::ToolDataRef& entry) {
    for (const auto& generator : generators) {
        if (generator->source.table && entry.table && jmp::sameEntry(generator->source, entry)) return generator.get();
    }
    return nullptr;
}

bool charactergenerator::Manager::create(jmp::ToolDataRef& entry) {
    if (!isGenerator(entry)) return false;
    for (auto& generator : generators) {
        if (generator->strategy) continue;
        std::string_view name;
        entry.table->getValue(entry.row, "type", name);
        const auto type = types.find(name);
        const auto selection = strategies.selectGenerator(type);
        generator->source = entry;
        generator->type = type;
        generator->manager = &enemies;
        generator->strategy.reset();
        generator->strategy = factory.construct(strategies.constructor(selection));
        generator->strategy->context = generator.get();
        entry.table->getValue(entry.row, "pos_x", generator->position[0]);
        entry.table->getValue(entry.row, "pos_y", generator->position[1]);
        entry.table->getValue(entry.row, "pos_z", generator->position[2]);
        generator->strategy->begin();
        break;
    }
    return true;
}

bool charactergenerator::Manager::remove(jmp::ToolDataRef& entry) {
    if (!isGenerator(entry)) return false;
    auto* generator = find(entry);
    if (!generator) return false;
    if (generator->count < 0 || generator->count > static_cast<int>(generator->children.size())) {
        throw std::runtime_error("Invalid original generator child count during removal.");
    }
    for (int i = 0; i < generator->count; ++i) {
        auto* child = generator->children[i];
        if (!child) throw std::runtime_error("Original generator removal encountered a null child.");
        child->state().generator = nullptr;
        child->releaseFromManager();
    }
    generator->reset();
    return true;
}

bool charactergenerator::Manager::message(jmp::ToolDataRef& entry, const char* command) {
    if (!isGenerator(entry)) return false;
    auto* generator = find(entry);
    if (!generator) return false;
    if (!command) throw std::runtime_error("Original generator command is null.");
    unsigned state;
    if (std::strcmp(command, "start") == 0) state = 0x104;
    else if (std::strcmp(command, "stop") == 0) state = 0x103;
    else return false;
    if (!generator->strategy) throw std::runtime_error("Original generator command has no strategy.");
    generator->strategy->setNextState(static_cast<std::uint16_t>(state));
    return true;
}

void charactergenerator::Manager::update() {
    if (!enabled) return;
    for (const auto& generator : generators) {
        if (!generator->strategy) continue;
        if (!runtime.roomVisible(runtime.resolveRoom(generator->position), 0)) generator->total = 0;
        generator->strategy->update();
    }
}

std::array<std::uint8_t, 262> charactergenerator::Manager::requiredModels() {
    std::array<std::uint8_t, 262> models{};
    std::array<std::uint8_t, 381> requestedTypes{};
    for (const auto& generator : generators) {
        if (!generator->strategy) continue;
        if (auto* observer = dynamic_cast<Observer*>(generator->strategy.get())) {
            Event event{1, &requestedTypes};
            observer->receive(event);
        }
        requestedTypes.at(generator->type) = 1;
    }
    for (unsigned i = 0; i < requestedTypes.size(); ++i) if (requestedTypes[i]) models.at(actors.model(i)) = 1;
    return models;
}
