#include "roomfurnitureresponse.h"
#include "resources/dolresources.h"

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
}

roomfurniture::Response::Response(const std::vector<std::uint8_t>& executable, const characteritems::Admission& admission) :
    admission(admission) {
    std::string error;
    if (!trig.read(executable, error)) throw std::runtime_error(error);
    constexpr unsigned furnitureSoundTable = 0x8038eb8c;
    constexpr unsigned furnitureEmissionParameters = 0x804da150;
    for (unsigned i = 0; i < sounds.size(); ++i) sounds[i] = constant<unsigned>(executable, furnitureSoundTable + i * 4);
    ceilingOffset = constant<float>(executable, furnitureEmissionParameters);
    floorOffset = constant<float>(executable, furnitureEmissionParameters + 4);
    directionScale = constant<float>(executable, furnitureEmissionParameters + 8);
    floorSpeed = constant<float>(executable, furnitureEmissionParameters + 12);
    ceilingDirection = {constant<float>(executable, 0x804da130),
        constant<float>(executable, furnitureEmissionParameters + 16), constant<float>(executable, furnitureEmissionParameters + 20)};
    ceilingSpeed = constant<float>(executable, furnitureEmissionParameters + 24);
    suctionRadius = constant<float>(executable, 0x804da170);
    rotationRadius = constant<float>(executable, 0x804da1bc);
    rotationDecay = constant<float>(executable, 0x804da218);
    rotationMaximum = constant<float>(executable, 0x804da214);
    rotationVolume = constant<float>(executable, 0x804da198);
    portraitAmplitude = constant<float>(executable, 0x804da210);
    cabinetSpeed = constant<float>(executable, 0x804da168);
    cabinetMaximum = constant<float>(executable, 0x804da21c);
    cabinetRadius = constant<float>(executable, 0x804da1c8);
    cabinetHeight = constant<float>(executable, 0x804da1bc);
    cabinetOffset = constant<float>(executable, 0x804da178);
    cabinetContactOffset = constant<float>(executable, 0x804da250);
    cabinetDistanceSquared = constant<float>(executable, 0x804da1c0);
}

void roomfurniture::Response::chair(Object& object, RowState& row, bool visible, bool vacuumActive,
    unsigned frame, const ResponseServices& services) const {
    object.flags = visible ? object.flags | 1 : object.flags & ~1u;
    if (!visible) return;
    int strength = vacuumActive ? static_cast<int>(ceilingOffset * services.suctionStrength(suctionRadius, object.position)) : 0;
    if (!strength && vacuumActive) {
        auto point = object.position;
        point[0] += suctionRadius;
        point[1] += ceilingOffset;
        strength = static_cast<int>(ceilingOffset * services.suctionStrength(suctionRadius, point));
    }
    suction(object, strength, false, frame, services);
    strike(object, row, false, false, services);
    collect(object, row, false, services);
    occupant(object, row, false, services);
    if (row.flags & 0x20) {
        services.scanMessage(object.counter & 255);
        row.flags &= ~0x20u;
    }
}

void roomfurniture::Response::portrait(Object& object, RowState& row, bool visible, bool vacuumActive,
    bool speaking, unsigned frame, const ResponseServices& services) const {
    object.flags = visible ? object.flags | 1 : object.flags & ~1u;
    if (!visible) return;
    if (speaking) {
        object.angles[2] = static_cast<int>(portraitAmplitude * trig.sineAngle(object.updates * 0x4fb0));
        if (!(frame & 3)) services.sound(0x1878, object.position, nullptr);
        return;
    }
    object.angles[2] = 0;
    auto point = object.position;
    point[1] += ceilingOffset;
    strike(object, row, false, false, services);
    const int strength = vacuumActive ? static_cast<int>(ceilingOffset * services.suctionStrength(suctionRadius, point)) : 0;
    suction(object, strength, false, frame, services);
    collect(object, row, false, services);
    occupant(object, row, false, services);
    if (row.flags & 0x20) {
        services.scanMessage(object.counter & 255);
        row.flags &= ~0x20u;
    }
}

roomfurniture::Vector roomfurniture::Response::position(const Object& object, const RowState& row) const {
    auto result = object.position;
    if (row.height == 2) result[1] = (result[1] - ceilingOffset) + static_cast<float>(object.itemOffset);
    else if (row.height < 2) result[1] = (floorOffset + result[1]) + static_cast<float>(object.itemOffset);
    else throw std::runtime_error("Furniture emission requires an initialized height class.");
    return result;
}

roomfurniture::Vector roomfurniture::Response::direction(const Object& object) const {
    const auto elevation = static_cast<std::uint32_t>(object.angles[0]) + 0x2000;
    const auto heading = static_cast<std::uint32_t>(object.angles[1]);
    const float horizontal = static_cast<float>(double(directionScale) * trig.cosineAngle(elevation));
    return {horizontal * trig.sineAngle(heading), static_cast<float>(double(directionScale) * trig.sineAngle(elevation)),
        horizontal * trig.cosineAngle(heading)};
}

