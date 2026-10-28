#include "resources/dolresources.h"
#include "game/world/roomfurnitureowner.h"
#include "game/startup/startupresources.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
    template<class T> T constant(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, sizeof(T), bytes, error)) throw std::runtime_error(error);
        std::reverse(bytes.begin(), bytes.end());
        T value;
        std::memcpy(&value, bytes.data(), sizeof(value));
        return value;
    }

    template<class T> T field(const jmp::ToolData& table, unsigned row, const char* name) {
        T value{};
        if (!table.getValue(row, name, value)) throw std::runtime_error(std::string("Missing furniture field: ") + name);
        return value;
    }
}

roomfurniture::Owner::Owner(const std::vector<std::uint8_t>& executable, const roomlocation::Rooms& rooms,
    const jmp::ToolData& table, const std::array<std::uint8_t, 0x220>& inventory, unsigned map) : rooms(rooms), sourceTable(table),
    rows(table.count(), -1), states(table.count()) {
    std::string error;
    if (!poseTables.read(executable, error)) throw std::runtime_error(error);
    angleBias = constant<double>(executable, 0x804da138);
    angleScale = constant<double>(executable, 0x804da140);
    previousFrame = constant<float>(executable, 0x804d9c50);
    for (unsigned i = 0; i < spring.size(); ++i) spring[i] = constant<float>(executable, 0x804da1a8 + i * 4);
    quantization = constant<float>(executable, 0x804d9a7c);
    inverseQuantization = constant<float>(executable, 0x804d9a78);
    opacityDistance = constant<float>(executable, 0x804da16c);
    minimumOpacity = constant<std::uint8_t>(executable, 0x804d01b8);
    const double below = constant<float>(executable, 0x804da120);
    const double above = constant<float>(executable, 0x804da124);
    const double ceiling = constant<float>(executable, 0x804da128);
    for (unsigned row = 0; row < table.count(); ++row) {
        auto& state = states[row];
        if (map == 2 && (inventory.at(0x1a0 + row / 8) & (0x80 >> (row & 7)))) state.flags |= 8;
        const auto room = field<std::uint32_t>(table, row, "room_no") & 255;
        const auto found = std::find_if(rooms.entries().begin(), rooms.entries().end(),
            [&](const roomlocation::Room& entry) { return (entry.id & 255) == room; });
        if (found == rooms.entries().end()) throw std::runtime_error("Furniture state references an absent room.");
        const float y = field<float>(table, row, "pos_y");
        const double minimum = static_cast<float>(found->bounds[1]);
        const double maximum = static_cast<float>(found->bounds[4]);
        if (y <= static_cast<float>(minimum - below) || static_cast<float>(above + minimum) < y) {
            state.height = y <= static_cast<float>(above + minimum) || static_cast<float>(maximum - ceiling) <= y ? 2 : 1;
        }
        const auto behavior = field<std::uint32_t>(table, row, "move");
        if (behavior == 7 || behavior == 19) state.height = 2;
        else if (behavior == 17) state.height = 0;
        switch (behavior) {
        case 22: state.flags |= 0x10; break;
        case 19: state.flags |= 0x40; break;
        case 29: state.flags |= 0x80; break;
        case 33: state.flags |= 0x100; break;
        case 37: case 9: state.flags |= 0x200; break;
        }
    }
}

roomfurniture::Object roomfurniture::Owner::construct(const Placement& placement, const Resources& resources,
    const jmp::ToolData& table) const {
    Object result;
    result.row = placement.row;
    result.behavior = placement.behavior;
    switch (result.behavior) {
    case 0: case 7: case 15: case 17: case 20: case 22: case 37: break;
    default: throw std::runtime_error("Furniture owner requires its original initialization callback.");
    }
    const char* positions[]{"pos_x", "pos_y", "pos_z"};
    const char* directions[]{"dir_x", "dir_y", "dir_z"};
    const char* arguments[]{"arg0", "arg1", "arg2"};
    for (unsigned i = 0; i < 3; ++i) {
        result.position[i] = field<float>(table, result.row, positions[i]);
        const auto degrees = static_cast<std::int16_t>(static_cast<int>(field<float>(table, result.row, directions[i])));
        result.angles[i] = static_cast<std::int16_t>(static_cast<int>(angleBias + angleScale * degrees));
        result.arguments[i] = static_cast<int>(field<float>(table, result.row, arguments[i]));
    }
    result.room = rooms.resolve(result.position, 0);
    result.moveLevel = field<std::uint32_t>(table, result.row, "move_level");
    result.generate = field<std::uint32_t>(table, result.row, "generate");
    result.sound = field<std::uint32_t>(table, result.row, "furniture_sound");
    result.itemTable = field<std::uint32_t>(table, result.row, "item_table");
    result.generateCount = field<std::uint32_t>(table, result.row, "generate_num");
    result.counter = field<std::uint32_t>(table, result.row, "counter");
    result.itemOffset = static_cast<int>(field<float>(table, result.row, "item_offset_y"));
    result.flags = 2;
    if (!field<std::uint32_t>(table, result.row, "not_transparent")) result.flags |= 8;
    result.previousFrame = previousFrame;
    if (placement.model != 0xffff) {
        result.model = &resources.models().at(placement.model);
        result.matrices.resize(result.model->hierarchy.visits().size());
    }
    if (result.behavior == 15 || result.behavior == 20) {
        const auto name = std::string(field<std::string_view>(table, result.row, "dmd_name"));
        for (const auto* suffix : {"_0.anm", "_1.anm"}) {
            const auto found = std::find_if(resources.animations().begin(), resources.animations().end(),
                [&](const Animation& animation) { return animation.name == name + suffix; });
            if (found != resources.animations().end()) result.animations.push_back(&*found);
        }
    }
    return result;
}

