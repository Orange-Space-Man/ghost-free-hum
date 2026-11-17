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
    choicePanel = load(archive, "/kawano/dmman/m_window1.bti");
    hand = load(archive, "/kawano/dmman/hand.bti");
    const auto layout = read(windowLayout, 32);
    auto half = [&](unsigned offset) { return static_cast<std::int16_t>((unsigned(layout[offset]) << 8) | layout[offset + 1]); };
    for (unsigned i = 0; i < choiceOrigin.size(); ++i) choiceOrigin[i] = half(24 + i * 2);
    choiceTextX = half(4);
    choiceTextY = half(6);
    choicePadding = half(12);
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
    this->textScale = scale;
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
    choicePictures(windows, result);
    return result;
}

const std::vector<roommaterials::Texture>& gameevent::MessageGraphics::textures() const { return images; }

void gameevent::MessageGraphics::choicePictures(const MessageWindows& windows, std::vector<startupscene::Picture>& pictures) const {
    const auto& choice = windows.choices();
    using Phase = MessageWindows::Phase;
    if (choice.phase == Phase::Hidden) return;
    int width = 0, height = 0;
    std::vector<int> heights;
    for (const auto& line : choice.lines) {
        const auto size = measure(line);
        width = std::max(width, static_cast<int>(size[0] * textScale));
        heights.push_back(static_cast<int>(size[1] * textScale));
        height += heights.back();
    }
    const unsigned origin = windows.window(2).phase == Phase::Hidden ? 0 : 2;
    const int x = choiceOrigin[origin], baseline = choiceOrigin[origin + 1];
    const bool animated = choice.phase == Phase::Opening || choice.phase == Phase::Closing;
    const int y = baseline - static_cast<int>(textScale * (animated ? font.ascent : font.ascent + font.descent));
    const float ratio = animated ? std::clamp(float(choice.timer) / (choice.phase == Phase::Opening ? openingFrames : closingFrames), 0.0f, 1.0f) : 0;
    const int center = y + (height + 70) / 2 - 30;
    const std::array<int, 4> xs{x - 40, x - 10, x + width + choicePadding + 10, x + width + choicePadding + 40};
    std::array<int, 4> ys{y - 30, y, y + height, y + height + 40};
    for (auto& value : ys) value = static_cast<int>(value + ratio * (center - value));
    const std::array<float, 4> us{0,30,210,240}, vs{0,30,80,120};
    const std::array<unsigned, 4> alpha{255, unsigned(32 + 223 * (height + 40) / (height + 70)), unsigned(32 + 223 * 40 / (height + 70)),32};
    for (unsigned row = 0; row < 3; ++row) for (unsigned column = 0; column < 3; ++column) {
        if (ys[row + 1] <= ys[row]) continue;
        startupscene::Picture panel;
        panel.texture = choicePanel;
        panel.x = static_cast<float>(xs[column]); panel.y = static_cast<float>(ys[row]);
        panel.width = static_cast<float>(xs[column + 1] - xs[column]); panel.height = static_cast<float>(ys[row + 1] - ys[row]);
        for (unsigned vertex = 0; vertex < 4; ++vertex) {
            panel.coordinates[vertex] = {us[column + (vertex & 1)] / images[choicePanel].width, vs[row + (vertex >> 1)] / images[choicePanel].height};
            panel.colors[vertex] = {108/255.0f,225/255.0f,95/255.0f,alpha[row + (vertex >> 1)]/255.0f};
        }
        pictures.push_back(panel);
    }
    if (choice.phase != Phase::Complete) return;
    int offsetY = 0;
    for (unsigned i = 0; i < choice.lines.size(); ++i) {
        const int tx = x + choiceTextX, ty = baseline + choiceTextY + offsetY;
        for (unsigned pass = 0; pass < 2; ++pass) {
            const int shadow = pass == 0 ? 2 : 0;
            auto text = gamefont::print(font, choice.lines[i], float(tx + choicePadding + shadow), float(ty + shadow),
                fontSize, fontSize, lineSpacing, pass == 0);
            for (auto& picture : text.pictures) { picture.texture += fontOffset; pictures.push_back(picture); }
            if (i == choice.cursor) {
                startupscene::Picture cursor;
                cursor.texture = hand; cursor.x = float(tx - 16 + shadow); cursor.y = float(ty - 21 + shadow);
                cursor.width = float(images[hand].width); cursor.height = float(images[hand].height);
                const std::array<float, 4> color{pass ? 1.0f : 0.0f,pass ? 1.0f : 0.0f,pass ? 1.0f : 0.0f,1};
                cursor.colors = {color,color,color,color}; pictures.push_back(cursor);
            }
        }
        offsetY += heights[i];
    }
}
