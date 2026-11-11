#include "game/events/eventmessagegraphics.h"
#include "game/menu/gamefontprint.h"
#include "resources/dolresources.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
    unsigned word(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 24) | (unsigned(bytes.at(offset + 1)) << 16) |
            (unsigned(bytes.at(offset + 2)) << 8) | bytes.at(offset + 3);
    }

    bool clip(startupscene::Picture& picture, float left, float top, float right, float bottom) {
        const float x0 = std::max(left, picture.x), y0 = std::max(top, picture.y);
        const float x1 = std::min(right, picture.x + picture.width), y1 = std::min(bottom, picture.y + picture.height);
        if (x0 >= x1 || y0 >= y1) return false;
        const float u0 = (x0 - picture.x) / picture.width, u1 = (x1 - picture.x) / picture.width;
        const float v0 = (y0 - picture.y) / picture.height, v1 = (y1 - picture.y) / picture.height;
        const auto uv = picture.coordinates;
        const auto colors = picture.colors;
        for (unsigned i = 0; i < 4; ++i) {
            const float u = i & 1 ? u1 : u0, v = i & 2 ? v1 : v0;
            for (unsigned j = 0; j < 2; ++j)
                picture.coordinates[i][j] = (uv[0][j] * (1 - u) + uv[1][j] * u) * (1 - v) +
                    (uv[2][j] * (1 - u) + uv[3][j] * u) * v;
            for (unsigned j = 0; j < 4; ++j)
                picture.colors[i][j] = (colors[0][j] * (1 - u) + colors[1][j] * u) * (1 - v) +
                    (colors[2][j] * (1 - u) + colors[3][j] * u) * v;
        }
        picture.x = x0;
        picture.y = y0;
        picture.width = x1 - x0;
        picture.height = y1 - y0;
        return true;
    }
}

std::size_t gameevent::MessageGraphics::load(const gamearchive::Archive& archive, const std::string& path) {
    const int resource = archive.findPathResource(path);
    if (resource < 0) throw std::runtime_error("Missing original message resource: " + path);
    roommaterials::Texture image;
    std::string error;
    if (!startupresources::decodeTexture(archive.resource(resource), image, error)) throw std::runtime_error(error);
    images.push_back(std::move(image));
    return images.size() - 1;
}

gameevent::MessageGraphics::MessageGraphics(const gamearchive::Archive& archive, const std::vector<std::uint8_t>& executable,
    const gamefont::Font& font) : font(font) {
    auto read = [&](unsigned address, unsigned size) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, size, bytes, error)) throw std::runtime_error(error);
        return bytes;
    };
    constexpr unsigned portraitNames = 0x803054a8, windowColors = 0x80305180, windowLayout = 0x804d0540;
    upper = load(archive, "/kawano/dmman/m_window4.bti");
    lower = load(archive, "/kawano/dmman/m_window5.bti");
    const auto names = read(portraitNames, static_cast<unsigned>(portraits.size()) * 4);
    for (unsigned i = 0; i < portraits.size(); ++i) {
        const auto name = read(word(names, i * 4), 96);
        portraits[i] = load(archive, std::string(name.begin(), std::find(name.begin(), name.end(), 0)));
    }
    const auto palette = read(windowColors, static_cast<unsigned>(colors.size()) * 4);
    for (unsigned i = 0; i < colors.size(); ++i)
        for (unsigned j = 0; j < 4; ++j) colors[i][j] = palette[i * 4 + j] / 255.0f;
    const auto layout = read(windowLayout, 24);
    auto half = [&](unsigned offset) { return static_cast<std::int16_t>((unsigned(layout[offset]) << 8) | layout[offset + 1]); };
    textX = half(0);
    textY = half(2);
    portraitX = half(8);
    portraitY = half(10);
    openingFrames = half(18);
    closingFrames = half(20);
    auto number = [&](unsigned address) {
        const auto bits = word(read(address, 4), 0);
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    };
    constexpr unsigned textScale = 0x804d0518, glyphSize = 0x804da2b4, lineSize = 0x804da2b8;
    const float scale = number(textScale);
    fontSize = static_cast<unsigned>(double(number(glyphSize)) * scale);
    lineSpacing = static_cast<int>(double(number(lineSize)) * scale);
    fontOffset = images.size();
    images.insert(images.end(), font.textures.begin(), font.textures.end());
}

std::array<float, 2> gameevent::MessageGraphics::measure(const std::string& text) const {
    return gamefont::print(font, text, 0, 0, font.width, font.ascent + font.descent, font.leading).size;
}

std::vector<startupscene::Picture> gameevent::MessageGraphics::pictures(const MessageWindows& windows) const {
    std::vector<startupscene::Picture> result;
    using Phase = MessageWindows::Phase;
    for (unsigned i = 0; i < 4; ++i) {
        const auto& window = windows.window(i);
        if (window.phase == Phase::Hidden) continue;
        const bool open = window.phase >= Phase::Revealing && window.phase <= Phase::Scrolling;
        startupscene::Picture panel;
        panel.texture = i == 2 ? lower : upper;
        panel.x = static_cast<float>(window.x);
        panel.y = static_cast<float>(window.y);
        panel.width = static_cast<float>(images[panel.texture].width);
        panel.height = static_cast<float>(images[panel.texture].height);
        const auto& tint = colors.at(window.color);
        panel.colors = {tint, tint, tint, tint};
        panel.colors[0][3] = panel.colors[1][3] = 1;
        panel.colors[2][3] = panel.colors[3][3] = 32 / 255.0f;
        if (!open) {
            const int duration = window.phase == Phase::Opening ? openingFrames : closingFrames;
            const int inset = window.timer * 120 / duration;
            panel.y += inset;
            panel.width = 480;
            panel.height = static_cast<float>(120 - inset);
        }
        if (panel.height > 0) result.push_back(panel);
        if (!open) continue;
        if (window.color > 0 && window.color < 20) {
            startupscene::Picture portrait;
            portrait.texture = portraits[window.color - 1];
            portrait.x = static_cast<float>(window.x + portraitX);
            portrait.y = static_cast<float>(window.y + portraitY);
            portrait.width = static_cast<float>(images[portrait.texture].width);
            portrait.height = static_cast<float>(images[portrait.texture].height);
            result.push_back(portrait);
        }
        const unsigned count = window.revealed - window.first - (window.phase == Phase::PageWait ? 1 : 0);
        const auto text = std::string_view(window.text).substr(window.first, count);
        for (unsigned pass = 0; pass < 2; ++pass) {
            const int offset = pass == 0 ? 2 : 0;
            auto layout = gamefont::print(font, text, static_cast<float>(window.x + portraitX + textX + offset),
                static_cast<float>(window.y + portraitY + textY - window.scroll + offset), fontSize, fontSize, lineSpacing, pass == 0);
            for (auto& picture : layout.pictures) {
                picture.texture += fontOffset;
                if (clip(picture, static_cast<float>(std::max(0, window.x)), static_cast<float>(window.y + 27),
                    static_cast<float>(std::max(0, window.x) + 480), static_cast<float>(window.y + 117))) result.push_back(picture);
            }
        }
    }
    return result;
}

const std::vector<roommaterials::Texture>& gameevent::MessageGraphics::textures() const { return images; }
