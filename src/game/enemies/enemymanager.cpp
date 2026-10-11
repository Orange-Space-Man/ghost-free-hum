#include "game/enemies/enemymanager.h"

#include <cmath>
#include <limits>
#include <stdexcept>

std::uint32_t enemies::Owner::event(unsigned message, std::array<std::uint8_t, 381>* types) {
    characterevent::Event payload(message);
    payload.types = types;
    return event(payload);
}

namespace {
    bool valid(const jmp::ToolDataRef& source) {
        return source.table && source.row >= 0;
    }

    std::array<float, 3> position(const jmp::ToolDataRef& source) {
        std::array<float, 3> result{};
        const char* fields[] = {"pos_x", "pos_y", "pos_z"};
        for (unsigned i = 0; i < 3; ++i) {
            if (!source.table->getValue(source.row, fields[i], result[i])) {
                throw std::runtime_error("Actor source lacks an original position field.");
            }
        }
        return result;
    }

    std::array<std::int16_t, 3> rotation(const jmp::ToolDataRef& source) {
        std::array<std::int16_t, 3> result{};
        const char* fields[] = {"dir_x", "dir_y", "dir_z"};
        for (unsigned i = 0; i < 3; ++i) {
            float degrees = 0;
            source.table->getValue(source.row, fields[i], degrees);
            if (!std::isfinite(degrees) || std::abs(degrees) > 360000) {
                throw std::runtime_error("Invalid actor rotation.");
            }
            double angle = degrees;
            while (angle < 0 || angle > 360) {
                if (angle < 0) {
                    angle += 360;
                }
                if (angle > 360) {
                    angle -= 360;
                }
            }
            const unsigned value = static_cast<unsigned>(65536 * angle / 360) & 65535;
            result[i] = static_cast<std::int16_t>(value < 32768 ? int(value) : int(value) - 65536);
        }
        return result;
    }

    std::optional<std::string_view> name(const jmp::ToolDataRef& source) {
        return enemycreation::createName({source, 0});
    }

    bool sameRoom(unsigned first, unsigned second) {
        return first == second || ((first & 255) == 2 && (second & 255) == 30) ||
            ((first & 255) == 30 && (second & 255) == 2);
    }
}

enemies::Manager::Manager(const enemytypes::Catalog& types, const enemycreation::Catalog& creation, Runtime& runtime,
    unsigned players, unsigned mapMode) : types(types), creation(creation), runtime(runtime), players(players), mapMode(mapMode),
    characters(std::make_unique<jmp::ToolData>()) {
    if (players < 1 || players > appear.size()) {
        throw std::runtime_error("Unsupported original player count.");
    }
}

enemies::Manager::~Manager() {
    characters.reset();
    requests = {};
    for (auto entry = entries.rbegin(); entry != entries.rend(); ++entry) {
        entry->owner.reset();
    }
}

jmp::ToolDataRef enemies::Manager::findAppearPoint(unsigned point) const {
    const auto field = characters->searchItemInfo("name");
    unsigned start = 0;
    while (start < characters->count()) {
        const int row = characters->findEntryByValue(field, "luige", start);
        if (row < 0) {
            break;
        }
        std::uint32_t found = 0;
        characters->getValue(row, "appear_point", found);
        if (found == point) {
            return {characters.get(), row};
        }
        start = static_cast<unsigned>(row) + 1;
    }
    return {};
}

void enemies::Manager::attachCharacterInfo(const jmp::ToolData& table, unsigned point) {
    *characters = table;
    for (unsigned i = 0; i < players; ++i) {
        auto& slot = appear[i];
        slot.source = findAppearPoint(point);
        slot.position = valid(slot.source) ? position(slot.source) : std::array<float, 3>{};
    }
}

