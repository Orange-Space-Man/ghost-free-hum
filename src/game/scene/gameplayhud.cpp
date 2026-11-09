#include "game/scene/gameplayhud.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    std::vector<std::uint8_t> read(const std::vector<std::uint8_t>& executable, unsigned address, unsigned size) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, size, bytes, error))
            throw std::runtime_error(error + " HUD address " + std::to_string(address));
        return bytes;
    }

    unsigned word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
            (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
    }

    std::string name(const std::vector<std::uint8_t>& executable, unsigned table, unsigned index) {
        const auto pointer = read(executable, table + index * 4, 4);
        const auto text = read(executable, word(pointer, 0), 96);
        return std::string(text.begin(), std::find(text.begin(), text.end(), 0));
    }
}

std::size_t gameplay::Hud::load(const gamearchive::Archive& archive, const std::string& path) {
    const int index = archive.findPathResource(path);
    if (index < 0) throw std::runtime_error("Missing original HUD resource: " + path);
    roommaterials::Texture image;
    std::string error;
    if (!startupresources::decodeTexture(archive.resource(index), image, error)) throw std::runtime_error(error);
    images.push_back(std::move(image));
    return images.size() - 1;
}

gameplay::Hud::Hud(const gamearchive::Archive& archive, const std::vector<std::uint8_t>& executable, const std::vector<unsigned>& rooms) {
    constexpr unsigned roomNameTable = 0x80305f20, lifeNumberTable = 0x80305020;
    for (const auto room : rooms) {
        if (roomTextures.count(room)) continue;
        const auto path = name(executable, roomNameTable, room);
        if (!path.empty()) roomTextures.emplace(room, load(archive, path));
    }
    for (unsigned i = 0; i < digits.size(); ++i) digits[i] = load(archive, name(executable, lifeNumberTable, i));
    hearts = {load(archive, "/kawano/lifemeter/heartD.bti"), load(archive, "/kawano/lifemeter/heartD2.bti"),
        load(archive, "/kawano/lifemeter/heartD_KAGE.bti")};
    const auto constants = read(executable, 0x804d052e, 14);
    healthAlpha = constants[0];
    const auto bits = word(constants, 10);
    std::memcpy(&digitScale, &bits, 4);
    const auto step = read(executable, 0x804d04e0, 2);
    slideStep = (unsigned(step[0]) << 8) | step[1];
    const auto table = read(executable, glme01::sineTable, 4096 * 4);
    sine.resize(4096);
    for (unsigned i = 0; i < sine.size(); ++i) {
        const auto value = word(table, i * 4);
        std::memcpy(&sine[i], &value, 4);
    }
}

void gameplay::Hud::update(const playeractor::View& player, bool enabled, bool roomCleared) {
    visible = enabled;
    cleared = roomCleared;
    life += life < player.life ? 1 : life > player.life ? -1 : 0;
    maximumLife += maximumLife < player.maximumLife ? 1 : maximumLife > player.maximumLife ? -1 : 0;
    reaction = player.field888;
    const unsigned period = reaction == 0 ? 60 : reaction == 1 ? 40 : reaction == 2 ? 30 : 20;
    pulsePeriod += pulsePeriod < period ? 1 : pulsePeriod > period ? -1 : 0;
    pulseAngle += 65536 / pulsePeriod;
    slide = enabled ? std::max(0, slide - int(slideStep)) : std::min(100, slide + int(slideStep));
    if (!enabled) {
        if (previousRoom != player.room) {
            selectedRoom = player.room & 255;
            roomPhase = roomTextures.count(selectedRoom) ? 1 : 0;
            roomCounter = roomDelay = 0;
            previousRoom = player.room;
        }
        return;
    }
    roomDelay = std::min(255u, roomDelay + 255u / (cleared ? 30 : 120));
    if (previousRoom != player.room) {
        roomDelay = 0;
        pendingRoom = player.room & 255;
        const bool unnamed = !roomTextures.count(pendingRoom);
        if (roomPhase <= 1) {
            if (unnamed) roomPhase = 0;
            else {
                selectedRoom = pendingRoom;
                roomPhase = 1;
                roomCounter = 0;
            }
        } else if (roomPhase >= 4) {
            if (selectedRoom == pendingRoom) roomPhase = 2;
            else if (roomPhase == 5 && unnamed) roomPhase = 4;
            else if (roomPhase == 4 && !unnamed) roomPhase = 5;
        } else {
            if (roomPhase == 3) roomCounter = 255;
            roomPhase = unnamed ? 4 : 5;
        }
        previousRoom = player.room;
    }
    if (roomPhase == 1 && roomDelay == 255) roomPhase = 2;
    else if (roomPhase == 2) {
        roomCounter += 5;
        if (roomCounter >= 255) { roomPhase = 3; roomCounter = 0; }
    } else if (roomPhase == 3) {
        if (++roomCounter > 60) { roomPhase = 4; roomCounter = 255; }
    } else if (roomPhase == 4 || roomPhase == 5) {
        roomCounter = roomCounter > 5 ? roomCounter - 5 : 0;
        if (!roomCounter) {
            if (roomPhase == 4) roomPhase = 0;
            else { selectedRoom = pendingRoom; roomPhase = 1; }
        }
    }
}

