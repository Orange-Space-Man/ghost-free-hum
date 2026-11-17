#include "game/scene/gameplaycamerafollow.h"
#include "resources/dolresources.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {
    constexpr unsigned mapDescriptors = 0x802ff218, presetTable = 0x802ffc48;
    constexpr unsigned leadScaleAddress = 0x804d9ce0, leadDecayAddress = 0x804d9ce4;
    constexpr unsigned insetAddress = 0x804d0098, heightThresholdAddress = 0x804d9cec;
    constexpr unsigned followRateAddress = 0x804d9cf0, absentRoomRateAddress = 0x804d9d04;
    constexpr unsigned nearThresholdAddress = 0x804d9d00, farThresholdAddress = 0x804d9cfc;
    constexpr unsigned reactionHeightAddress = 0x804d9d08, reactionDepthAddress = 0x804d9d0c;
    constexpr unsigned quantizationAddress = 0x804d9a7c, inverseQuantizationAddress = 0x804d9a78;
    constexpr unsigned smoothingScaleAddress = 0x804d9a8c, fieldOfViewAddress = 0x804d009c;
    constexpr unsigned coincidentOffsetAddress = 0x804d9cb8, nearDelayAddress = 0x804d00a0, normalDelayAddress = 0x804d00a1;
    constexpr unsigned nearPlaneAddress = 0x804d0000, farPlaneAddress = 0x804d0004;
    constexpr unsigned upHorizontalAddress = 0x804d9cd0, upVerticalAddress = 0x804d9d10;

    unsigned word(const std::vector<std::uint8_t>& executable, unsigned address) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, 4, bytes, error)) throw std::runtime_error(error);
        return (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) | (unsigned(bytes[2]) << 8) | bytes[3];
    }

    float scalar(const std::vector<std::uint8_t>& executable, unsigned address) {
        const auto bits = word(executable, address);
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    std::int32_t integer(float value) {
        if (!std::isfinite(value) || value < -2147483648.0f || value >= 2147483648.0f)
            return std::numeric_limits<std::int32_t>::min();
        return static_cast<std::int32_t>(value);
    }
}

gameplaycamera::Follow::Follow(const std::vector<std::uint8_t>& executable, unsigned map) {
    if (map != 1 && map != 2 && map != 3 && map != 6) throw std::runtime_error("Unsupported camera follow map.");
    gallery = map == 6;
    const auto descriptor = word(executable, mapDescriptors + map * 4);
    const auto count = word(executable, descriptor) >> 24;
    const auto records = word(executable, descriptor + 20);
    for (unsigned index = 0; index < count; ++index) {
        const auto address = records + index * 48;
        Room room{word(executable, address), word(executable, address + 4)};
        for (unsigned axis = 0; axis < 3; ++axis) {
            const auto lower = word(executable, address + 8 + axis * 4);
            const auto upper = word(executable, address + 20 + axis * 4);
            room.minimum[axis] = static_cast<float>(static_cast<std::int32_t>(lower));
            room.extent[axis] = static_cast<float>(static_cast<std::int32_t>(upper - lower));
        }
        rooms.push_back(room);
    }
    targetHeight = scalar(executable, 0x804d9d48);
    targetMinimumDepth = scalar(executable, 0x804d9d58);
    targetRate = scalar(executable, 0x804d9d14);
    captureFarDistance = scalar(executable, 0x804d9d6c);
    captureLookDistance = scalar(executable, 0x804d9da0);
    roomId = 0xffffffff;
    roomFlags = 0;
    for (unsigned mode = 0; mode < presets.size(); ++mode)
        for (unsigned field = 0; field < 4; ++field) presets[mode][field] = scalar(executable, presetTable + mode * 16 + field * 4);
    parameters = presets[0];
    nearDelay = word(executable, nearDelayAddress) >> 24;
    normalDelay = word(executable, normalDelayAddress) >> 24;
    leadScale = scalar(executable, leadScaleAddress);
    leadDecay = scalar(executable, leadDecayAddress);
    inset = scalar(executable, insetAddress);
    heightThreshold = scalar(executable, heightThresholdAddress);
    followRate = scalar(executable, followRateAddress);
    corridorDepth = scalar(executable, 0x804d9cf4);
    absentRoomRate = scalar(executable, absentRoomRateAddress);
    nearThreshold = scalar(executable, nearThresholdAddress);
    farThreshold = scalar(executable, farThresholdAddress);
    reactionHeight = scalar(executable, reactionHeightAddress);
    reactionDepth = scalar(executable, reactionDepthAddress);
    quantization = scalar(executable, quantizationAddress);
    inverseQuantization = scalar(executable, inverseQuantizationAddress);
    smoothingScale = scalar(executable, smoothingScaleAddress);
    fieldOfView = scalar(executable, fieldOfViewAddress);
    coincidentOffset = scalar(executable, coincidentOffsetAddress);
    nearPlane = static_cast<float>(static_cast<std::int32_t>(word(executable, nearPlaneAddress)));
    farPlane = static_cast<float>(static_cast<std::int32_t>(word(executable, farPlaneAddress)));
    up = {scalar(executable, upHorizontalAddress), scalar(executable, upVerticalAddress), scalar(executable, upHorizontalAddress)};
}