void enemies::Manager::construct(unsigned index, unsigned type) {
    auto& slot = entries.at(index);
    if (slot.owner) {
        throw std::runtime_error("Original actor slot is already constructed.");
    }
    auto owner = runtime.constructOwner(creation.ownerConstructor(type), type);
    if (!owner) {
        throw std::runtime_error("Original character owner construction failed.");
    }
    auto& actor = runtime.constructActor(type, creation.capacity(type));
    owner->attach(actor, *this);
    slot.owner = std::move(owner);
    slot.category = creation.category(actor.type());
    slot.state = 2;
    slot.visible = false;
}

void enemies::Manager::initializeActors() {
    unsigned total = players;
    for (const auto& pool : creation.pools()) {
        if (pool.count > entries.size() - total) {
            throw std::runtime_error("Original actor pools exceed slot storage.");
        }
        total += pool.count;
    }
    for (const auto& slot : entries) {
        if (slot.owner) {
            throw std::runtime_error("Actor initialization would overwrite existing owners.");
        }
    }
    initialized = true;
    for (unsigned i = 0; i < players; ++i) {
        appear[i].room = runtime.resolveRoom(appear[i].position);
    }
    runtime.resetActorIndex();
    unsigned next = 0;
    for (; next < players; ++next) {
        construct(next, 0);
        appear[next].actor = entries[next].owner->actor().id();
        entries[next].owner->source = appear[next].source;
        place(*entries[next].owner);
    }
    for (const auto& pool : creation.pools()) {
        for (unsigned i = 0; i < pool.count; ++i) {
            construct(next, pool.type);
            deactivate(entries[next++]);
        }
    }
}

void enemies::Manager::place(Owner& owner) {
    if (!valid(owner.source)) {
        return;
    }
    const auto location = position(owner.source);
    const auto angles = rotation(owner.source);
    owner.actor().setPosition(location);
    owner.actor().setRotation(angles);
    std::uint8_t invisible = 0;
    owner.source.table->getValue(owner.source.row, "invisible", invisible);
    owner.actor().visibility(invisible ? 0 : 255, invisible ? 0 : 255);
}

bool enemies::Manager::reactivate(Slot& slot, unsigned type) {
    slot.state = 2;
    slot.visible = false;
    const bool result = slot.owner->activate(type);
    if (result) {
        place(*slot.owner);
        slot.owner->beginStrategy();
    }
    return result;
}

void enemies::Manager::deactivate(Slot& slot) {
    if (!slot.owner) {
        throw std::runtime_error("Cannot deactivate an unconstructed original actor slot.");
    }
    slot.state = 1;
    slot.owner->deactivate();
    slot.owner->actor().setPosition({-32000, -32000, -32000});
}

void enemies::Manager::deactivate(unsigned slot) {
    deactivate(entries.at(slot));
}

enemies::Slot* enemies::Manager::activate(unsigned type, const jmp::ToolDataRef& source) {
    const auto category = creation.category(type);
    for (auto& slot : entries) {
        if (slot.state == 1 && slot.category == category) {
            slot.owner->source = source;
            if (reactivate(slot, type)) {
                return &slot;
            }
            deactivate(slot);
            return nullptr;
        }
    }
    return nullptr;
}

unsigned enemies::Manager::createActor(unsigned type, int strategy) {
    auto* slot = activate(type, {});
    if (!slot) {
        return 0xffffffff;
    }
    if (strategy >= 0) {
        slot->owner->replaceStrategy(static_cast<unsigned>(strategy));
    }
    return slot->owner->actor().id();
}

bool enemies::Manager::isPlayer(const jmp::ToolDataRef& source) const {
    if (!valid(source)) {
        return false;
    }
    std::string_view value;
    return source.table->getValue(source.row, "name", value) && value == "luige";
}

