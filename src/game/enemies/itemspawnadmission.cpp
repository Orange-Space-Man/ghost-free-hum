#include "resources/dolresources.h"
#include "game/enemies/itemspawnadmission.h"
#include "game/startup/startupresources.h"

#include <algorithm>
#include <stdexcept>

namespace {
    std::string_view name(const jmp::ToolDataRef& item) {
        std::string_view result;
        if (!item.table->getValue(item.row, "name", result)) throw std::runtime_error("Original item record has no name field.");
        return result;
    }

    bool bit(const playercomponents::BitBuffer* buffer, unsigned index) {
        if (!buffer || !buffer->data || index / 8 >= buffer->bytes) throw std::runtime_error("Original item admission bit buffer is unavailable.");
        return (buffer->data[index / 8] & (1u << (index & 7))) != 0;
    }
}

characteritems::Admission::Admission(const std::vector<std::uint8_t>& executable, const enemytypes::Catalog& types) : types(types) {
    const auto read = [&](unsigned address, auto& destination) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, destination.size() * 4, bytes, error)) throw std::runtime_error(error);
        for (unsigned i = 0; i < destination.size(); ++i) destination[i] = (unsigned(bytes[i * 4]) << 24) |
            (unsigned(bytes[i * 4 + 1]) << 16) | (unsigned(bytes[i * 4 + 2]) << 8) | bytes[i * 4 + 3];
    };
    read(0x8020c950, redirectTypes);
    read(0x8020c98c, keyTypes);
}

bool characteritems::Admission::nothing(const jmp::ToolDataRef& item) const {
    return !item.table || item.row < 0 || name(item) == "nothing";
}

bool characteritems::Admission::money(const jmp::ToolDataRef& item) const {
    return item.table && item.row >= 0 && name(item) == "money";
}

unsigned characteritems::Admission::type(const jmp::ToolDataRef& item) const {
    std::string_view value;
    if (!item.table || item.row < 0 || !item.table->getValue(item.row, "character_name", value)) return 0xffffffff;
    return types.find(value);
}

bool characteritems::Admission::key(unsigned type) const { return std::find(keyTypes.begin(), keyTypes.end(), type) != keyTypes.end(); }
bool characteritems::Admission::key(const jmp::ToolDataRef& item) const { return !nothing(item) && key(type(item)); }

bool characteritems::Admission::redirected(const jmp::ToolDataRef& item) const {
    if (key(item)) return true;
    const auto actorType = type(item);
    return std::find(redirectTypes.begin(), redirectTypes.end(), actorType) != redirectTypes.end();
}

bool characteritems::Admission::blocked(const jmp::ToolDataRef& item, const playeractor::View& player) const {
    if (key(item)) {
        std::uint32_t door;
        return item.table->getValue(item.row, "OpenDoorNo", door) && bit(player.flags, door & 255);
    }
    const auto actorType = type(item);
    return (types.flags(actorType) & 1) && bit(player.bits[0], actorType);
}