void roomfurniture::Response::dust(const Object& object, const Vector& point, const ResponseServices& services) const {
    services.effect(0x61, point);
    services.sound(0x1877, object.position, nullptr);
}

void roomfurniture::Response::appeared(const Vector& point, const ResponseServices& services) const {
    services.effect(0x54, point);
    services.effect(0x55, point);
}

void roomfurniture::Response::collect(Object& object, RowState& row, bool forced, const ResponseServices& services,
    const Vector* offset, const float* retainedLaunchSpeed) const {
    if (!forced && object.suctionTicks != 30) return;
    if (services.roomCleared(object.room >> 24) && row.occupant != -1) return;
    Vector point, velocity;
    float speed;
    if (offset) {
        for (unsigned i = 0; i < 3; ++i) point[i] = object.position[i] + (*offset)[i];
        velocity = direction(object);
        speed = retainedLaunchSpeed ? *retainedLaunchSpeed : 0;
    } else {
        point = position(object, row);
        velocity = row.height == 2 ? ceilingDirection : direction(object);
        speed = row.height == 2 ? ceilingSpeed : floorSpeed;
    }
    if (object.collectionFlags & 1) {
        dust(object, point, services);
        return;
    }
    const auto item = services.selectItem(object.itemTable);
    if (!admission.money(item)) {
        if (admission.nothing(item)) dust(object, point, services);
        else {
            const bool repeated = admission.redirected(item);
            const auto id = repeated && (object.collectionFlags & 2) ? 0xffffffffu : services.createItem(item, point);
            if (static_cast<std::int32_t>(id) < 0) dust(object, point, services);
            else appeared(point, services);
            if (repeated) {
                object.collectionFlags |= 2;
                return;
            }
        }
    } else if (row.flags & 8) dust(object, point, services);
    else {
        if (offset && !retainedLaunchSpeed && object.generate >= 1 && object.generate <= 3)
            throw std::runtime_error("Offset furniture scatter requires the original retained launch register.");
        if (object.generate == 1 || object.generate == 3)
            services.scatter(false, speed, point, velocity, object.generateCount & 255);
        if (object.generate == 2 || object.generate == 3)
            services.scatter(true, speed, point, velocity, object.generateCount & 255);
        if (object.generate >= 4 && object.generate <= 8) {
            constexpr unsigned types[]{0xc5, 0xc7, 0xc6, 0xcb, 0xc9};
            services.createType(types[object.generate - 4], point);
        }
        appeared(point, services);
        row.flags |= 8;
        services.persistCollected();
    }
    object.collectionFlags |= 1;
}

void roomfurniture::Response::occupant(Object& object, RowState& row, bool forced, const ResponseServices& services) const {
    if (row.occupant == -1) {
        row.flags &= ~4u;
        return;
    }
    if (row.flags & 2) return;
    if (!services.roomCleared(object.room >> 24)) {
        row.flags &= ~4u;
        return;
    }
    const auto point = position(object, row);
    if ((row.flags & 4) || forced) {
        services.releaseOccupant(object.row, point);
        row.flags &= ~4u;
    } else if (object.suctionTicks > 30) services.releaseOccupant(object.row, point);
}

unsigned roomfurniture::Response::strike(Object& object, RowState& row, bool animate, bool suppressCollection,
    const ResponseServices& services) const {
    if (!(row.flags & 1)) {
        object.strikeTicks = 0;
        return 0;
    }
    if (row.reactionTwo) {
        object.angles[2] = static_cast<int>(static_cast<float>(static_cast<std::int32_t>(object.moveLevel * 100)) *
            trig.sineAngle(object.updates * 2000));
        if (!(object.strikeTicks & 15)) {
            const auto index = object.sound & 255;
            services.sound(sounds[index > 25 ? 0 : index], object.position, nullptr);
        }
    } else if (object.strikeTicks > 11 && object.strikeTicks < 16) {
        object.angles[2] = static_cast<int>(static_cast<float>(static_cast<std::int32_t>(object.moveLevel * 80)) *
            trig.sineAngle(object.updates * 0x9000));
    }
    if (++object.strikeTicks < 20) return 1;
    row.flags &= ~1u;
    if (!suppressCollection) {
        collect(object, row, true, services);
        occupant(object, row, true, services);
    }
    if (animate && !object.animation) services.animation(object, 1);
    return 2;
}

