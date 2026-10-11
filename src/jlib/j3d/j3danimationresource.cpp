#include "jlib/j3d/j3danimationresource.h"
#include "jlib/j3d/j3dmodelinstance.h"

#include <algorithm>
#include <stdexcept>

namespace {
    class Input {
    public:
        Input(const std::vector<std::uint8_t>& bytes, unsigned start, unsigned size) : bytes(bytes), start(start), size(size) {
            if (start > bytes.size() || size > bytes.size() - start) {
                throw std::runtime_error("J3D animation block exceeds its resource.");
            }
        }
        std::uint8_t byte(unsigned offset) const {
            check(offset, 1);
            return bytes[start + offset];
        }
        unsigned half(unsigned offset) const {
            check(offset, 2);
            return (unsigned(byte(offset)) << 8) | byte(offset + 1);
        }
        unsigned word(unsigned offset) const {
            check(offset, 4);
            return (half(offset) << 16) | half(offset + 2);
        }
        unsigned pointer(unsigned field, unsigned length = 1) const {
            const auto offset = word(field);
            if (!offset) {
                if (length) throw std::runtime_error("J3D animation is missing a required array.");
                return 0;
            }
            check(offset, length);
            return start + offset;
        }
        void check(unsigned offset, unsigned length) const {
            if (offset > size || length > size - offset) {
                throw std::runtime_error("J3D animation table exceeds its block.");
            }
        }
        void names(unsigned field, j3dmodel::AnimationGroup& group, bool optional = false) const {
            if (optional && !word(field)) {
                return;
            }
            auto result = std::make_unique<j3dmodel::NameTable>();
            std::string error;
            if (!result->read(bytes, pointer(field, 4), start + size, error)) {
                throw std::runtime_error(error);
            }
            group.names = std::move(result);
        }
        void materials(unsigned field, j3dmodel::AnimationGroup& group, unsigned count) const {
            const auto offset = pointer(field, count * 2);
            group.materials.resize(count);
            for (unsigned i = 0; i < count; ++i) {
                group.materials[i] = static_cast<std::uint16_t>((unsigned(bytes[offset + i * 2]) << 8) | bytes[offset + i * 2 + 1]);
            }
        }
        void textureGroup(j3dmodel::AnimationGroup& group, unsigned second) const {
            group.count = half(second ? 0x34 : 12);
            const unsigned rows = group.count / 3;
            const unsigned fields = second ? 0x3c : 0x14;
            group.table = pointer(fields, group.count * 18);
            materials(fields + 4, group, rows);
            names(fields + 8, group, second != 0);
            const auto slots = pointer(fields + 12, rows);
            const auto centers = pointer(fields + 16, rows * 12);
            group.slots.resize(rows);
            group.centers.resize(rows);
            for (unsigned i = 0; i < rows; ++i) {
                group.slots[i] = bytes[slots + i];
                std::copy_n(bytes.begin() + centers + i * 12, 12, group.centers[i].begin());
            }
            for (unsigned i = 0; i < 3; ++i) {
                group.valueCounts[i] = half((second ? 0x36 : 14) + i * 2);
                group.values[i] = pointer(fields + 20 + i * 4, group.valueCounts[i] * (i == 1 ? 2 : 4));
            }
        }

    private:
        const std::vector<std::uint8_t>& bytes;
        unsigned start, size;
    };

    std::unique_ptr<j3dmodel::AnimationBinding> binding(const std::shared_ptr<j3dmodel::Animation>& animation, unsigned index) {
        auto result = std::make_unique<j3dmodel::AnimationBinding>();
        result->animation = animation;
        result->index = static_cast<std::uint16_t>(index);
        return result;
    }
}