startupscene::Picture gameplay::Hud::picture(std::size_t texture, int x, int y, float scale,
    bool centered, std::array<float, 4> color) const {
    const auto& image = images.at(texture);
    startupscene::Picture result;
    result.texture = texture;
    result.width = float(int(image.width * scale));
    result.height = float(int(image.height * scale));
    result.x = float(x - (centered ? int(result.width) / 2 : 0));
    result.y = float(y - (centered ? int(result.height) / 2 : 0));
    result.colors.fill(color);
    return result;
}

std::vector<startupscene::Picture> gameplay::Hud::pictures() const {
    std::vector<startupscene::Picture> result;
    const float alpha = healthAlpha / 255.0f;
    const float pulse = std::abs(sine[(pulseAngle >> 4) & 4095]);
    const float maximum = 1 - pulse * (100 - maximumLife) / 100.0f;
    const float backScale = (maximum + std::sqrt(maximum)) * 0.5f;
    result.push_back(picture(hearts[2], 126, 437 + slide, backScale, true, {0, 0, 0, alpha}));
    if (life != 100 || maximumLife != 100)
        result.push_back(picture(hearts[1], 120, 429 + slide, backScale, true, {1, 1, 1, alpha}));
    const float front = life * (1 - pulse * (reaction ? 0.25f : 0.125f)) / 100.0f;
    const float frontScale = int(32 * ((front + std::sqrt(front)) * 0.5f)) / 32.0f;
    result.push_back(picture(hearts[0], 119, 429 + slide, frontScale, true, {1, 1, 1, alpha}));
    for (unsigned shadow = 0; shadow < 2; ++shadow) {
        const std::array<float, 4> color = shadow ? std::array<float, 4>{1, 12 / 255.0f, 0, alpha} :
            std::array<float, 4>{0, 0, 0, alpha};
        const int x = 142 + (shadow ? 0 : 2), y = 424 + slide + (shadow ? 0 : 2);
        if (life >= 100) result.push_back(picture(digits.at(life / 100), x, y, digitScale, false, color));
        if (life >= 10) result.push_back(picture(digits.at((life / 10) % 10), int(x + 22 * digitScale), y, digitScale, false, color));
        result.push_back(picture(digits.at(life % 10), int(x + 44 * digitScale), y, digitScale, false, color));
    }
    if (visible && roomPhase >= 2 && roomPhase <= 5) {
        const unsigned fade = roomPhase == 3 ? 255 : roomCounter;
        const float brightness = cleared ? 1 : 220 / 255.0f;
        result.push_back(picture(roomTextures.at(selectedRoom), 43, 44, 1, false,
            {brightness, brightness, brightness, float(160 * fade / 255) / 255}));
    }
    return result;
}

const std::vector<roommaterials::Texture>& gameplay::Hud::textures() const { return images; }