bool enemies::Manager::create(jmp::ToolDataRef& entry) {
    const auto request = enemycreation::makeRequest(entry, types);
    if (request.type & 0x80000000) {
        return false;
    }
    if (isPlayer(entry)) {
        return true;
    }
    std::uint32_t door = 0;
    if (entry.table->getValue(entry.row, "OpenDoorNo", door)) {
        for (unsigned player = 0; player < players; ++player) {
            if (runtime.doorOpened(player, door & 255)) {
                return false;
            }
        }
    }
    if ((types.flags(request.type) & 1) && runtime.typeCaptured(0, request.type)) {
        return false;
    }
    if (mapMode == 2) {
        const auto room = runtime.resolveRoom(position(entry));
        if (room != 0x46 && creation.suppressInClearedRoom(request.type) && runtime.roomCleared(room >> 24)) {
            return false;
        }
    }
    std::uint32_t percent = 100;
    if (entry.table->getValue(entry.row, "appear_percent", percent) && runtime.random() % 100 >= percent) {
        return false;
    }
    const auto createName = enemycreation::createName(request);
    if (createName && *createName != "----") {
        requests.append(request);
        return true;
    }
    return activate(request.type, entry) != nullptr;
}

enemies::Slot* enemies::Manager::findActive(const jmp::ToolDataRef& source) {
    for (auto& slot : entries) {
        if (slot.state == 2 && jmp::sameEntry(slot.owner->source, source)) {
            return &slot;
        }
    }
    return nullptr;
}

bool enemies::Manager::remove(jmp::ToolDataRef& entry) {
    if (enemycreation::resolveType(entry, types) & 0x80000000) {
        return false;
    }
    if (isPlayer(entry)) {
        return true;
    }
    for (unsigned i = 0; i < requests.size(); ++i) {
        if (jmp::sameEntry(requests.at(i).source, entry)) {
            requests.erase(i);
            break;
        }
    }
    auto* slot = findActive(entry);
    if (!slot) {
        return false;
    }
    deactivate(*slot);
    return true;
}

bool enemies::Manager::message(jmp::ToolDataRef& entry, const char* command) {
    if (enemycreation::resolveType(entry, types) & 0x80000000) {
        return false;
    }
    if (isPlayer(entry)) {
        return true;
    }
    if (!command) {
        throw std::runtime_error("Null original actor command.");
    }
    const std::string_view value(command);
    if (value != "start" && value != "stop") {
        return false;
    }
    auto* slot = findActive(entry);
    if (!slot) {
        return false;
    }
    slot->owner->nextStrategyState(value == "start" ? 0x104 : 0x103);
    return true;
}

void enemies::Manager::activateNamed(std::string_view name) {
    unsigned index = 0;
    while (index < requests.size()) {
        const auto request = requests.at(index);
        const auto value = enemycreation::createName(request);
        if (!value) {
            throw std::runtime_error("Original pending request has no create_name.");
        }
        if (*value != name) {
            ++index;
            continue;
        }
        if (activate(request.type, request.source)) {
            requests.erase(index);
        }
    }
}

void enemies::Manager::update(unsigned index) {
    auto& slot = entries.at(index);
    if (slot.state == 2) {
        slot.owner->update();
    }
}

void enemies::Manager::updateVisibility() {
    for (auto& slot : entries) {
        if (slot.state != 2) {
            continue;
        }
        const bool visible = runtime.roomVisible(slot.owner->actor().room(), 0);
        if (slot.visible && !visible) {
            slot.owner->event(10);
        }
        if (!slot.visible && visible) {
            slot.owner->event(11);
        }
        slot.visible = visible;
    }
    anyPlayerSpecialState();
}

bool enemies::Manager::anyPlayerSpecialState() {
    bool result = false;
    for (unsigned i = 0; i < players; ++i) {
        if (runtime.playerSpecialState(i)) {
            result = true;
        }
    }
    return result;
}

unsigned enemies::Manager::countNamed(std::string_view value) const {
    unsigned count = 0;
    for (const auto& slot : entries) {
        if (slot.state != 2) {
            continue;
        }
        const auto found = name(slot.owner->source);
        if (found && *found == value) {
            ++count;
        }
    }
    for (unsigned i = 0; i < requests.size(); ++i) {
        const auto found = enemycreation::createName(requests.at(i));
        if (!found) {
            throw std::runtime_error("Original pending request has no create_name.");
        }
        if (*found == value) {
            ++count;
        }
    }
    return count;
}

