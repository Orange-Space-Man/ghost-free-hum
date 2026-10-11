#include "game/menu/menulayoutgraphics.h"
#include "resources/roomarchivereader.h"
#include "resources/gameassetpackage.h"

#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace {
    using Matrix = std::array<float, 6>;

    std::array<float, 4> color(std::uint32_t value) {
        return {((value >> 24) & 255) / 255.0f, ((value >> 16) & 255) / 255.0f,
            ((value >> 8) & 255) / 255.0f, (value & 255) / 255.0f};
    }

    Matrix concatenate(const Matrix& a, const Matrix& b) {
        return {a[0]*b[0]+a[2]*b[1], a[1]*b[0]+a[3]*b[1],
            a[0]*b[2]+a[2]*b[3], a[1]*b[2]+a[3]*b[3],
            a[0]*b[4]+a[2]*b[5]+a[4], a[1]*b[4]+a[3]*b[5]+a[5]};
    }

    Matrix transform(const menulayout::Pane& pane) {
        const float radians = -static_cast<float>(pane.rotation) * 0.017453292519943295f;
        const float cosine = std::cos(radians), sine = std::sin(radians);
        const int pivotX = pane.anchor % 3 == 1 ? pane.width / 2 : pane.anchor % 3 == 2 ? pane.width : 0;
        const int pivotY = pane.anchor / 3 == 1 ? pane.height / 2 : pane.anchor / 3 == 2 ? pane.height : 0;
        return {cosine, sine, -sine, cosine, pane.x + pivotX - cosine * pivotX + sine * pivotY,
            pane.y + pivotY - sine * pivotX - cosine * pivotY};
    }
}

bool menulayout::Graphics::loadOptionsPortrait(const std::filesystem::path& packagePath, std::string& error) {
    gameassets::Package package;
    std::vector<std::uint8_t> data, decoded, archive;
    roommaterials::Texture texture;
    if (!package.open(packagePath, error) || !package.read("files/Game/game_usa.szp", data, error) ||
        !roomarchive::decompress(data, archive, error) ||
        !roomarchive::read(archive, "kawano/dmman/pressed/hakase_icon.bti", data, error)) return false;
    if (data.size() >= 4 && data[0] == 'Y' && data[1] == 'a' && data[2] == 'y' && data[3] == '0') {
        if (!roomarchive::decompress(data, decoded, error)) return false;
        data = std::move(decoded);
    }
    if (!startupresources::decodeTexture(data, texture, error)) return false;
    names["optionsPortrait"] = textures.size();
    textures.push_back(std::move(texture));
    return true;
}

void menulayout::Graphics::setFont(const gamefont::Font& value) {
    if (fontOffset) textures.resize(fontOffset);
    font = value;
    fontOffset = textures.size();
    textures.insert(textures.end(), font.textures.begin(), font.textures.end());
}

