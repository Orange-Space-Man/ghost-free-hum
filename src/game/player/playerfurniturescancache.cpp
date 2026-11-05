#include "playerfurniturescancache.h"
#include "resources/dolresources.h"

#include <cstring>
#include <algorithm>
#include <cmath>
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
    maximum(scalar(executable, 0x804da9f0)), half(scalar(executable, 0x804da9fc)), angleScale(scalar(executable, 0x804daa00)),
    contactMargin(scalar(executable, 0x804daa10)), randomScale(scalar(executable, 0x804daa14)),
    inversionMargin(scalar(executable, 0x804daa18)), frontThreshold(scalar(executable, 0x80219ea8)) {}

bool playerscanner::FurnitureCache::refresh(unsigned nextRoom, const std::function<void(const SheetInput&)>& initializeSheet) {
    if (room == nextRoom) return false;
    const auto& table = owner.placementTable();
    auto selected = roomfurniture::selectRows(table, nextRoom, flags);
    std::vector<FurnitureTarget> next;
    std::vector<FurnitureContact> nextContacts;
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
        FurnitureContact contact{center, center[1], center[1], field<std::uint32_t>(table, row, "furniture_sound")};
        for (unsigned corner = 0; corner < 8; ++corner) {
            const float x = corner & 1 ? -extent[0] : extent[0];
            const float y = corner & 2 ? -extent[1] : extent[1];
            const float z = corner & 4 ? -extent[2] : extent[2];
            const float worldY = ((matrix[4] * x + matrix[5] * y) + matrix[6] * z) + matrix[7];
            contact.minimumY = std::min(contact.minimumY, worldY);
            contact.maximumY = std::max(contact.maximumY, worldY);
        }
        nextContacts.push_back(contact);
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
    contactBounds = std::move(nextContacts);
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
const std::vector<playerscanner::FurnitureContact>& playerscanner::FurnitureCache::contacts() const { return contactBounds; }

playerscanner::FurnitureInteraction playerscanner::FurnitureCache::press(const FurnitureInput& input,
    const std::array<std::uint8_t, 4>& activeSheets, const std::function<std::uint32_t()>& random) const {
    const float margin = contactMargin + input.radius;
    for (unsigned i = 0; i < entries.size(); ++i) {
        const auto& target = entries[i];
        const auto& contact = contactBounds[i];
        const auto flags = owner.rowState(rows[i]).flags;
        if (!target.enabled || (flags & 0x200) || contact.minimumY > input.position[1] + input.height ||
            input.position[1] > contact.maximumY) continue;
        Vector direction;
        for (unsigned axis = 0; axis < 3; ++axis) direction[axis] = contact.center[axis] - input.position[axis];
        if (((direction[0] * input.direction[0] + direction[1] * input.direction[1]) + direction[2] * input.direction[2]) <= frontThreshold)
            continue;
        const auto& matrix = target.inverse;
        const auto local = [&](unsigned axis) {
            return ((matrix[axis * 4] * input.position[0] + matrix[axis * 4 + 1] * input.position[1]) +
                matrix[axis * 4 + 2] * input.position[2]) + matrix[axis * 4 + 3];
        };
        if (!(std::abs(local(0)) < target.extent[0] + margin && std::abs(local(2)) < target.extent[2] + margin)) continue;
        unsigned reaction = 0;
        if (input.position[1] + input.reachHeight > contact.maximumY) reaction = 2;
        else if (flags & 0x40) reaction = 4;
        else if (flags & 0x100) {
            bool covered = false;
            for (unsigned slot = 0; slot < sheets.size(); ++slot)
                if (activeSheets[slot] && sheets[slot] == static_cast<int>(i)) covered = true;
            if (!covered) reaction = 5;
        } else {
            if (!random) throw std::runtime_error("Furniture reaction requires the original random stream.");
            const auto selection = static_cast<unsigned>(static_cast<float>(double(randomScale) * float(random())));
            if (selection < 3) reaction = selection + 1;
        }
        if (!reaction) continue;
        const float length = std::sqrt((direction[0] * direction[0] + direction[1] * direction[1]) + direction[2] * direction[2]);
        if (length >= 0.0001f) for (auto& value : direction) value *= 1 / length;
        else direction.fill(0);
        return {static_cast<int>(i), direction, reaction, contact.sound, false};
    }
    return {};
}

playerscanner::FurnitureInteraction playerscanner::FurnitureCache::automatic(const FurnitureInput& input) const {
    for (unsigned i = 0; i < entries.size(); ++i) {
        if (!entries[i].enabled || !(owner.rowState(rows[i]).flags & 0x80)) continue;
        const auto& contact = contactBounds[i];
        if (input.position[1] < contact.minimumY - inversionMargin || input.position[1] > contact.maximumY + inversionMargin)
            continue;
        if (input.inverted ? input.position[1] <= input.roomCenterY : input.position[1] >= input.roomCenterY) continue;
        const float x = contact.center[0] - input.position[0], z = contact.center[2] - input.position[2];
        if (std::sqrt(x * x + z * z) < contactMargin + input.radius)
            return {static_cast<int>(i), {}, 0, 0, true};
    }
    return {};
}

bool playerscanner::FurnitureCache::mirror(unsigned target) const {
    const auto row = rows.at(target);
    for (const auto sheet : sheets) if (sheet == static_cast<int>(target)) return false;
    return (owner.rowState(row).flags & 0x10) != 0;
}