void roomfurniture::Response::suction(Object& object, int strength, bool animate, unsigned frame,
    const ResponseServices& services) const {
    if (!strength || !object.moveLevel) {
        const auto angle = static_cast<std::int16_t>(object.angles[2]);
        const auto distance = static_cast<std::int16_t>(-angle);
        object.angles[2] = static_cast<std::int16_t>(distance < -256 ? angle - 256 : distance > 256 ? angle + 256 : 0);
        object.suctionTicks = 0;
        return;
    }
    const auto amplitude = static_cast<std::int32_t>(static_cast<std::uint32_t>(strength) * object.moveLevel);
    object.angles[2] = static_cast<int>(trig.sineAngle((object.updates & 255) * 0x9000) * static_cast<float>(amplitude));
    ++object.suctionTicks;
    if (!(frame & 3)) {
        const auto index = object.sound & 255;
        const float value = static_cast<float>(amplitude);
        services.sound(sounds[index > 25 ? 0 : index], object.position, &value);
    }
    if (animate && object.suctionTicks == 30 && !object.animation) services.animation(object, 1);
}

void roomfurniture::Response::basic(Object& object, RowState& row, bool visible, bool vacuumActive,
    unsigned animationKind, unsigned frame, const ResponseServices& services) const {
    if (object.behavior != 0 && object.behavior != 15 && object.behavior != 22 && object.behavior != 38)
        throw std::runtime_error("Furniture requires its selected behavior handler.");
    object.flags = visible ? object.flags | 1 : object.flags & ~1u;
    if (!visible) {
        if (object.behavior != 22) object.collectionFlags &= ~2u;
        return;
    }
    const bool animated = object.behavior == 15 || object.behavior == 38;
    const bool animate = animated && animationKind == 2;
    if (!animated) strike(object, row, false, false, services);
    auto point = object.position;
    if (!animated) point[1] = ceilingOffset + point[1];
    const auto strength = vacuumActive ? static_cast<int>(ceilingOffset * services.suctionStrength(suctionRadius, point)) : 0;
    suction(object, strength, animate, frame, services);
    if (animated) strike(object, row, animate, false, services);
    collect(object, row, false, services);
    occupant(object, row, false, services);
    if (object.behavior != 22 && (row.flags & 0x20)) {
        services.scanMessage(object.counter & 255);
        row.flags &= ~0x20u;
    }
    if (animated) object.frame += directionScale;
}

void roomfurniture::Response::rotating(Object& object, RowState& row, bool visible, bool vacuumActive,
    const ResponseServices& services) const {
    object.flags = visible ? object.flags | 1 : object.flags & ~1u;
    if (!visible) return;
    const auto strength = vacuumActive ? static_cast<int>(ceilingOffset * services.suctionStrength(rotationRadius, object.position)) : 0;
    if (!strength) {
        object.suctionTicks = 0;
        object.rotationAmplitude = std::max(object.rotationAmplitude - rotationDecay, 0.0f);
    } else {
        object.rotationAmplitude = std::min(object.rotationAmplitude + static_cast<float>(object.suctionTicks * 10), rotationMaximum);
        ++object.suctionTicks;
    }
    object.previousRotation = static_cast<float>(object.angles[0]);
    const unsigned angle = (object.updates & 7) << 13;
    object.angles[0] = static_cast<int>(object.rotationAmplitude * trig.sineAngle(angle)) & 0xffff;
    object.angles[2] = static_cast<int>(object.rotationAmplitude * trig.cosineAngle(angle)) & 0xffff;
    strike(object, row, false, false, services);
    collect(object, row, false, services);
    occupant(object, row, false, services);
    if (row.flags & 0x20) {
        services.scanMessage(object.counter & 255);
        row.flags &= ~0x20u;
    }
    if (static_cast<float>(object.angles[0]) < object.previousRotation) {
        const float volume = std::min(rotationVolume * object.rotationAmplitude, directionScale);
        services.sound(0x1868, object.position, &volume);
    }
}

void roomfurniture::Response::cabinet(Object& object, RowState& row, bool visible, bool vacuumActive,
    unsigned animationKind, unsigned frame, const Vector& playerPosition, const ResponseServices& services) const {
    basic(object, row, visible, vacuumActive, animationKind, frame, services);
    const auto kind = object.arguments[0];
    if (object.animation != 1) {
        if (kind == 1) services.cabinetActive(false);
        return;
    }
    if (kind == 2 || kind == 3) {
        const float radius = cabinetRadius + std::min(cabinetSpeed * object.frame, cabinetMaximum);
        services.contact(object.position, {radius, cabinetHeight, 0, cabinetContactOffset, 0});
    } else if (kind == 1) {
        const auto& matrix = object.matrices.at(1);
        const Vector point{matrix[3], matrix[7], matrix[11]};
        if (object.frame >= cabinetSpeed) {
            services.contact(point, {cabinetRadius, suctionRadius, 0, cabinetOffset, 0});
        } else {
            const float x = playerPosition[0] - point[0];
            const float y = (ceilingOffset + playerPosition[1]) - point[1];
            const float z = playerPosition[2] - point[2];
            if ((z * z + x * x) + y * y < cabinetDistanceSquared) services.hurtPlayer(point, 10, 6);
        }
        if (object.arguments[1]) services.setFlag(static_cast<unsigned>(object.arguments[1]) & 255);
        services.cabinetActive(true);
    }
}