bool j3dmodel::Animation::read(const std::vector<std::uint8_t>& source, std::string& error) {
    error.clear();
    try {
        if (source.size() > 0xffffffff) {
            throw std::runtime_error("J3D animation exceeds the original address range.");
        }
        const Input resource(source, 0, static_cast<unsigned>(source.size()));
        if (resource.word(0) != 0x4a334431) {
            throw std::runtime_error("Unsupported original J3D animation signature.");
        }
        Animation result;
        switch (resource.word(4)) {
        case 0x62636b31: result.kind = AnimationKind::Transform; break;
        case 0x62706b31: result.kind = AnimationKind::Color; break;
        case 0x62746b31: result.kind = AnimationKind::Texture; break;
        case 0x62726b31: result.kind = AnimationKind::Tev; break;
        default: throw std::runtime_error("This J3D animation loader requires BCK, BPK, BTK or BRK data.");
        }
        result.bytes = std::make_shared<const std::vector<std::uint8_t>>(source);
        unsigned offset = 32;
        for (unsigned block = 0; block < resource.word(12); ++block) {
            const auto type = resource.word(offset), size = resource.word(offset + 4);
            if (size < 8) throw std::runtime_error("Invalid J3D animation block size.");
            const auto available = static_cast<unsigned>(source.size()) - offset;
            if (size > available && block + 1 != resource.word(12)) {
                throw std::runtime_error("J3D animation block chain exceeds its resource.");
            }
            const Input input(source, offset, std::min(size, available));
            const bool transform = type == 0x414e4b31 && result.kind == AnimationKind::Transform;
            const bool texture = type == 0x54544b31 && result.kind == AnimationKind::Texture;
            const bool color = type == 0x50414b31 && result.kind == AnimationKind::Color;
            const bool tev = type == 0x54524b31 && result.kind == AnimationKind::Tev;
            if (transform || texture || color || tev) {
                result.attribute = input.byte(8);
                const auto frames = input.half(color ? 12 : 10);
                result.frameCount = static_cast<std::int16_t>(frames < 32768 ? int(frames) : int(frames) - 65536);
                result.frame = 0;
            }
            auto& group = result.groups[0];
            if (transform) {
                result.angleScale = input.byte(9);
                group.count = input.half(12);
                group.table = input.pointer(20, group.count * 54);
                for (unsigned i = 0; i < 3; ++i) {
                    group.valueCounts[i] = input.half(14 + i * 2);
                    group.values[i] = input.pointer(24 + i * 4, group.valueCounts[i] * (i == 1 ? 2 : 4));
                }
            } else if (texture) {
                result.angleScale = input.byte(9);
                input.textureGroup(result.groups[0], 0);
                input.textureGroup(result.groups[1], 1);
            } else if (color) {
                group.count = input.half(14);
                group.table = input.pointer(24, group.count * 24);
                input.materials(28, group, group.count);
                input.names(32, group);
                for (unsigned i = 0; i < 4; ++i) {
                    group.valueCounts[i] = input.half(16 + i * 2);
                    group.values[i] = input.pointer(36 + i * 4, group.valueCounts[i] * 2);
                }
            } else if (tev) {
                for (unsigned section = 0; section < 2; ++section) {
                    auto& target = result.groups[section];
                    target.count = input.half(12 + section * 2);
                    target.table = input.pointer(32 + section * 4, target.count * 28);
                    input.materials(40 + section * 4, target, target.count);
                    input.names(48 + section * 4, target);
                    target.slots.resize(target.count);
                    for (unsigned i = 0; i < target.count; ++i) {
                        target.slots[i] = resource.byte(target.table + i * 28 + 24);
                    }
                    for (unsigned i = 0; i < 4; ++i) {
                        target.valueCounts[i] = input.half(16 + section * 8 + i * 2);
                        target.values[i] = input.pointer(56 + section * 16 + i * 4, target.valueCounts[i] * 2);
                    }
                }
            }
            if (block + 1 < resource.word(12)) {
                offset += size;
            }
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

void j3dmodel::Animation::resolveMaterials(const NameTable& names) {
    for (auto& group : groups) {
        for (unsigned i = 0; i < group.materials.size(); ++i) {
            const auto* name = group.names ? group.names->name(static_cast<std::uint16_t>(i)) : nullptr;
            if (!name) {
                throw std::runtime_error("J3D animation lacks its declared material name.");
            }
            group.materials[i] = static_cast<std::uint16_t>(names.index(*name));
        }
    }
}

void j3dmodel::prepareMaterialAnimations(ModelData& model) {
    for (auto& entry : model.materials.entries) {
        auto animation = std::make_unique<MaterialAnimation>();
        if (entry->original && !(entry->identityFlags & 0xc0000000)) {
            entry->identityFlags |= 0x80000000;
            entry->mode = entry->original->mode;
            entry->fieldE = entry->original->fieldE;
            entry->animation.reset();
        }
        entry->animation = std::move(animation);
        entry->flags |= 1;
    }
}

unsigned j3dmodel::bindAnimation(ModelData& model, const std::shared_ptr<Animation>& animation) {
    if (!animation) throw std::runtime_error("Cannot bind an absent J3D animation.");
    if (animation->kind == AnimationKind::Transform) {
        const unsigned kind = model.geometry.flags & 15;
        if (model.jointCalculators.empty()) throw std::runtime_error("J3D transform animation requires the first joint.");
        if (kind < 3) {
            auto calculator = std::make_unique<MatrixCalculator>(kind);
            calculator->animation = animation;
            model.jointCalculators[0] = std::move(calculator);
        } else {
            model.jointCalculators[0].reset();
        }
        animation->frame = 0;
        return 0;
    }
    if (!model.geometry.materialNames) throw std::runtime_error("J3D animation requires model material names.");
    animation->resolveMaterials(*model.geometry.materialNames);
    animation->frame = 0;
    unsigned result = 0;
    for (unsigned section = 0; section < 2; ++section) {
        const auto& group = animation->groups[section];
        for (unsigned i = 0; i < group.materials.size(); ++i) {
            const auto material = group.materials[i];
            if (material == 65535 && animation->kind != AnimationKind::Texture) {
                continue;
            }
            auto& target = *model.materials.entries.at(material);
            if (!target.animation) {
                result = 1;
                continue;
            }
            if (animation->kind == AnimationKind::Color) {
                target.animation->colors[0] = binding(animation, i);
            } else if (animation->kind == AnimationKind::Texture) {
                const auto slot = group.slots.at(i);
                if (slot == 255) continue;
                auto* matrix = section ? target.texture->postMatrices.at(slot).get() : target.texture->matrices.at(slot).get();
                if (!matrix) throw std::runtime_error("J3D texture animation requires an existing material matrix.");
                std::copy(group.centers.at(i).begin(), group.centers.at(i).end(), matrix->info.begin() + 4);
                if (section) target.animation->postMatrices.at(slot) = binding(animation, i);
                else target.animation->matrices.at(slot) = binding(animation, i);
            } else if (animation->kind == AnimationKind::Tev) {
                const auto slot = group.slots.at(i);
                if (section) target.animation->tevConstants.at(slot) = binding(animation, i);
                else target.animation->tevColors.at(slot) = binding(animation, i);
            }
        }
    }
    return result;
}
