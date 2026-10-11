#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "jlib/j3d/j3dmaterialfactory.h"

#include <algorithm>
#include <stdexcept>

namespace {
    class Input {
    public:
        Input(const std::vector<std::uint8_t>& bytes, std::size_t start, std::size_t size) : bytes(bytes), start(start), size(size) {
            if (start > bytes.size() || size > bytes.size() - start) {
                throw std::runtime_error("J3D material block exceeds its resource.");
            }
        }

        std::uint8_t byte(std::size_t offset) const {
            check(offset, 1);
            return bytes[start + offset];
        }

        std::uint16_t half(std::size_t offset) const {
            check(offset, 2);
            return static_cast<std::uint16_t>((unsigned(byte(offset)) << 8) | byte(offset + 1));
        }

        std::uint32_t word(std::size_t offset) const {
            check(offset, 4);
            return (std::uint32_t(half(offset)) << 16) | half(offset + 2);
        }

        std::size_t table(unsigned field, unsigned index, unsigned stride, unsigned length) const {
            const auto base = word(field);
            if (!base) {
                throw std::runtime_error("J3D material references an absent table.");
            }
            const auto offset = std::size_t(base) + std::size_t(index) * stride;
            check(offset, length);
            return offset;
        }

        template<std::size_t Count> std::array<std::uint8_t, Count> copy(std::size_t offset) const {
            check(offset, Count);
            std::array<std::uint8_t, Count> result;
            std::copy_n(bytes.begin() + start + offset, Count, result.begin());
            return result;
        }

        void check(std::size_t offset, std::size_t length) const {
            if (offset > size || length > size - offset) {
                throw std::runtime_error("J3D material field exceeds its block.");
            }
        }

    private:
        const std::vector<std::uint8_t>& bytes;
        std::size_t start, size;
    };

    std::uint16_t colorChannel(const std::array<std::uint8_t, 8>& info) {
        const unsigned diffuse = info[4] ? info[3] : 0;
        const unsigned ambient = info[5] == 255 ? 0 : info[5];
        return static_cast<std::uint16_t>((unsigned(info[0]) << 1) | info[1] | (unsigned(info[2] & 15) << 2) |
            (unsigned(info[2] & 240) << 7) | (ambient << 6) | (diffuse << 7) |
            (unsigned(info[4] != 2) << 9) | (unsigned(info[4] != 0) << 10));
    }

    std::uint8_t operation(std::uint8_t old, const std::uint8_t* values) {
        unsigned result = (old & 0xfb) | (unsigned(values[0]) << 2);
        if (values[0] < 2) {
            result = (result & 0xcf) | (unsigned(values[2]) << 4);
            result = (result & 0xfc) | values[1];
        } else {
            result = (result & 0xcf) | ((values[0] & 6) << 3);
            result = (result & 0xfc) | 3;
        }
        result = (result & 0xf7) | (unsigned(values[3]) << 3);
        return static_cast<std::uint8_t>((result & 0x3f) | (unsigned(values[4]) << 6));
    }

    std::array<std::uint8_t, 8> stage(const std::array<std::uint8_t, 20>& info,
        const std::array<std::uint8_t, 2>& swap) {
        std::array<std::uint8_t, 8> result{};
        result[0] = operation(0, info.data() + 5);
        result[1] = static_cast<std::uint8_t>((info[1] << 4) | info[2]);
        result[2] = static_cast<std::uint8_t>((info[3] << 4) | info[4]);
        result[4] = operation(0, info.data() + 14);
        result[5] = static_cast<std::uint8_t>((info[10] << 5) | (info[11] << 2) | (info[12] >> 1));
        result[6] = static_cast<std::uint8_t>((info[12] << 7) | (info[13] << 4) | (swap[1] << 2) | swap[0]);
        return result;
    }

    class Factory {
    public:
        Factory(const Input& input, const j3dmodel::MaterialDefaults& defaults) : input(input),
            formats(defaults.formats, 0, defaults.formats.size()), registers(defaults.registers, 0, defaults.registers.size()),
            colors(defaults.colors, 0, defaults.colors.size()), old(input.word(0) == 0x4d415432) {
            if (!old && input.word(0) != 0x4d415433) {
                throw std::runtime_error("Unsupported original J3D material block.");
            }
            if (defaults.formats.size() != 0x190 || defaults.registers.size() != 0x38 || defaults.colors.size() != 0x20) {
                throw std::runtime_error("J3D material defaults have not been imported.");
            }
        }