unsigned enemies::Manager::countRoom(unsigned room) {
    unsigned count = 0;
    for (const auto& slot : entries) {
        if (slot.state == 2 && creation.countsForRoom(slot.owner->actor().type()) && sameRoom(slot.owner->actor().room(), room)) {
            ++count;
        }
    }
    for (unsigned i = 0; i < requests.size(); ++i) {
        const auto& request = requests.at(i);
        if (creation.countsForRoom(request.type) && runtime.resolveRoom(position(request.source)) == room) {
            ++count;
        }
    }
    return count;
}

std::array<std::uint8_t, 262> enemies::Manager::resourceMask() {
    std::array<std::uint8_t, 381> mask{};
    for (auto& slot : entries) {
        if (slot.state == 2) {
            slot.owner->event(0x1d, &mask);
        }
    }
    for (unsigned i = 0; i < requests.size(); ++i) {
        mask.at(requests.at(i).type) = 1;
    }
    std::array<std::uint8_t, 262> result{};
    for (unsigned type = 0; type < mask.size(); ++type) {
        if (mask[type]) {
            result.at(creation.model(type)) = 1;
        }
    }
    return result;
}

enemies::Owner& enemies::Manager::owner(int slot) {
    return slot >= 0 && slot < 128 && entries[slot].owner ? *entries[slot].owner : runtime.absentOwner();
}

const std::array<enemies::Slot, 128>& enemies::Manager::slots() const {
    return entries;
}

const enemycreation::Requests& enemies::Manager::pending() const {
    return requests;
}

const enemies::AppearPoint& enemies::Manager::appearPoint(unsigned player) const {
    return appear.at(player);
}

bool enemies::Manager::actorsInitialized() const {
    return initialized;
}

unsigned enemies::Manager::playerCount() const {
    return players;
}

enemies::Owner* enemies::Manager::activeOwner(const Actor& actor) const {
    for (const auto& slot : entries) {
        if (slot.state == 2 && slot.owner && &slot.owner->actor() == &actor) return slot.owner.get();
    }
    return nullptr;
}

bool enemies::Manager::restorePlacement(Owner& owner) {
    if (!valid(owner.source)) return false;
    owner.actor().setPosition(position(owner.source));
    owner.actor().setRotation(rotation(owner.source));
    return true;
}

int enemies::Manager::nearestPlayer(const std::array<float, 3>& position, std::array<float, 3>* displacement) {
    if (players == 1 && !displacement) return 0;
    int selected = -1;
    float nearest = std::numeric_limits<float>::max();
    for (unsigned i = 0; i < players; ++i) {
        const auto player = playerPosition(i);
        const float x = player[0] - position[0], y = player[1] - position[1], z = player[2] - position[2];
        const float distance = std::sqrt((x * x + y * y) + z * z);
        if (distance < nearest) {
            nearest = distance;
            selected = static_cast<int>(i);
            if (displacement) *displacement = {x, y, z};
        }
    }
    return selected;
}

int enemies::Manager::nearPlayer(const std::array<float, 3>& position, float range) {
    std::array<float, 3> displacement;
    const int selected = nearestPlayer(position, &displacement);
    if (selected < 0) throw std::runtime_error("Original nearest-player query did not initialize its displacement.");
    const float nearest = std::sqrt((displacement[0] * displacement[0] + displacement[1] * displacement[1]) +
        displacement[2] * displacement[2]);
    return range <= nearest ? -1 : selected;
}

bool enemies::Manager::roomVisible(unsigned room) { return runtime.roomVisible(room, 0); }

std::array<float, 3> enemies::Manager::playerPosition(unsigned player) {
    if (player >= players) {
        throw std::out_of_range("Original player index out of range.");
    }
    return initialized ? runtime.playerPosition(player) : appear[player].position;
}

unsigned enemies::Manager::playerRoom(unsigned player) {
    if (player >= players) {
        throw std::out_of_range("Original player index out of range.");
    }
    return initialized ? runtime.playerRoom(player) : appear[player].room;
}