unsigned gameplaycamera::Follow::preset() const { return selected; }

void gameplaycamera::Follow::select(unsigned value, unsigned delay) {
    if (value == selected) return;
    if (selectionTicks < delay) { ++selectionTicks; return; }
    parameters = presets.at(value);
    selected = value;
    selectionTicks = 0;
}

void gameplaycamera::Follow::approach(std::array<float, 3>& value, const std::array<float, 3>& target, float rate) const {
    for (unsigned axis = 0; axis < 3; ++axis) {
        const float delta = static_cast<float>(integer(quantization * (target[axis] - value[axis])));
        const float step = (smoothingScale * inverseQuantization) * delta;
        value[axis] += static_cast<float>(double(step) * double(rate));
    }
}

void gameplaycamera::Follow::refreshRoom(unsigned room) {
    if (room != roomId && (room >> 24) != 255) {
        const auto& record = rooms.at(room >> 24);
        if (record.id != room) throw std::runtime_error("Camera follow requires a resolved room descriptor.");
        if ((record.flags && !(gallery && record.flags == 1)) || (room & 255) == 0xe ||
            (room & 255) == 0x29 || (room & 255) == 0x44)
            throw std::runtime_error("Camera follow requires the room's specialized camera dependencies.");
        roomId = room;
        roomFlags = record.flags;
        minimum = record.minimum;
        extent = record.extent;
    }
}

// 80020FAC: the aim point for a subject position (lead is the subject's current horizontal lead).
std::array<float, 3> gameplaycamera::Follow::aimFor(const std::array<float, 3>& subject, float lead, bool inverted) const {
    const float minimumX = minimum[0] + parameters[1];
    const float maximumX = (minimum[0] + extent[0]) - parameters[1];
    float x = subject[0] + lead;
    if (x < minimumX) x = minimumX;
    else if (maximumX < x) x = maximumX;
    float y = minimum[1] + parameters[2];
    if (inverted) y = (parameters[2] + minimum[1]) + extent[1];
    else if (heightThreshold < subject[1] - y) y = subject[1] + parameters[2];
    const float back = minimum[2] + extent[2];
    float z = back - inset;
    if (subject[2] < z) z = subject[2];
    return {x, y, z};
}

// 80020E8C: the eye for a subject position (cameraTargetY is the camera's current aim height).
std::array<float, 3> gameplaycamera::Follow::eyeFor(const std::array<float, 3>& subject, float lead, float cameraTargetY,
    float height, float depth) const {
    const float minimumX = minimum[0] + parameters[1];
    const float maximumX = (minimum[0] + extent[0]) - parameters[1];
    float x = subject[0] + lead;
    if (x < minimumX) x = minimumX;
    else if (maximumX < x) x = maximumX;
    const float back = minimum[2] + extent[2];
    float eyeZ = back + parameters[0];
    if (subject[2] < back - inset) eyeZ -= (back - inset) - subject[2];
    return {x, (height + cameraTargetY) + parameters[3], depth + eyeZ};
}