        unsigned count() const {
            return input.half(8);
        }

        unsigned remap(unsigned index) const {
            return input.half(input.table(16, index, 2, 2));
        }

        j3dmodel::Material create(unsigned index, std::uint32_t flags) const {
            const auto record = input.table(12, remap(index), old ? 0x138 : 0x14c, old ? 0x138 : 0x14c);
            const unsigned shift = old ? 0 : 20;
            const auto half = [&](unsigned field) { return input.half(record + field); };
            const auto byteValue = [&](unsigned field, unsigned table, unsigned fallback) {
                const auto value = input.byte(record + field);
                return static_cast<std::uint8_t>(value == 255 ? fallback : input.byte(input.table(table, value, 1, 1)));
            };
            const auto pair = [&](unsigned field, unsigned table, unsigned slot) {
                return half(field + slot * 2) == 65535 ? std::size_t(0) :
                    input.table(table, half(field + slot * 2), 4, 4);
            };
            unsigned stageCount = byteValue(4, old ? 0x4c : 0x58, 0);
            unsigned textureCount = 0;
            for (unsigned i = 0; i < 8; ++i) {
                textureCount += half(0x70 + shift + i * 2) != 65535;
            }
            stageCount = std::max(stageCount, textureCount);
            stageCount = std::max(stageCount, (flags >> 16) & 31);
            if (stageCount > 16) {
                throw std::runtime_error("Original J3D TEV factory cannot allocate the requested stage count.");
            }
            j3dmodel::Material result;
            result.index = static_cast<std::uint16_t>(index);
            result.mode = input.byte(record);
            result.color = std::make_unique<j3dmodel::MaterialColorBlock>();
            result.texture = std::make_unique<j3dmodel::MaterialTextureBlock>();
            result.tev = std::make_unique<j3dmodel::MaterialTevBlock>();
            result.indirect = std::make_unique<j3dmodel::MaterialIndirectBlock>();
            result.pixel = std::make_unique<j3dmodel::MaterialPixelBlock>();
            auto& color = *result.color;
            color.lighting = (flags & 0x40000000) != 0;
            color.count = byteValue(2, old ? 0x20 : 0x24, 0);
            const auto cull = input.byte(record + 1);
            color.cull = cull == 255 ? 255 : static_cast<std::uint8_t>(input.word(input.table(old ? 0x18 : 0x1c, cull, 4, 4)));
            for (unsigned i = 0; i < 2; ++i) {
                const auto offset = pair(8, old ? 0x1c : 0x20, i);
                color.material[i] = offset ? input.copy<4>(offset) : colors.copy<4>(old ? 16 : 0);
                if (color.lighting) {
                    color.ambient[i] = registers.copy<4>(4);
                }
            }
            for (unsigned i = 0; i < 4; ++i) {
                const auto value = half(12 + i * 2);
                color.channels[i] = value == 65535 ? 65535 : colorChannel(input.copy<8>(input.table(old ? 0x24 : 0x28, value, 8, 8)));
            }
            auto& texture = *result.texture;
            texture.full = (flags & 0x4000000) != 0;
            texture.count = byteValue(3, old ? 0x28 : 0x34, 0);
            for (unsigned i = 0; i < 8; ++i) {
                auto value = half(0x14 + shift + i * 2);
                texture.coordinates[i].info = value == 65535 ? formats.copy<3>(4) :
                    input.copy<3>(input.table(old ? 0x2c : 0x38, value, 4, 3));
                value = half(0x24 + shift + i * 2);
                if (texture.full) {
                    const auto offset = value == 65535 ? 0 : input.table(old ? 0x30 : 0x3c, value, 8, 5);
                    texture.postCoordinates[i].info = offset ? input.copy<3>(offset) : formats.copy<3>(4);
                    texture.postCoordinates[i].matrix = offset ? input.half(offset + 3) : 65535;
                }
            }
            for (unsigned i = 0; i < 30; ++i) {
                const auto value = half((i < 10 ? 0x34 + i * 2 : 0x48 + (i - 10) * 2) + shift);
                if (value == 65535) {
                    continue;
                }
                const auto offset = input.table(i < 10 ? (old ? 0x34 : 0x40) : (old ? 0x38 : 0x44), value, 100, 100);
                auto matrix = std::make_unique<j3dmodel::TextureMatrix>();
                matrix->info = input.copy<100>(offset);
                if (i < 10) {
                    texture.matrices[i] = std::move(matrix);
                } else if (texture.full) {
                    texture.postMatrices[i - 10] = std::move(matrix);
                }
            }
            const auto nbt = half(0x136 + shift);
            texture.nbtScale = nbt == 65535 ? formats.copy<16>(0x180) : input.copy<16>(input.table(old ? 0x74 : 0x80, nbt, 16, 16));
            auto& tev = *result.tev;
            tev.capacity = stageCount < 2 ? 1 : stageCount < 3 ? 2 : stageCount < 5 ? 4 : 16;
            tev.count = byteValue(4, old ? 0x4c : 0x58, 255);
            tev.textures.assign(std::min(tev.capacity, 8U), 65535);
            tev.stages.resize(tev.capacity);
            for (unsigned i = 0; i < std::min(stageCount, 8U); ++i) {
                const auto value = half(0x70 + shift + i * 2);
                tev.textures[i] = value == 65535 ? 65535 : input.half(input.table(old ? 0x3c : 0x48, value, 2, 2));
            }
            for (unsigned i = 0; i < tev.capacity; ++i) {
                auto& target = tev.stages[i];
                target.order = formats.copy<3>(0xc8);
                target.registers = stage(formats.copy<20>(0x134), registers.copy<2>(0x1c));
                target.indirect = formats.copy<9>(0x148);
                if (i >= stageCount) {
                    continue;
                }
                auto value = half(0xa8 + shift + i * 2);
                target.order = value == 65535 ? formats.copy<3>(0xc8) : input.copy<3>(input.table(old ? 0x40 : 0x4c, value, 4, 3));
                value = half(0xd0 + shift + i * 2);
                const auto info = value == 65535 ? formats.copy<20>(0x134) : input.copy<20>(input.table(old ? 0x50 : 0x5c, value, 20, 20));
                value = half(0xf0 + shift + i * 2);
                const auto swap = value == 65535 ? registers.copy<2>(0x1c) : input.copy<2>(input.table(old ? 0x54 : 0x60, value, 4, 2));
                target.registers = stage(info, swap);
                if (tev.capacity > 1) {
                    target.colorSelection = input.byte(record + 0x88 + shift + i);
                    target.alphaSelection = input.byte(record + 0x98 + shift + i);
                }
            }
            if (tev.capacity > 1) {
                for (unsigned i = 0; i < 4; ++i) {
                    const auto value = half(0xc8 + shift + i * 2);
                    const auto data = value == 65535 ? colors.copy<8>(old ? 20 : 4) : input.copy<8>(input.table(old ? 0x44 : 0x50, value, 8, 8));
                    for (unsigned channel = 0; channel < 4; ++channel) {
                        tev.colors[i][channel] = static_cast<std::uint16_t>((unsigned(data[channel * 2]) << 8) | data[channel * 2 + 1]);
                    }
                    const auto constant = pair(0x80 + shift, old ? 0x48 : 0x54, i);
                    tev.constants[i] = constant ? input.copy<4>(constant) : colors.copy<4>(old ? 28 : 12);
                    const auto swap = pair(0x110 + shift, old ? 0x58 : 0x64, i);
                    const auto components = swap ? input.copy<4>(swap) : registers.copy<4>(0x20);
                    tev.swaps[i] = static_cast<std::uint8_t>(components[0] * 64 + components[1] * 16 + components[2] * 4 + components[3]);
                }
            }
            auto& pixel = *result.pixel;
            if (flags & 0x10000000) {
                if (result.mode & 1) pixel.kind = j3dmodel::MaterialPixelKind::Opaque;
                else if (result.mode & 2) pixel.kind = j3dmodel::MaterialPixelKind::TextureEdge;
                else if (result.mode & 4) pixel.kind = j3dmodel::MaterialPixelKind::Translucent;
            }
            const auto fog = half(0x130 + shift);
            if (fog != 65535) {
                auto value = std::make_unique<std::array<std::uint8_t, 44>>(input.copy<44>(input.table(old ? 0x5c : 0x68, fog, 44, 44)));
                if (pixel.kind == j3dmodel::MaterialPixelKind::Full) {
                    pixel.fog = std::move(value);
                }
            }
            if (pixel.kind == j3dmodel::MaterialPixelKind::Full) {
                const auto alpha = half(0x132 + shift), blend = half(0x134 + shift);
                if (alpha != 65535) {
                    const auto info = input.copy<5>(input.table(old ? 0x60 : 0x6c, alpha, 8, 5));
                    pixel.alpha = static_cast<std::uint16_t>(info[0] * 32 + info[2] * 8 + info[3]);
                    pixel.alphaReferences = {info[1], info[4]};
                }
                pixel.blend = blend == 65535 ? registers.copy<4>(0x24) : input.copy<4>(input.table(old ? 0x64 : 0x70, blend, 4, 4));
                const auto depth = input.byte(record + 6);
                if (depth != 255) {
                    const auto info = input.copy<3>(input.table(old ? 0x68 : 0x74, depth, 4, 3));
                    pixel.depth = static_cast<std::uint16_t>(info[1] * 2 + info[0] * 16 + info[2]);
                }
                pixel.depthLocation = byteValue(5, old ? 0x6c : 0x78, 255);
                pixel.dither = byteValue(7, old ? 0x70 : 0x7c, 255);
            }
            auto& indirect = *result.indirect;
            indirect.full = (flags & 0x1000000) != 0;
            for (auto& matrix : indirect.matrices) matrix = formats.copy<28>(0x118);
            for (auto& order : indirect.orders) order = formats.copy<2>(0x108);
            for (auto& scale : indirect.scales) scale = registers.copy<2>(0x14);
            if (!old && input.word(0x18) && std::uint32_t(input.word(0x18) - input.word(0x14)) >= 5) {
                const auto entry = input.table(0x18, index, 0x138, 0x138);
                if (input.byte(entry) == 1) {
                    if (indirect.full) {
                        indirect.count = input.byte(entry + 1);
                        for (unsigned i = 0; i < 3; ++i) indirect.matrices[i] = input.copy<28>(entry + 0x14 + i * 28);
                        for (unsigned i = 0; i < 4; ++i) {
                            indirect.orders[i] = input.copy<2>(entry + 4 + i * 4);
                            indirect.scales[i] = input.copy<2>(entry + 0x68 + i * 4);
                        }
                    }
                    for (unsigned i = 0; i < stageCount; ++i) tev.stages[i].indirect = input.copy<9>(entry + 0x78 + i * 12);
                }
            }
            return result;
        }

