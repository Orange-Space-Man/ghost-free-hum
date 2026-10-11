#include "game/world/roomvisibilitywalk.h"

#include <stdexcept>

roomtraversal::Walk::Walk(const std::vector<SceneRoom>& rooms, const doordata::Table& portals, State& visibility,
    std::vector<std::uint32_t>& flags, std::vector<std::uint16_t>& savedFlags, unsigned map,
    const WalkServices& services) : rooms(rooms), portals(portals), visibility(visibility), flags(flags),
    savedFlags(savedFlags), map(map), services(services) {
    if (!services.doorBlocked) throw std::invalid_argument("Room traversal requires the original door state.");
    if (rooms.size() != flags.size()) throw std::invalid_argument("Room traversal owner count mismatch.");
}

std::vector<roomtraversal::Submission> roomtraversal::Walk::stationary(unsigned room, unsigned buffer,
    const roomportal::Matrix& projection, const Bounds& viewport, std::uint8_t clippingMode, std::uint8_t floorOpacity) {
    if ((rooms.at(room).attributes & 1) && floorOpacity) throw std::runtime_error("Room traversal requires the floor-overlap branch.");
    visibility.reset(buffer, static_cast<std::int16_t>(rooms.size()));
    std::vector<Submission> result;
    const auto mark = [&](unsigned index, unsigned depth, const Bounds& bounds) {
        const auto id = rooms.at(index).id;
        result.push_back({id, -1, bounds});
        auto unused = std::uint16_t{};
        auto& saved = map == 2 ? savedFlags.at(id & 255) : unused;
        visibility.mark(buffer, map, id, flags.at(index), saved, static_cast<std::uint8_t>(depth), -1,
            clippingMode, bounds, viewport);
    };
    std::function<void(unsigned, unsigned, const Bounds&)> walk;
    walk = [&](unsigned index, unsigned depth, const Bounds& bounds) {
        for (auto link : rooms.at(index).portals) {
            const auto& portal = portals.entries.at(link);
            if (!doordata::traversalEnabled(portal) || portal.rooms[0] != index || portal.attributes[1] == 3 ||
                portal.rooms[1] == room || services.doorBlocked(portal.identity)) continue;
            const auto clipped = roomportal::project(roomportal::corners(portal.center, portal.dimensions[0],
                portal.dimensions[1]), projection, viewport, bounds);
            if ((!clipped[0] && !clipped[2]) || (!clipped[1] && !clipped[3])) continue;
            mark(portal.rooms[1], depth + 1, clipped);
            walk(portal.rooms[1], depth + 1, clipped);
        }
    };
    mark(room, 0, viewport);
    walk(room, 0, viewport);
    for (auto link : rooms.at(room).portals) {
        const auto& portal = portals.entries.at(link);
        bool enabled = (portal.attributes[0] & 2) != 0;
        if (!enabled && (portal.attributes[0] & 4)) {
            if (portal.rooms[0] == room) {
                if (!services.centerVisible) throw std::runtime_error("Conditional room portal requires original center visibility.");
                enabled = services.centerVisible(portal.center);
            } else if (portal.rooms[1] == room) enabled = true;
        }
        if (!enabled || portal.attributes[1] == 3 || services.doorBlocked(portal.identity)) continue;
        const auto neighbor = portal.rooms[0] == room ? portal.rooms[1] : portal.rooms[0];
        mark(neighbor, 0, viewport);
        walk(neighbor, 0, viewport);
    }
    flags.at(room) &= ~1u;
    return result;
}