void gameplaycamera::Follow::update(CameraOwner& camera, const playeractor::View& player, unsigned room, unsigned playerIndex) {
    if (!player.position) throw std::runtime_error("Camera follow requires the published player position.");
    if (playerIndex >= leadX.size()) throw std::out_of_range("Camera player index exceeds the original subject table.");
    refreshRoom(room);
    const bool capturing = (camera.flags & 1) && camera.callback == captureCallback;
    if (((camera.flags & 5) && !capturing) || (capturing && (camera.flags & 4)) || player.field895 || player.field894)
        throw std::runtime_error("Camera follow requires the selected original scanner, ladder or override callback.");
    if (capturing) {
        updateCapture(camera, player, room, playerIndex);
        return;
    }
    camera.aspect = static_cast<float>(camera.viewport[2] - camera.viewport[0]) /
        static_cast<float>(camera.viewport[3] - camera.viewport[1]);
    camera.visibilityMode = 0;
    camera.fieldOfView = fieldOfView;
    camera.nearPlane = nearPlane;
    camera.farPlane = farPlane;
    camera.up = up;
    for (unsigned axis : {0u, 2u}) {
        auto& lead = axis ? leadZ[playerIndex] : leadX[playerIndex];
        if (!player.velocity) lead = 0;
        else {
            const float next = leadScale * (*player.velocity)[axis];
            lead = std::abs(next) < std::abs(lead) ? lead * leadDecay : next;
        }
    }
    if ((room >> 24) == 255) approach(camera.target, *player.position, absentRoomRate);
    else {
        const bool far = player.field89E && player.field8AC > farThreshold;
        if (player.field834 == 1) select(far ? 4 : 3, 0);
        else if (far) select(1, 0);
        else if (player.field89E && player.field8AC < nearThreshold && !(roomFlags & 3)) select(2, nearDelay);
        else select(0, normalDelay);
        // 8002134C stores the reaction offsets in globals (804D8278/804D8274) that the capture camera reuses.
        eyeHeight = player.field89C == 1 ? reactionHeight : 0;
        eyeDepth = player.field89C == 1 ? reactionDepth : 0;
        std::array<float, 3> target, eye;
        if (gallery && (roomFlags & 1)) {
            // 800210E4: Gallery corridor rooms keep the camera centered across the passage
            // and follow Luigi along its depth, rather than using the ordinary room clamp.
            const float x = minimum[0] + extent[0] * followRate;
            const float z = std::min((*player.position)[2], minimum[2] + extent[2] - inset);
            target = {x, minimum[1] + parameters[2], z};
            eye = {x, target[1] + eyeHeight,
                (*player.position)[2] + corridorDepth + eyeDepth + leadZ[playerIndex]};
        } else {
            target = aimFor(*player.position, leadX[playerIndex], player.field834 == 1);
            eye = eyeFor(*player.position, leadX[playerIndex], camera.target[1], eyeHeight, eyeDepth);
        }
        approach(camera.target, target, followRate);
        approach(camera.position, eye, followRate);
        // 8002134C: a capture target installs the capture camera override after the ordinary smoothing (8001FEBC).
        if (player.field8C9) {
            camera.flags |= 1;
            camera.callback = captureCallback;
            camera.visibilityMode = 0;
            camera.fieldE0 = 0;
        }
    }
    if (camera.target == camera.position) camera.target[2] = camera.position[2] + coincidentOffset;
}