    private:
        const Input& input;
        Input formats, registers, colors;
        bool old;
    };
}

bool j3dmodel::MaterialDefaults::read(const std::vector<std::uint8_t>& executable, std::string& error) {
    MaterialDefaults result;
    if (!dolresources::read(executable, glme01::j3dDefaultFormats, 0x190, result.formats, error) ||
        !dolresources::read(executable, glme01::j3dDefaultRegisters, 0x38, result.registers, error) ||
        !dolresources::read(executable, glme01::j3dDefaultColors, 0x20, result.colors, error)) {
        return false;
    }
    *this = std::move(result);
    return true;
}

bool j3dmodel::readMaterials(const std::vector<std::uint8_t>& bytes, unsigned blockOffset, std::uint32_t flags,
    const MaterialDefaults& defaults, Materials& materials, std::string& error) {
    error.clear();
    try {
        const Input resource(bytes, 0, bytes.size());
        const Input input(bytes, blockOffset, resource.word(std::size_t(blockOffset) + 4));
        const Factory factory(input, defaults);
        Materials result;
        const auto count = factory.count();
        while (result.uniqueCount < count && result.uniqueCount <= factory.remap(result.uniqueCount)) {
            ++result.uniqueCount;
        }
        if (flags & 0x200000) {
            result.originals.reserve(result.uniqueCount);
            for (unsigned i = 0; i < result.uniqueCount; ++i) {
                result.originals.push_back(factory.create(i, flags));
                auto& material = result.originals.back();
                material.identity = reinterpret_cast<std::uintptr_t>(&material) >> 4;
            }
        }
        result.entries.reserve(count);
        for (unsigned i = 0; i < count; ++i) {
            result.entries.push_back(std::make_unique<Material>(factory.create(i, flags)));
        }
        for (unsigned i = 0; i < count; ++i) {
            auto& material = *result.entries[i];
            if (flags & 0x200000) {
                material.original = &result.originals.at(factory.remap(i));
                material.identity = reinterpret_cast<std::uintptr_t>(material.original) >> 4;
            } else {
                material.identity = 0xc0000000;
                material.identityFlags = 0xc0000000;
            }
        }
        materials = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