void roomfurniture::Owner::create(const Resources& resources, const jmp::ToolData& table) {
    std::vector<Object> prepared;
    for (const auto& placement : resources.placements()) {
        if (rows.at(placement.row) != -1) throw std::runtime_error("Repeated furniture creation requires the original replacement lifecycle.");
        prepared.push_back(construct(placement, resources, table));
    }
    const auto available = std::count_if(objects.begin(), objects.end(), [](const Object& object) { return !(object.flags & 2); });
    if (prepared.size() > static_cast<std::size_t>(available)) throw std::runtime_error("Furniture pool exhaustion requires the original slot-zero reuse path.");
    for (auto& object : prepared) {
        const auto slot = std::find_if(objects.begin(), objects.end(), [](const Object& entry) { return !(entry.flags & 2); });
        rows[object.row] = static_cast<int>(slot - objects.begin());
        *slot = std::move(object);
    }
}

void roomfurniture::Owner::initializeObjects() {
    for (auto& object : objects) {
        if (!(object.flags & 2) || object.updates) continue;
        object.flags |= 5;
        ++object.updates;
        refreshPose(object);
    }
}

void roomfurniture::Owner::refreshPose(Object& object) {
    if (!object.model) return;
    object.world = poseTables.world(object.position, object.angles, object.scale);
    if (object.animations.empty()) object.matrices = object.model->hierarchy.staticPose(poseTables, object.world);
    else {
        const auto& animation = object.animations.at(object.animation)->animation;
        dooranimation::updateFrame(animation, object.frame);
        object.model->hierarchy.sampleInto(animation, poseTables, object.frame, object.world,
            object.matrices.data(), static_cast<unsigned>(object.matrices.size()));
    }
    object.frameDelta = object.frame - object.previousFrame;
    if (object.frameDelta < 0) object.frameDelta = 1;
    object.previousFrame = object.frame;
}

void roomfurniture::Owner::updateIdle(const roomtraversal::State& visibility, unsigned buffer) {
    for (auto& object : objects) {
        if (!(object.flags & 2)) continue;
        if (!object.updates) {
            object.flags |= 5;
        } else {
            const bool visible = visibility.marked(object.room >> 24, buffer, buffer);
            object.flags = visible ? object.flags | 1 : object.flags & ~1u;
            if (visible) {
                auto& state = states.at(object.row);
                if ((state.flags & 0x21) || state.occupant != -1)
                    throw std::runtime_error("Furniture interaction requires its reaction and event services.");
                if (object.behavior == 0 || object.behavior == 15 || object.behavior == 22) {
                    const auto angle = static_cast<std::int16_t>(object.angles[2]);
                    const auto distance = static_cast<std::int16_t>(-angle);
                    object.angles[2] = static_cast<std::int16_t>(distance < -256 ? angle - 256 : distance > 256 ? angle + 256 : 0);
                    state.flags &= ~4u;
                    if (object.behavior == 15) object.frame += 1;
                } else if (object.behavior == 7 || object.behavior == 20) {
                    for (unsigned axis = 0; axis < 2; ++axis) {
                        auto& position = object.swing[axis];
                        auto& velocity = object.swingVelocity[axis];
                        if (object.arguments[0] == 0 || object.arguments[0] == 1) {
                            const auto index = object.arguments[0] * 2;
                            position = inverseQuantization * static_cast<float>(static_cast<int>(double(quantization) * position));
                            velocity = static_cast<float>(double(spring[index + 1]) *
                                (velocity + static_cast<float>(-double(spring[index]) * position)));
                            position += velocity;
                        }
                        if (velocity != 0 || position != 0)
                            throw std::runtime_error("Moving furniture requires its swing sound and collection services.");
                        object.angles[axis * 2] = static_cast<int>(position);
                    }
                } else if (object.behavior == 17) {
                    object.angles[0] = object.angles[2] = 0;
                    state.flags &= ~4u;
                }
            }
        }
        ++object.updates;
        refreshPose(object);
    }
}

void roomfurniture::Owner::updateOpacity(float playerZ) {
    for (auto& object : objects) {
        if (!(object.flags & 2)) continue;
        object.opacity = 255;
        if (object.flags & 8) {
            const auto value = static_cast<int>(opacityDistance + playerZ - object.position[2]);
            if (value < 161) object.opacity = static_cast<std::uint8_t>(std::max(value, int(minimumOpacity)));
        }
    }
}

void roomfurniture::Owner::release(unsigned row) {
    auto& index = rows.at(row);
    if (index == -1) return;
    auto& object = objects.at(index);
    object.model = nullptr;
    object.animations.clear();
    object.matrices.clear();
    object.flags &= ~2u;
    index = -1;
}

void roomfurniture::Owner::releaseAll() {
    for (unsigned i = 0; i < rows.size(); ++i) release(i);
}

const roomfurniture::Object* roomfurniture::Owner::object(unsigned row) const {
    const auto index = rows.at(row);
    return index == -1 ? nullptr : &objects.at(index);
}

const std::array<roomfurniture::Object, 80>& roomfurniture::Owner::slots() const { return objects; }
const roomfurniture::RowState& roomfurniture::Owner::rowState(unsigned row) const { return states.at(row); }
const jmp::ToolData& roomfurniture::Owner::placementTable() const { return sourceTable; }
void roomfurniture::Owner::markScanned(unsigned row) { states.at(row).flags |= 0x20; }
void roomfurniture::Owner::markSheetInitialized(unsigned row) { states.at(row).flags |= 2; }
bool roomfurniture::Owner::hasScanOccupant(unsigned row) const { return states.at(row).occupant != -1; }
