#include "jlib/j3d/j3dmodelgeometry.h"

#include <cstring>
#include <stdexcept>

namespace {
    class Input {
    public:
        Input(const std::vector<std::uint8_t>& bytes, unsigned start, unsigned size) : bytes(bytes), start(start), size(size) {
            if (start > bytes.size() || size > bytes.size() - start) {
                throw std::runtime_error("J3D block exceeds its resource.");
            }
        }

        unsigned byte(unsigned offset) const {
            check(offset, 1);
            return bytes[start + offset];
        }

        unsigned half(unsigned offset) const {
            check(offset, 2);
            return (unsigned(bytes[start + offset]) << 8) | bytes[start + offset + 1];
        }

        unsigned word(unsigned offset) const {
            check(offset, 4);
            return (half(offset) << 16) | half(offset + 2);
        }

        float scalar(unsigned offset) const {
            const auto value = word(offset);
            float result;
            std::memcpy(&result, &value, sizeof(result));
            return result;
        }

        unsigned pointer(unsigned field, unsigned length = 1) const {
            const auto offset = word(field);
            if (!offset) {
                return 0;
            }
            check(offset, length);
            return start + offset;
        }

        unsigned table(unsigned base, unsigned index, unsigned stride, unsigned length) const {
            check(base, 0);
            if (stride && index > (size - base) / stride) {
                throw std::runtime_error("J3D table index exceeds its block.");
            }
            const auto offset = base + index * stride;
            check(offset, length);
            return offset;
        }

        void check(unsigned offset, unsigned length) const {
            if (offset > size || length > size - offset) {
                throw std::runtime_error("J3D table exceeds its block.");
            }
        }

    private:
        const std::vector<std::uint8_t>& bytes;
        unsigned start, size;
    };

    std::array<float, 3> vector(const Input& input, unsigned offset) {
        return {input.scalar(offset), input.scalar(offset + 4), input.scalar(offset + 8)};
    }

    std::unique_ptr<j3dmodel::NameTable> names(const Input& input, unsigned field, unsigned end,
        const std::vector<std::uint8_t>& bytes) {
        const auto offset = input.pointer(field, 4);
        if (!offset) {
            return nullptr;
        }
        auto result = std::make_unique<j3dmodel::NameTable>();
        std::string error;
        if (!result->read(bytes, offset, end, error)) {
            throw std::runtime_error(error);
        }
        return result;
    }

    void joints(const Input& input, j3dmodel::Geometry& geometry) {
        const auto count = input.half(8), data = input.word(12), remap = input.word(16);
        input.check(remap, count * 2);
        geometry.joints.resize(count);
        for (unsigned i = 0; i < count; ++i) {
            const auto offset = input.table(data, input.half(remap + i * 2), 64, 64);
            auto& joint = geometry.joints[i];
            joint.index = i;
            joint.kind = static_cast<std::uint8_t>(input.half(offset));
            joint.scaleCompensation = static_cast<std::uint8_t>(input.byte(offset + 2));
            if (joint.scaleCompensation == 255) {
                joint.scaleCompensation = 0;
            }
            joint.scale = vector(input, offset + 4);
            for (unsigned axis = 0; axis < 3; ++axis) {
                const auto value = input.half(offset + 16 + axis * 2);
                joint.rotation[axis] = static_cast<std::int16_t>(value < 32768 ? int(value) : int(value) - 65536);
            }
            joint.translation = vector(input, offset + 24);
            joint.radius = input.scalar(offset + 36);
            joint.minimum = vector(input, offset + 40);
            joint.maximum = vector(input, offset + 52);
        }
    }

    void envelopes(const Input& input, j3dmodel::Geometry& geometry) {
        const auto count = input.half(8), counts = input.word(12), indices = input.word(16), weights = input.word(20);
        input.check(counts, count);
        geometry.envelopes.resize(count);
        unsigned cursor = 0;
        for (unsigned index = 0; index < count; ++index) {
            auto& envelope = geometry.envelopes[index];
            const auto entries = input.byte(counts + index);
            for (unsigned i = 0; i < entries; ++i, ++cursor) {
                envelope.joints.push_back(static_cast<std::uint16_t>(input.half(input.table(indices, cursor, 2, 2))));
                envelope.weights.push_back(input.scalar(input.table(weights, cursor, 4, 4)));
            }
        }
        geometry.inverseMatrixOffset = input.pointer(24);
    }