bool menulayout::Graphics::load(const std::vector<std::uint8_t>& archive, const Layout& layout, std::string& error,
    const std::vector<std::string>& additionalTextures) {
    error.clear();
    try {
        Graphics result;
        roommaterials::Texture white;
        white.width = white.height = 1;
        white.pixels = {255, 255, 255, 255};
        result.textures.push_back(std::move(white));
        auto textureNames = additionalTextures;
        for (const auto& pane : layout.panes) {
            if (!pane.palette.empty()) throw std::runtime_error("External menu palettes are not supported.");
            textureNames.insert(textureNames.end(), pane.textures.begin(), pane.textures.end());
        }
        for (const auto& name : textureNames) {
            if (name.empty() || name == "optionsPortrait" || result.names.count(name)) continue;
            std::string path = "timg/" + name;
            for (auto& character : path) if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
            std::vector<std::uint8_t> data;
            roommaterials::Texture texture;
            if (!roomarchive::read(archive, path, data, error) || !startupresources::decodeTexture(data, texture, error)) {
                throw std::runtime_error(path + ": " + error);
            }
            result.names[name] = result.textures.size();
            result.textures.push_back(std::move(texture));
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

std::vector<startupscene::Picture> menulayout::Graphics::pictures(const Layout& layout) const {
    std::vector<startupscene::Picture> result;
    std::vector<Matrix> matrices(layout.panes.size());
    std::vector<bool> visible(layout.panes.size());
    for (const auto paneIndex : layout.drawOrder()) {
        const auto& pane = layout.panes[paneIndex];
        auto matrix = transform(pane);
        bool shown = pane.visible;
        if (pane.parent >= 0) {
            matrix = concatenate(matrices.at(pane.parent), matrix);
            shown = shown && visible.at(pane.parent);
        }
        matrices[paneIndex] = matrix;
        visible[paneIndex] = shown;
        if (!shown || pane.alpha == 0) continue;
        auto picture = [&](std::size_t texture, float x, float y, float width, float height,
            float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1) -> startupscene::Picture& {
            startupscene::Picture image;
            image.texture = texture + textureOffset;
            image.x = x;
            image.y = y;
            image.width = width;
            image.height = height;
            image.alpha = pane.alpha / 255.0f;
            image.black = color(pane.black);
            image.white = color(pane.white);
            image.transform = matrix;
            image.coordinates = {{{u0,v0}, {u1,v0}, {u0,v1}, {u1,v1}}};
            result.push_back(image);
            return result.back();
        };
        if (pane.kind == "TBX1" && !font.textures.empty() && !pane.text.empty()) {
            std::vector<std::vector<unsigned>> lines(1);
            std::vector<float> widths(1, 0);
            float cursor = 0;
            for (std::size_t index = 0; index < pane.text.size(); ++index) {
                unsigned character = static_cast<unsigned char>(pane.text[index]);
                if (character == 0) break;
                if (character == '\n') {
                    lines.emplace_back();
                    widths.push_back(0);
                    cursor = 0;
                    continue;
                }
                if (character < 32) throw std::runtime_error("Menu text control sequence requires additional support.");
                if (character > 127) {
                    if (++index >= pane.text.size()) throw std::runtime_error("Truncated menu text character.");
                    character = (character << 8) | static_cast<unsigned char>(pane.text[index]);
                }
                const auto& glyph = font.glyph(character);
                float advance = (glyph.advance + (lines.back().empty() ? glyph.left : 0)) * static_cast<float>(pane.fontWidth) / font.width;
                if (!lines.back().empty() && cursor + advance > pane.width) {
                    lines.emplace_back();
                    widths.push_back(0);
                    cursor = 0;
                    advance = (glyph.advance + glyph.left) * static_cast<float>(pane.fontWidth) / font.width;
                }
                lines.back().push_back(character);
                widths.back() = cursor + advance;
                cursor += advance + pane.characterSpacing;
            }
            const int textHeight = static_cast<int>(lines.size() - 1) * pane.lineSpacing + static_cast<int>(pane.fontHeight);
            const auto vertical = pane.textAlignment & 3, horizontal = (pane.textAlignment >> 2) & 3;
            float baseline = static_cast<float>(pane.fontHeight + (vertical == 0 ? (pane.height - textHeight) / 2 :
                vertical == 1 ? pane.height - textHeight : 0));
            for (std::size_t line = 0; line < lines.size(); ++line) {
                const int length = static_cast<int>(widths[line] + 0.5f);
                float x = static_cast<float>(horizontal == 0 ? (pane.width - length) / 2 : horizontal == 1 ? pane.width - length : 0);
                bool continuation = false;
                for (unsigned character : lines[line]) {
                    startupscene::Picture image;
                    x += font.draw(character, x, baseline, static_cast<float>(pane.fontWidth), static_cast<float>(pane.fontHeight), continuation, image) + pane.characterSpacing;
                    image.texture += fontOffset + textureOffset;
                    image.transform = matrix;
                    image.alpha = pane.alpha / 255.0f;
                    image.colors = {color(pane.colors[0]), color(pane.colors[0]), color(pane.colors[1]), color(pane.colors[1])};
                    result.push_back(image);
                    continuation = true;
                }
                baseline += pane.lineSpacing;
            }
        } else if (pane.kind == "PIC1" && (!pane.textures[0].empty() || pane.previewTexture >= 0)) {
            if (pane.binding != 15 || pane.wrap != 0) {
                throw std::runtime_error("Menu picture binding requires additional rendering support: " + pane.tag);
            }
            auto& image = picture(pane.previewTexture >= 0 ? 0 : names.at(pane.textures[0]), 0, 0, static_cast<float>(pane.width), static_cast<float>(pane.height),
                pane.mirror & 2 ? 1.0f : 0.0f, pane.mirror & 1 ? 1.0f : 0.0f,
                pane.mirror & 2 ? 0.0f : 1.0f, pane.mirror & 1 ? 0.0f : 1.0f);
            if (pane.previewTexture >= 0) image.texture = static_cast<std::size_t>(pane.previewTexture);
            for (unsigned corner = 0; corner < 4; ++corner) image.colors[corner] = color(pane.colors[corner]);
            if (pane.mirror & 4) {
                const auto coordinates = image.coordinates;
                image.coordinates = {coordinates[2], coordinates[0], coordinates[3], coordinates[1]};
            }
        } else if (pane.kind == "WIN1") {
            std::array<std::size_t, 4> indices;
            for (unsigned i = 0; i < 4; ++i) indices[i] = names.at(pane.textures[i]);
            const auto& first = textures.at(indices[0]);
            const auto& last = textures.at(indices[3]);
            if (pane.width < static_cast<int>(first.width + textures.at(indices[1]).width) ||
                pane.height < static_cast<int>(first.height + textures.at(indices[2]).height)) continue;
            auto& contents = picture(0, static_cast<float>(pane.content[0]), static_cast<float>(pane.content[1]),
                static_cast<float>(pane.content[2]), static_cast<float>(pane.content[3]));
            contents.black = {0,0,0,0};
            contents.white = {1,1,1,1};
            for (unsigned i = 0; i < 4; ++i) contents.colors[i] = color(pane.colors[i]);
            if (!pane.textures[4].empty()) throw std::runtime_error("Textured menu window contents require additional rendering support.");
            const float right = static_cast<float>(pane.width - last.width), bottom = static_cast<float>(pane.height - last.height);
            for (unsigned i = 0; i < 4; ++i) {
                const unsigned flags = (pane.mirror >> (6 - i * 2)) & 3;
                const auto& texture = textures.at(indices[i]);
                picture(indices[i], i & 1 ? right : 0, i & 2 ? bottom : 0,
                    static_cast<float>(texture.width), static_cast<float>(texture.height),
                    flags & 2 ? 1.0f : 0.0f, flags & 1 ? 1.0f : 0.0f,
                    flags & 2 ? 0.0f : 1.0f, flags & 1 ? 0.0f : 1.0f);
            }
            for (unsigned i : {1u, 3u}) {
                const unsigned flags = (pane.mirror >> (6 - i * 2)) & 3;
                const float u = flags & 2 ? 1.0f : 0.0f, v = flags & 1 ? 0.0f : 1.0f;
                picture(indices[i], static_cast<float>(first.width), i == 1 ? 0 : bottom,
                    right - first.width, static_cast<float>(textures.at(indices[i]).height), u, v, u, 1-v);
            }
            for (unsigned i : {2u, 3u}) {
                const unsigned flags = (pane.mirror >> (6 - i * 2)) & 3;
                const float u = flags & 2 ? 0.0f : 1.0f, v = flags & 1 ? 1.0f : 0.0f;
                picture(indices[i], i == 2 ? 0 : right, static_cast<float>(first.height),
                    static_cast<float>(textures.at(indices[i]).width), bottom - first.height, u, v, 1-u, v);
            }
        }
    }
    return result;
}
