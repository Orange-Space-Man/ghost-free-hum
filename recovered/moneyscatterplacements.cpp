#include "moneyscatterplacements.h"

#include <stdexcept>
#include <utility>

namespace {
    bool matches(const jmp::ToolDataRef& entry) {
        std::string_view name;
        if (!entry.table || entry.row < 0 || !entry.table->getValue(entry.row, "name", name))
            throw std::runtime_error("Original coin placement requires its name field.");
        return name == "coin";
    }
}

moneyscatter::Placements::Placements(Owner& owner, const roomlocation::Rooms& rooms,
    const std::array<std::int16_t, 256>& roomIndices, const std::vector<std::uint32_t>& roomFlags,
    std::function<Vector(int)> retainedDirection) : owner(owner), rooms(rooms), roomIndices(roomIndices),
    roomFlags(roomFlags), retainedDirection(std::move(retainedDirection)) {
    if (!this->retainedDirection) throw std::runtime_error("Placed coins require the original retained direction scratch provider.");
}

bool moneyscatter::Placements::create(jmp::ToolDataRef& entry) {
    if (!matches(entry)) return false;
    std::uint32_t roomNumber;
    if (!entry.table->getValue(entry.row, "room_no", roomNumber))
        throw std::runtime_error("Original coin placement requires its room number.");
    const auto room = rooms.entries().at(static_cast<std::uint16_t>(roomIndices.at(roomNumber & 0xff)) & 0xff).id;
    if (!(roomFlags.at(room >> 24) & 0x40)) {
        Vector position;
        const std::array<const char*, 3> names{"pos_x", "pos_y", "pos_z"};
        for (unsigned i = 0; i < names.size(); ++i)
            if (!entry.table->getValue(entry.row, names[i], position[i]))
                throw std::runtime_error("Original coin placement reads an unwritten position.");
        position[1] += 10;
        if (owner.storage().particles.free.count) owner.place(position, entry.row, room, retainedDirection(entry.row));
    }
    return true;
}

bool moneyscatter::Placements::remove(jmp::ToolDataRef& entry) {
    if (!matches(entry)) return false;
    owner.storage().removePlacement(entry.row);
    return true;
}

bool moneyscatter::Placements::message(jmp::ToolDataRef&, const char*) { return false; }
