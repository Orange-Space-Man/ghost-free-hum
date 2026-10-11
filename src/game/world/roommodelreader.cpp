#include "game/world/roommodelreader.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace {
    constexpr std::size_t headerSize = 0x60;
    constexpr std::size_t nodeSize = 0x8c;

    std::uint32_t read32(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (static_cast<std::uint32_t>(data[offset]) << 24) |
            (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
            (static_cast<std::uint32_t>(data[offset + 2]) << 8) | data[offset + 3];
    }

    std::int32_t readLink(const std::vector<std::uint8_t>& data, std::size_t offset) {
        const std::uint32_t value = (static_cast<std::uint32_t>(data[offset]) << 8) | data[offset + 1];
        return value < 0x8000 ? static_cast<std::int32_t>(value) : static_cast<std::int32_t>(value) - 0x10000;
    }

    float readFloat(const std::vector<std::uint8_t>& data, std::size_t offset) {
        const std::uint32_t bits = read32(data, offset);
        float value = 0;
        static_assert(sizeof(value) == sizeof(bits));
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    bool readNodes(const std::vector<std::uint8_t>& data, std::size_t start, std::size_t end,
        std::vector<roommodel::Node>& nodes, std::string& error) {
        const std::size_t capacity = std::min((end - start) / nodeSize, static_cast<std::size_t>(0x8000));
        if (capacity == 0) {
            error = "Hierarchy does not contain a complete root record.";
            return false;
        }

        struct Visit {
            std::size_t index = 0;
            bool leaving = false;
        };

        std::vector<std::uint8_t> state(capacity, 0);
        std::vector<Visit> pending{{0, false}};
        while (!pending.empty()) {
            const Visit visit = pending.back();
            pending.pop_back();
            if (visit.leaving) {
                state[visit.index] = 2;
                continue;
            }
            if (state[visit.index] == 1) {
                error = "Hierarchy contains a cycle at node " + std::to_string(visit.index) + ".";
                return false;
            }
            if (state[visit.index] == 2) {
                continue;
            }

            const std::size_t offset = start + visit.index * nodeSize;
            roommodel::Node node;
            node.index = static_cast<std::uint16_t>(visit.index);
            node.link2 = readLink(data, offset + 2);
            node.link4 = readLink(data, offset + 4);
            for (std::size_t axis = 0; axis < 3; ++axis) {
                node.scale[axis] = readFloat(data, offset + 0x0c + axis * 4);
                node.rotation[axis] = readFloat(data, offset + 0x18 + axis * 4);
                node.translation[axis] = readFloat(data, offset + 0x24 + axis * 4);
                if (!std::isfinite(node.scale[axis]) || !std::isfinite(node.rotation[axis]) || !std::isfinite(node.translation[axis])) {
                    error = "Non-finite transform at node " + std::to_string(visit.index) + ".";
                    return false;
                }
            }

            state[visit.index] = 1;
            nodes.push_back(node);
            pending.push_back({visit.index, true});
            for (const std::int32_t link : {node.link4, node.link2}) {
                if (link == -1) {
                    continue;
                }
                if (link < 0 || static_cast<std::size_t>(link) >= capacity) {
                    error = "Hierarchy link outside section at node " + std::to_string(visit.index) + ".";
                    return false;
                }
                pending.push_back({static_cast<std::size_t>(link), false});
            }
        }
        return true;
    }
}

bool roommodel::inspect(const std::vector<std::uint8_t>& data, Model& model, std::string& error) {
    model = {};
    error.clear();
    if (data.size() < headerSize) {
        error = "File is shorter than the supported 0x60-byte header.";
        return false;
    }

    Model result;
    result.fileSize = data.size();
    result.firstByte = data[0];
    for (const std::uint32_t offset : {0x0cU, 0x10U, 0x14U, 0x18U, 0x1cU, 0x24U, 0x28U, 0x2cU, 0x30U, 0x34U, 0x38U, 0x3cU, 0x40U, 0x44U}) {
        Section section;
        section.headerOffset = offset;
        section.fileOffset = read32(data, offset);
        section.optional = offset == 0x18 || offset == 0x1c || (offset >= 0x24 && offset <= 0x30);
        if (section.fileOffset != 0 && (section.fileOffset < headerSize || section.fileOffset >= data.size())) {
            error = "Section offset outside payload at header byte " + std::to_string(offset) + ".";
            return false;
        }
        result.sections.push_back(section);
    }

    const std::size_t start = read32(data, 0x3c);
    if (start == 0) {
        error = "Missing hierarchy offset; this layout is not supported.";
        return false;
    }
    std::size_t end = data.size();
    for (const Section& section : result.sections) {
        if (section.fileOffset > start) {
            end = std::min(end, static_cast<std::size_t>(section.fileOffset));
        }
    }
    // The boundary is a safety limit, not a record count; padding may follow the hierarchy.
    if (!readNodes(data, start, end, result.nodes, error)) {
        return false;
    }
    model = std::move(result);
    return true;
}
