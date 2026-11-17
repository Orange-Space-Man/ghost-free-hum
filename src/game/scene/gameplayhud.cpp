#include "game/scene/gameplayhud.h"
#include "resources/dolresources.h"
#include "resources/glme01addresses.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string_view>

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

gameplay::Hud::Hud(const gamearchive::Archive& archive, const std::vector<std::uint8_t>& executable, const std::vector<unsigned>& rooms, unsigned map) : locationEnabled(map == 2) {
    constexpr unsigned lifeNumberTable = 0x80305020, floorNameTable = 0x803058ec;
    for (const auto room : rooms) addRoom(archive, executable, room);
    for (unsigned i = 0; i < digits.size(); ++i) digits[i] = load(archive, name(executable, lifeNumberTable, i));
    for (unsigned i = 0; i < floors.size(); ++i) floors[i] = load(archive, name(executable, floorNameTable, i));
    ghostShocks = {load(archive, "/kawano/2dsub/ghost_life_a.bti"), load(archive, "/kawano/2dsub/ghost_life_b.bti")};
    const auto ghostScaleBits = word(read(executable, 0x804d0534, 4), 0);
    std::memcpy(&ghostScale, &ghostScaleBits, 4);
    hearts = {load(archive, "/kawano/lifemeter/heartD.bti"), load(archive, "/kawano/lifemeter/heartD2.bti"),
        load(archive, "/kawano/lifemeter/heartD_KAGE.bti")};
    demoPanelImages = {load(archive, "/kawano/base/cgbh.bti"),
        load(archive, "/kawano/base/cgbk_kage.tim"), load(archive, "/kawano/base/ledcover.tim")};
    std::string moneyError;
    const int moneyResource = archive.findPathResource("/kawano/list/blo/sgb_1.blo");
    if (moneyResource < 0 || !menulayout::read(archive.resource(moneyResource), moneyLayout, moneyError))
        throw std::runtime_error("Game Boy counter layout: " + moneyError);
    auto* backdrop = moneyLayout.find("BACK");
    backdrop->visible = false;
    backdrop->textures.fill({});
    auto* noise = moneyLayout.find("nois");
    noise->visible = false;
    moneyNoisePane = *noise;
    moneyNoiseColor = noise->white;
    moneyNoise = load(archive, "/kawano/base/timg/sgb_noise.bti");
    std::vector<std::string> extraDigits;
    for (unsigned i = 0; i < moneyDigits.size(); ++i) {
        const auto path = name(executable, 0x80338950, i);
        moneyDigits[i] = path.substr(path.find_last_of('/') + 1);
        extraDigits.push_back(moneyDigits[i]);
    }
    if (!moneyGraphics.load(archive, moneyLayout, moneyError, extraDigits)) throw std::runtime_error(moneyError);
    moneyGraphics.textureOffset = images.size();
    images.insert(images.end(), moneyGraphics.textures.begin(), moneyGraphics.textures.end());
    // The original scanner uses the gbf_1 Game Boy Horror sight layout.
    const int sightResource = archive.findPathResource("/kawano/base/blo/gbf_1.blo");
    if (sightResource < 0 || !menulayout::read(archive.resource(sightResource), scannerLayout, moneyError))
        throw std::runtime_error("Game Boy scanner layout: " + moneyError);
    auto* sightBackdrop = scannerLayout.find("BACK");
    if (!sightBackdrop) throw std::runtime_error("Game Boy scanner backdrop is missing.");
    sightBackdrop->visible = false;
    sightBackdrop->textures.fill({});
    if (!scannerGraphics.load(archive, scannerLayout, moneyError))
        throw std::runtime_error("Game Boy scanner graphics: " + moneyError);
    scannerGraphics.textureOffset = images.size();
    images.insert(images.end(), scannerGraphics.textures.begin(), scannerGraphics.textures.end());
    const int frameResource = archive.findPathResource("/kawano/base/blo/gbf_0.blo");
    if (frameResource < 0 || !menulayout::read(archive.resource(frameResource), scannerFrameLayout, moneyError) ||
        !scannerFrameGraphics.load(archive, scannerFrameLayout, moneyError))
        throw std::runtime_error("Game Boy scanner frame: " + moneyError);
    scannerFrameGraphics.textureOffset = images.size();
    images.insert(images.end(), scannerFrameGraphics.textures.begin(), scannerFrameGraphics.textures.end());
    const auto panelPositions = read(executable, 0x804d0524, 10);
    for (unsigned i = 0; i < demoPanelPositions.size(); ++i)
        demoPanelPositions[i] = static_cast<std::int16_t>((panelPositions[i * 2] << 8) | panelPositions[i * 2 + 1]);
    demoPanelY = demoPanelPositions[1];
    const auto constants = read(executable, 0x804d052e, 14);
    healthAlpha = constants[0];
    demoPanelMinimumAlpha = constants[1];
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

bool gameplay::Hud::addRoom(const gamearchive::Archive& archive, const std::vector<std::uint8_t>& executable, unsigned room) {
    if (!locationEnabled) return false;
    constexpr unsigned roomNameTable = 0x80305f20;
    if (roomTextures.count(room)) return false;
    const auto path = name(executable, roomNameTable, room);
    if (path.empty()) return false;
    const auto texture = load(archive, path);
    roomTextures.emplace(room, texture);
    return true;
}

void gameplay::Hud::update(const playeractor::View& player, bool fullscreen, bool roomCleared) {
    cleared = roomCleared;
    life += life < player.life ? 1 : life > player.life ? -1 : 0;
    maximumLife += maximumLife < player.maximumLife ? 1 : maximumLife > player.maximumLife ? -1 : 0;
    reaction = player.field888;
    const unsigned period = reaction == 0 ? 60 : reaction == 1 ? 40 : reaction == 2 ? 30 : 20;
    pulsePeriod += pulsePeriod < period ? 1 : pulsePeriod > period ? -1 : 0;
    pulseAngle += 65536 / pulsePeriod;
    slide = fullscreen ? std::min(100, slide + int(slideStep)) : std::max(0, slide - int(slideStep));
    constexpr int floorHeight = 550;
    const int height = static_cast<int>((*player.position)[1]);
    const unsigned floor = height < 0 ? (height / floorHeight == 0 ? 0 : 4) :
        static_cast<unsigned>(std::min(height / floorHeight, 3));
    if (floor != selectedFloor && height == (height / floorHeight) * floorHeight) {
        selectedFloor = floor;
        floorPhase = 2;
        floorCounter = 0;
    }
    if (floorPhase == 2) {
        floorCounter += 5;
        if (floorCounter >= 255) { floorPhase = 3; floorCounter = 0; }
    } else if (floorPhase == 3) {
        if (++floorCounter > 60) { floorPhase = 4; floorCounter = 255; }
    } else if (floorPhase == 4) {
        floorCounter = floorCounter > 5 ? floorCounter - 5 : 0;
        if (!floorCounter) floorPhase = 0;
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

void gameplay::Hud::updateDemoPanel(const playeractor::View& player, bool active, bool messageVisible, bool opened) {
    demoConversation = active;
    demoPanelActive = player.field861 && (active || opened || demoPanelY < demoPanelPositions[1]);
    ++moneyFrame;
    for (unsigned row = 0; row < 3; ++row) {
        const unsigned places = row == 2 ? 3 : 4;
        auto value = static_cast<unsigned>(std::clamp<std::int32_t>(static_cast<std::int32_t>(player.fieldsED4[row]), 0, row == 2 ? 999 : 9999));
        for (unsigned digit = 1; digit <= places; ++digit) {
            for (char shadow : {'a', 'b'}) {
                const auto tag = std::to_string(row + 1) + "n" + std::to_string(digit) + shadow;
                auto* pane = moneyLayout.find(tag);
                pane->textures[0] = moneyDigits[value % 10];
                pane->visible = digit == 1 || value != 0;
            }
            value /= 10;
        }
    }
    // Normal counters and conversation use the same sliding panel in 8003CFC0.
    const int target = !player.field861 || player.field895 || (!active && !opened) ? demoPanelPositions[1] :
        messageVisible ? demoPanelPositions[3] : demoPanelPositions[2];
    if (player.field895) demoPanelY = target;
    else {
        const int delta = demoPanelY - target;
        if (delta) demoPanelY -= delta / 4 + (delta < 0 ? -1 : 1);
    }
    demoPanelAlpha = demoPanelY > demoPanelPositions[2] ? demoPanelMinimumAlpha :
        demoPanelY > demoPanelPositions[3] ? demoPanelMinimumAlpha +
            (255 - demoPanelMinimumAlpha) * (demoPanelPositions[2] - demoPanelY) /
            (demoPanelPositions[2] - demoPanelPositions[3]) : 255;
}

std::array<int, 4> gameplay::Hud::demoViewport() const {
    // 8003CFC0 -> 80115474, including the original one-pixel viewport Y offset.
    return {demoPanelPositions[0] + 24, demoPanelY + 21, 110, 84};
}

std::vector<startupscene::Picture> gameplay::Hud::pictures() const {
    std::vector<startupscene::Picture> result;
    if (demoPanelActive) {
        const int x = demoPanelPositions[0];
        const float opacity = demoPanelAlpha / 255.0f;
        result.push_back(picture(demoPanelImages[1], x + 134, demoPanelY + 8, 1, false, {0, 0, 0, opacity}));
        result.push_back(picture(demoPanelImages[2], x + 24, demoPanelY - 27, 1, false, {1, 1, 1, opacity}));
        result.push_back(picture(demoPanelImages[0], x, demoPanelY, 1, false, {1, 1, 1, opacity}));
        if (!demoConversation) {
            const int screenX = x + 24, screenY = demoPanelY + 22;
            const auto* noise = &moneyNoisePane;
            const auto tint = std::array<float, 4>{((moneyNoiseColor >> 24) & 255) / 255.0f,
                ((moneyNoiseColor >> 16) & 255) / 255.0f, ((moneyNoiseColor >> 8) & 255) / 255.0f, 1};
            for (int y = 0; y < noise->height; y += 32) for (int column = 0; column < noise->width; column += 32) {
                auto image = picture(moneyNoise, screenX + noise->x + column, screenY + noise->y + y, 1, false, tint);
                image.width = float(std::min(32, noise->width - column));
                image.height = float(std::min(32, noise->height - y));
                const float u = image.width / 32, v = image.height / 32;
                const bool mirror = (moneyFrame / 2) % 2 == 0;
                const bool rotate = moneyFrame % 2 == 0;
                if (rotate) {
                    image.coordinates = mirror ? std::array<std::array<float, 2>, 4>{{{0, 0}, {0, u}, {v, 0}, {v, u}}} :
                        std::array<std::array<float, 2>, 4>{{{0, 1}, {0, 1 - u}, {v, 1}, {v, 1 - u}}};
                } else {
                    image.coordinates = mirror ? std::array<std::array<float, 2>, 4>{{{0, 1}, {u, 1}, {0, 1 - v}, {u, 1 - v}}} :
                        std::array<std::array<float, 2>, 4>{{{0, 0}, {u, 0}, {0, v}, {u, v}}};
                }
                image.alpha = opacity;
                result.push_back(image);
            }
            for (auto image : moneyGraphics.pictures(moneyLayout)) {
                image.transform[4] += float(screenX);
                image.transform[5] += float(screenY);
                image.alpha *= opacity;
                result.push_back(image);
            }
        }

    }
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
    if (roomPhase >= 2 && roomPhase <= 5) {
        const unsigned fade = roomPhase == 3 ? 255 : roomCounter;
        const float brightness = cleared ? 1 : 220 / 255.0f;
        result.push_back(picture(roomTextures.at(selectedRoom), 43, 44, 1, false,
            {brightness, brightness, brightness, float(160 * fade / 255) / 255}));
    }
    if (locationEnabled && floorPhase >= 2 && floorPhase <= 4) {
        const unsigned fade = floorPhase == 3 ? 255 : floorCounter;
        const float brightness = cleared ? 1 : 220 / 255.0f;
        result.push_back(picture(floors.at(selectedFloor), 537, 44, 1, false,
            {brightness, brightness, brightness, float(160 * fade / 255) / 255}));
    }
    return result;
}

std::vector<startupscene::Picture> gameplay::Hud::scannerPictures(bool discovered) const {
    auto result = scannerFrameGraphics.pictures(scannerFrameLayout);
    auto layout = scannerLayout;
    for (const auto tag : {"swin", "smes", "ster", "skey", "sque"})
        if (auto* pane = layout.find(tag)) pane->visible = discovered &&
            (std::string_view(tag) == "swin" || std::string_view(tag) == "smes" || std::string_view(tag) == "sque");
    auto sight = scannerGraphics.pictures(layout);
    result.insert(result.end(), sight.begin(), sight.end());
    return result;
}

const std::vector<roommaterials::Texture>& gameplay::Hud::textures() const { return images; }

void gameplay::Hud::updateGhosts(const std::vector<GhostHealth>& ghosts, unsigned frame) {
    ghostFrame = frame;
    for (auto& slot : ghostSlots) slot.seen = false;
    for (const auto& ghost : ghosts) {
        auto slot = std::find_if(ghostSlots.begin(), ghostSlots.end(), [&](const auto& value) { return value.ghost.owner == ghost.owner; });
        if (slot == ghostSlots.end()) {
            slot = std::find_if(ghostSlots.begin(), ghostSlots.end(), [](const auto& value) { return !value.ghost.owner; });
            if (slot == ghostSlots.end()) throw std::runtime_error("Original ghost health display slots exhausted.");
            slot->state = 1;
        }
        slot->ghost = ghost;
        slot->seen = true;
        ++slot->timer;
        if (slot->state == 1) {
            if (slot->timer > 60 || ghost.change < 0) {
                slot->state = 2;
                slot->timer = 0;
                slot->scale = ghost.change < 0 ? 1.5f : 1.2f;
            }
        } else if (slot->state == 2) {
            slot->scale = ghost.change < 0 ? 1.5f : std::max(1.0f, slot->scale - 0.04f);
        } else if (slot->state == 3) {
            slot->scale = slot->timer < 25 ? 1 : (50 - slot->timer) / 25.0f;
        }
        if (slot->state < 3 && ghost.captured) { slot->state = 3; slot->timer = 0; }
    }
    for (auto& slot : ghostSlots) if (!slot.seen) slot = {};
}

std::vector<startupscene::Picture> gameplay::Hud::ghostPictures(const std::array<float, 12>& view,
    const std::array<float, 16>& projection, const std::array<int, 4>& bounds) const {
    std::vector<startupscene::Picture> result;
    for (const auto& slot : ghostSlots) {
        if (!slot.ghost.owner || (slot.state != 1 && slot.state != 2)) continue;
        auto position = slot.ghost.position;
        position[1] -= 85;
        std::array<float, 4> camera{0, 0, 0, 1}, clip{};
        for (unsigned row = 0; row < 3; ++row) {
            camera[row] = view[row * 4 + 3];
            for (unsigned axis = 0; axis < 3; ++axis) camera[row] += view[row * 4 + axis] * position[axis];
        }
        for (unsigned row = 0; row < 4; ++row)
            for (unsigned axis = 0; axis < 4; ++axis) clip[row] += projection[row * 4 + axis] * camera[axis];
        if (clip[3] == 0) continue;
        int x = int(std::round((clip[0] / clip[3] + 1) * 320)), y = int(std::round((1 - clip[1] / clip[3]) * 240));
        const int ghostLife = slot.ghost.life;
        const int count = ghostLife >= 100 ? 3 : ghostLife >= 10 ? 2 : 1;
        const int right = bounds[2] - (int(ghostScale * (ghostLife < 1 ? 0 : count * 22)) + 12);
        if (!slot.ghost.attached && (x < bounds[0] - 50 || x > right + 50 || y < bounds[1] - 50 || y > bounds[3] + 50)) continue;
        x = std::clamp(x, bounds[0], right);
        y = std::clamp(y, bounds[1], bounds[3]);
        const float scale = ghostScale * slot.scale, t = 2 * (slot.scale - 1);
        auto lerp = [t](std::array<int, 4> a, std::array<int, 4> b) {
            std::array<float, 4> color;
            for (unsigned i = 0; i < 4; ++i) color[i] = int(a[i] + (b[i] - a[i]) * t) / 255.0f;
            return color;
        };
        const auto a = lerp({255, 255, 0, 128}, {255, 255, 0, 255});
        const auto b = lerp({200, 0, 255, 255}, {255, 255, 0, 255});
        const auto digitColor = slot.state == 1 ? std::array<float, 4>{1, 240 / 255.0f, 0, 1} :
            lerp({255, 240, 0, 255}, {255, 0, 255, 255});
        auto add = [&](std::size_t texture, float px, float py, float width, float height, std::array<float, 4> color) -> startupscene::Picture& {
            auto image = picture(texture, int(px), int(py), 1, false, color);
            image.width = float(int(width)); image.height = float(int(height));
            result.push_back(image);
            return result.back();
        };
        for (unsigned pass = 0; pass < (slot.state == 1 ? 2u : 1u); ++pass) {
            add(ghostShocks[1], x - 36 * scale, y - 36 * scale, 72 * scale, 72 * scale,
                slot.state == 1 && pass == 0 ? std::array<float, 4>{200 / 255.0f, 0, 1, 1} : b);
            auto& shock = add(ghostShocks[0], x - 32 * scale, y - 32 * scale, 64 * scale, 64 * scale, a);
            for (auto& uv : shock.coordinates) {
                if (ghostFrame % 2) uv = {uv[1], 1 - uv[0]};
                if ((ghostFrame / 2) % 2) uv[0] = 1 - uv[0];
            }
            int divisor = count == 3 ? 100 : count == 2 ? 10 : 1;
            for (int digit = 0; digit < count; ++digit, divisor /= 10) {
                const float dx = float(-9 * count + 18 * digit);
                const auto texture = digits.at((ghostLife / divisor) % 10);
                add(texture, x + (dx + 2) * scale, y - 10 * scale, 19 * scale, 26 * scale, {0, 0, 0, 1});
                add(texture, x + dx * scale, y - 12 * scale, 19 * scale, 26 * scale, digitColor);
            }
        }
    }
    return result;
}
