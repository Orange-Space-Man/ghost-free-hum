#include "game/world/roomtraversalstate.h"

#include <stdexcept>

void roomtraversal::State::initialize(unsigned roomCount) {
    for (auto& buffer : buffers) {
        buffer.rooms.assign(roomCount & 0xff, Record{});
        buffer.count = 0;
    }
}

void roomtraversal::State::reset(unsigned buffer, std::int16_t roomCount) {
    auto& target = buffers.at(buffer);
    if (roomCount > static_cast<int>(target.rooms.size())) throw std::out_of_range("Room traversal reset count.");
    target.count = 0;
    for (int i = 0; i < roomCount; ++i) target.rooms[i].flags = 0;
}

void roomtraversal::State::mark(unsigned buffer, unsigned map, std::uint32_t roomId,
    std::uint32_t& roomFlags, std::uint16_t& savedRoomFlags,
    std::uint8_t depth, std::int8_t marker, std::uint8_t clippingMode,
    const Bounds& clippedBounds, const Bounds& fullBounds) {
    auto& target = buffers.at(buffer);
    const unsigned index = roomId >> 24;
    auto& entry = target.rooms.at(index);
    roomFlags |= 2;
    if (map == 2) savedRoomFlags |= 1;
    entry.flags |= 1;
    if (marker != -1) entry.flags |= 2;
    entry.depth = depth;
    entry.marker = marker;
    entry.bounds = clippingMode == 1 ? clippedBounds : fullBounds;
    if (target.count == 64) return;
    target.visits[target.count++] = static_cast<std::uint8_t>(index);
}

bool roomtraversal::State::marked(unsigned roomIndex, int buffer, unsigned currentBuffer) const {
    const unsigned selected = buffer == -1 ? currentBuffer : static_cast<unsigned>(buffer);
    return (buffers.at(selected).rooms.at(roomIndex & 0xff).flags & 1) != 0;
}

const roomtraversal::Record& roomtraversal::State::record(std::uint32_t roomId, unsigned buffer) const {
    return buffers.at(buffer).rooms.at(roomId >> 24);
}

std::int8_t roomtraversal::State::rawMarker(unsigned roomIndex, unsigned buffer) const {
    return buffers.at(buffer).rooms.at(roomIndex & 0xff).marker;
}

std::int8_t roomtraversal::State::visibleMarker(std::uint32_t roomId, unsigned buffer) const {
    const auto& entry = record(roomId, buffer);
    if ((entry.flags & 1) == 0) return 0;
    if ((entry.flags & 2) != 0) return entry.marker;
    return -1;
}

unsigned roomtraversal::State::visitCount(unsigned buffer) const {
    return buffers.at(buffer).count;
}

std::uint8_t roomtraversal::State::visit(unsigned buffer, unsigned index) const {
    const auto& target = buffers.at(buffer);
    if (index >= target.count) throw std::out_of_range("Room traversal visit index.");
    return target.visits[index];
}