// 80022BC4 with 80022950 (the 804D827C flag is never written, so the 30-tick approach branch 800222C4 is unreachable).
void gameplaycamera::Follow::updateCapture(CameraOwner& camera, const playeractor::View& player, unsigned room,
    unsigned playerIndex) {
    if ((room >> 24) == 255) throw std::runtime_error("The capture camera requires a resolved room descriptor.");
    if (!player.field8C9) {
        camera.flags &= 0xfff6; // 8001FF64(camera, 1): releases without restoring the saved view
        camera.callback = 0;
        if (camera.target == camera.position) camera.target[2] = camera.position[2] + coincidentOffset;
        return;
    }
    if (!player.field8CC) throw std::runtime_error("The capture camera requires the published capture target.");
    if (player.field834 == 1) select(0xb, 0);
    else select(2, 0);
    const auto& subject = *player.position;
    const auto& focus = *player.field8CC;
    const std::array<float, 3> offset{focus[0] - subject[0], focus[1] - subject[1], focus[2] - subject[2]};
    const float squared = offset[2] * offset[2] + offset[0] * offset[0] + offset[1] * offset[1];
    const float distance = squared > 0 ? std::sqrt(squared) : squared;
    const float inverse = 1 / std::sqrt(squared);
    const std::array<float, 3> direction{offset[0] * inverse, offset[1] * inverse, offset[2] * inverse};
    std::array<float, 3> look;
    if (captureFarDistance <= distance)
        for (unsigned axis = 0; axis < 3; ++axis) look[axis] = captureLookDistance * direction[axis] + subject[axis];
    else
        for (unsigned axis = 0; axis < 3; ++axis) look[axis] = (subject[axis] + focus[axis]) * followRate;
    leadX[playerIndex] = 0;
    leadZ[playerIndex] = 0;
    const std::array<float, 3> aim = aimFor(look, 0, player.field834 == 1);
    const std::array<float, 3> eye = eyeFor(look, 0, camera.target[1], eyeHeight, eyeDepth);
    approach(camera.target, aim, absentRoomRate);
    approach(camera.position, eye, absentRoomRate);
    ++camera.fieldE0;
    if (camera.target == camera.position) camera.target[2] = camera.position[2] + coincidentOffset;
}

void gameplaycamera::Follow::beginTarget(CameraOwner& camera) const {
    camera.flags |= 1;
    camera.callback = 0x80024620;
    camera.fieldE0 = 0;
    camera.fieldOfView = fieldOfView;
    camera.visibilityMode = 0;
}

void gameplaycamera::Follow::updateTarget(CameraOwner& camera, const std::array<float, 3>& player,
    const std::array<float, 3>& target, unsigned room) const {
    const auto& bounds = rooms.at(room >> 24);
    if (bounds.id != room) throw std::runtime_error("Target camera requires a resolved room descriptor.");
    const std::array<float, 3> difference{target[0] - player[0], target[1] - player[1], target[2] - player[2]};
    const float distance = std::sqrt(difference[2] * difference[2] + (difference[0] * difference[0] + difference[1] * difference[1]));
    const float vertical = difference[1] / distance;
    std::array<float, 3> aim{(player[0] + target[0]) * followRate,
        (heightThreshold + player[1] + target[1]) * followRate, (player[2] + target[2]) * followRate};
    std::array<float, 3> eye{aim[0], float(double(aim[1]) + double(targetHeight)), heightThreshold + aim[2]};
    const std::array<float, 3> offset{eye[0] - aim[0], eye[1] - aim[1], eye[2] - aim[2]};
    const float inverseLength = 1 / std::sqrt(offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2]);
    const float scale = float(double(distance) * double(absentRoomRate + absentRoomRate * std::abs(vertical)));
    for (unsigned axis = 0; axis < 3; ++axis) eye[axis] = aim[axis] + (offset[axis] * inverseLength) * scale;
    if (eye[2] < targetMinimumDepth + aim[2]) eye[2] = targetMinimumDepth + aim[2];
    const float rear = float(double(bounds.minimum[2] + bounds.extent[2]) + double(parameters[0]));
    if (rear < eye[2]) eye[2] = rear;
    if (eye[1] < player[1]) eye[1] = player[1];
    approach(camera.target, aim, targetRate);
    approach(camera.position, eye, targetRate);
}
