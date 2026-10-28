#include "playerfurniturescancache.h"
#include "resources/dolresources.h"

#include <cstring>
#include <stdexcept>

namespace {
    template<class T> T field(const jmp::ToolData& table, unsigned row, const char* name) {
        T value{};
        if (!table.getValue(row, name, value)) throw std::runtime_error(std::string("Missing furniture scan field: ") + name);
        return value;
    }

    float scalar(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
        float result;
        std::memcpy(&result, &bits, 4);
        return result;
    }

    actormodel::Matrix inverse(const actormodel::Matrix& matrix) {
        actormodel::Matrix result{};
        for (unsigned row = 0; row < 3; ++row) {
            for (unsigned column = 0; column < 3; ++column) result[row * 4 + column] = matrix[column * 4 + row];
            result[row * 4 + 3] = (result[row * 4] * -matrix[3] + result[row * 4 + 1] * -matrix[7]) +
                result[row * 4 + 2] * -matrix[11];
        }
        return result;
    }
}

playerscanner::FurnitureCache::FurnitureCache(const std::vector<std::uint8_t>& executable,
    roomfurniture::Owner& owner, const gameflags::State& flags) : owner(owner), flags(flags), poses(executable),
    maximum(scalar(executable, 0x804da9f0)), half(scalar(executable, 0x804da9fc)), angleScale(scalar(executable, 0x804daa00)) {}

bool playerscanner::FurnitureCache::refresh(unsigned nextRoom, const std::function<void(const SheetInput&)>& initializeSheet) {
    if (room == nextRoom) return false;
    const auto& table = owner.placementTable();
    auto selected = roomfurniture::selectRows(table, nextRoom, flags);
    std::vector<FurnitureTarget> next;
    std::vector<SheetInput> pending;
    std::array<int, 4> nextSheets{-1, -1, -1, -1};
    const char* positions[]{"pos_x", "pos_y", "pos_z"};
    const char* directions[]{"dir_x", "dir_y", "dir_z"};
    const char* sizes[]{"furniture_x", "furniture_y", "furniture_z"};
    for (unsigned index = 0; index < selected.size(); ++index) {
        const auto row = selected[index];
        Vector center, extent;
        std::array<std::int16_t, 3> angles;
        for (unsigned axis = 0; axis < 3; ++axis) {
            center[axis] = field<float>(table, row, positions[axis]);
            extent[axis] = half * static_cast<float>(field<std::uint32_t>(table, row, sizes[axis]));
            angles[axis] = static_cast<std::int16_t>(static_cast<std::int32_t>(angleScale * field<float>(table, row, directions[axis])));
        }
        const auto height = owner.rowState(row).height;
        if (height == 0) center[1] += extent[1];
        else if (height == 2) center[1] -= extent[1];
        auto matrix = poses.rotation(angles);
        for (unsigned axis = 0; axis < 3; ++axis) matrix[axis * 4 + 3] = center[axis];
        next.push_back({inverse(matrix), extent, extent[0] != 0 || extent[1] != 0 || extent[2] != 0});
        const auto sheet = field<std::uint32_t>(table, row, "sheet");
        if (sheet && pending.size() < nextSheets.size()) {
            const auto slot = static_cast<unsigned>(pending.size());
            nextSheets[slot] = static_cast<int>(index);
            pending.push_back({slot, index, row, sheet, field<std::uint32_t>(table, row, "sheet_texture"), matrix, extent,
                owner.hasScanOccupant(row), field<std::uint32_t>(table, row, "sheet_gum") != 0});
        }
    }
    if (!pending.empty() && !initializeSheet) throw std::runtime_error("Furniture scan cache requires original sheet initialization.");
    for (const auto& sheet : pending) {
        initializeSheet(sheet);
        owner.markSheetInitialized(sheet.row);
    }
    rows = std::move(selected);
    entries = std::move(next);
    sheets = nextSheets;
    room = nextRoom;
    return true;
}

playerscanner::Hit playerscanner::FurnitureCache::query(const Vector& position, const Vector& direction) const {
    return furnitureTarget(entries, position, direction, maximum);
}

void playerscanner::FurnitureCache::mark(unsigned target) { owner.markScanned(rows.at(target)); }
bool playerscanner::FurnitureCache::occupied(unsigned target) const { return owner.hasScanOccupant(rows.at(target)); }
unsigned playerscanner::FurnitureCache::row(unsigned target) const { return rows.at(target); }
const std::vector<playerscanner::FurnitureTarget>& playerscanner::FurnitureCache::targets() const { return entries; }

bool playerscanner::FurnitureCache::mirror(unsigned target) const {
    const auto row = rows.at(target);
    for (const auto sheet : sheets) if (sheet == static_cast<int>(target)) return false;
    return (owner.rowState(row).flags & 0x10) != 0;
}