    void shapes(const Input& input, unsigned blockOffset, unsigned hierarchyOffset, j3dmodel::Geometry& geometry) {
        const auto count = input.half(8), data = input.word(12), remap = input.word(16);
        const auto descriptors = input.word(24), matrixTable = input.word(28), draws = input.word(32);
        const auto matrices = input.word(36), drawTable = input.word(40);
        input.check(remap, count * 2);
        geometry.shapes.clear();
        geometry.shapes.resize(count);
        const Input resource(*geometry.bytes, 0, static_cast<unsigned>(geometry.bytes->size()));
        for (unsigned cursor = hierarchyOffset; resource.half(cursor); cursor += 4) {
            if (resource.half(cursor) != 0x12) {
                continue;
            }
            const auto index = resource.half(cursor + 2);
            if (index >= count) {
                throw std::runtime_error("J3D hierarchy exceeds its shape table.");
            }
            const auto offset = input.table(data, input.half(remap + index * 2), 40, 40);
            auto shape = std::make_unique<j3dmodel::Shape>();
            shape->descriptorOffset = blockOffset + input.table(descriptors, input.half(offset + 4), 1, 8);
            shape->radius = input.scalar(offset + 12);
            shape->minimum = vector(input, offset + 16);
            shape->maximum = vector(input, offset + 28);
            const auto groups = input.half(offset + 2), firstMatrix = input.half(offset + 6), firstDraw = input.half(offset + 8);
            shape->groups.resize(groups);
            for (unsigned i = 0; i < groups; ++i) {
                auto& group = shape->groups[i];
                group.matrixKind = input.byte(offset);
                if (group.matrixKind < 4) {
                    const auto matrix = input.table(matrices, firstMatrix + i, 8, 8);
                    group.matrix = static_cast<std::uint16_t>(input.half(matrix));
                    if (group.matrixKind == 3) {
                        const auto matrixCount = input.half(matrix + 2), first = input.word(matrix + 4);
                        const auto table = input.table(matrixTable, first, 2, matrixCount * 2);
                        for (unsigned j = 0; j < matrixCount; ++j) {
                            group.matrixTable.push_back(static_cast<std::uint16_t>(input.half(table + j * 2)));
                        }
                    }
                }
                const auto draw = input.table(drawTable, firstDraw + i, 8, 8);
                group.drawSize = input.word(draw);
                const auto drawOffset = input.table(draws, input.word(draw + 4), 1, group.drawSize);
                group.drawOffset = blockOffset + drawOffset;
            }
            geometry.shapes[index] = std::move(shape);
        }
    }
}

bool j3dmodel::readGeometry(const std::vector<std::uint8_t>& bytes, std::uint32_t flags, Geometry& geometry, std::string& error) {
    error.clear();
    try {
        if (bytes.size() > 0xffffffff) {
            throw std::runtime_error("J3D resource exceeds the original address range.");
        }
        const Input resource(bytes, 0, static_cast<unsigned>(bytes.size()));
        if (resource.word(0) != 0x4a334432 || (resource.word(4) != 0x626d6432 && resource.word(4) != 0x626d6433)) {
            throw std::runtime_error("Unsupported original J3D model version.");
        }
        Geometry result;
        result.bytes = std::make_shared<const std::vector<std::uint8_t>>(bytes);
        unsigned offset = 32, hierarchyOffset = 0, materialCount = 0;
        for (unsigned i = 0; i < resource.word(12); ++i) {
            const auto type = resource.word(offset), size = resource.word(offset + 4);
            if (size < 8) {
                throw std::runtime_error("Invalid J3D block size.");
            }
            const Input input(bytes, offset, size);
            if (type == 0x494e4631) {
                result.flags = flags | input.half(8);
                result.packetCount = input.word(12);
                result.vertexCount = input.word(16);
                hierarchyOffset = input.pointer(20, 4);
            } else if (type == 0x56545831) {
                result.vertexFormatOffset = input.pointer(8);
                for (unsigned field = 0; field < result.vertexOffsets.size(); ++field) {
                    result.vertexOffsets[field] = input.pointer(12 + field * 4);
                }
                const auto normal = result.vertexOffsets[1];
                auto end = result.vertexOffsets[2];
                if (!end) {
                    end = result.vertexOffsets[3];
                }
                if (!end) {
                    end = result.vertexOffsets[5];
                }
                if (!end) {
                    end = offset + size;
                }
                if (normal && end < normal) {
                    throw std::runtime_error("Invalid original J3D normal array range.");
                }
                result.normalCount = normal ? (end - normal) / 12 + 1 : 0;
            } else if (type == 0x45565031) {
                envelopes(input, result);
            } else if (type == 0x44525731) {
                const auto count = input.half(8), modes = input.word(12), indices = input.word(16);
                result.drawMatrices.resize(count);
                for (unsigned matrix = 0; matrix < count; ++matrix) {
                    result.drawMatrices[matrix] = {static_cast<std::uint8_t>(input.byte(input.table(modes, matrix, 1, 1))),
                        static_cast<std::uint16_t>(input.half(input.table(indices, matrix, 2, 2)))};
                }
            } else if (type == 0x4a4e5431) {
                result.jointNames = names(input, 20, offset + size, bytes);
                joints(input, result);
            } else if ((type == 0x4d415432 && resource.word(4) == 0x626d6432) ||
                (type == 0x4d415433 && resource.word(4) == 0x626d6433)) {
                result.materialOffset = offset;
                materialCount = input.half(8);
                result.materialNames = names(input, 20, offset + size, bytes);
            } else if (type == 0x53485031) {
                if (!hierarchyOffset) {
                    throw std::runtime_error("J3D shape construction requires the preceding hierarchy.");
                }
                result.shapeNames = names(input, 20, offset + size, bytes);
                shapes(input, offset, hierarchyOffset, result);
            } else if (type == 0x54455831) {
                result.textureOffset = offset;
                result.textureNames = names(input, 16, offset + size, bytes);
                const auto count = input.half(8), headers = input.word(12);
                result.textureHeaders.resize(count);
                for (unsigned texture = 0; texture < count; ++texture) {
                    result.textureHeaders[texture] = offset + input.table(headers, texture, 32, 32);
                }
            }
            offset += size;
        }
        if (!hierarchyOffset || !readHierarchy(bytes, hierarchyOffset, static_cast<unsigned>(result.joints.size()),
            materialCount, static_cast<unsigned>(result.shapes.size()), result.hierarchy, error)) {
            if (error.empty()) error = "J3D model has no hierarchy.";
            return false;
        }
        geometry = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
