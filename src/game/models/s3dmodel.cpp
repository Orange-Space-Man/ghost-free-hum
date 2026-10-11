#include "game/models/s3dmodel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
    using s3dmodel::Matrix;

    void require(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    void range(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t size) {
        require(offset <= data.size() && size <= data.size() - offset, "Truncated S3D model resource.");
    }

    unsigned word(const std::vector<std::uint8_t>& data, std::size_t offset) {
        range(data, offset, 2);
        return (data[offset] << 8) | data[offset + 1];
    }

    unsigned dword(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (word(data, offset) << 16) | word(data, offset + 2);
    }

    float number(const std::vector<std::uint8_t>& data, std::size_t offset) {
        const auto bits = dword(data, offset);
        float value;
        std::memcpy(&value, &bits, 4);
        require(std::isfinite(value), "Non-finite S3D model value.");
        return value;
    }

    Matrix identity() { return {1,0,0,0, 0,1,0,0, 0,0,1,0}; }

    Matrix multiply(const Matrix& a, const Matrix& b) {
        Matrix result{};
        for (unsigned row = 0; row < 3; ++row) {
            for (unsigned column = 0; column < 4; ++column) {
                result[row * 4 + column] = a[row * 4] * b[column] + a[row * 4 + 1] * b[4 + column] + a[row * 4 + 2] * b[8 + column];
            }
            result[row * 4 + 3] += a[row * 4 + 3];
        }
        return result;
    }

    Matrix localMatrix(const s3danimation::Pose& pose, const std::vector<float>& table) {
        const unsigned x = (static_cast<unsigned>(pose.rotation[0]) >> 4) & 4095;
        const unsigned y = (static_cast<unsigned>(pose.rotation[1]) >> 4) & 4095;
        const unsigned z = (static_cast<unsigned>(pose.rotation[2]) >> 4) & 4095;
        const float sx = table[x], sy = table[y], sz = table[z];
        const float cx = table[x + 1024], cy = table[y + 1024], cz = table[z + 1024];
        return {cy*cz, sy*sx*cz-cx*sz, sx*sz+sy*cx*cz, pose.translation[0],
            cy*sz, cx*cz+sy*sx*sz, sy*cx*sz-sx*cz, pose.translation[1],
            -sy, cy*sx, cy*cx, pose.translation[2]};
    }
}

bool s3dmodel::Model::read(const std::vector<std::uint8_t>& data, std::string& error, bool loadShadows) {
    error.clear();
    try {
        range(data, 0, 128);
        require(dword(data, 0) == 0x04b40000, "Unsupported S3D model header.");
        Model result;
        for (unsigned index = 0; index < word(data, 16); ++index) {
            std::array<float, 3> position;
            for (unsigned axis = 0; axis < 3; ++axis) position[axis] = number(data, dword(data, 72) + index * 12 + axis * 4);
            result.positions.push_back(position);
        }
        const unsigned jointCount = word(data, 8), envelopeCount = word(data, 12);
        const unsigned packetCount = word(data, 10), shapeCount = word(data, 38), materialCount = word(data, 40);
        const auto hierarchy = dword(data, 48), packets = dword(data, 52), shapeTable = dword(data, 112), pairs = dword(data, 116);
        range(data, hierarchy, jointCount * 16);
        range(data, packets, packetCount * 32);
        range(data, shapeTable, shapeCount * 8);
        result.joints.resize(jointCount);
        require(jointCount > 0, "Missing S3D root joint.");
        for (unsigned index = 0; index < jointCount; ++index) {
            const auto record = hierarchy + index * 16;
            require(word(data, record) == index && word(data, record + 6) == 0, "Unsupported S3D joint mode.");
            auto& joint = result.joints[index];
            const int child = static_cast<std::int16_t>(word(data, record + 2));
            const int sibling = static_cast<std::int16_t>(word(data, record + 4));
            joint.child = child ? static_cast<int>(index) + child : -1;
            joint.sibling = sibling ? static_cast<int>(index) + sibling : -1;
            require((!child || joint.child >= 0) && (!sibling || joint.sibling >= 0) && joint.child < static_cast<int>(jointCount) &&
                joint.sibling < static_cast<int>(jointCount), "Invalid S3D joint link.");
        }
        std::vector<bool> visited(jointCount, false);
        std::vector<unsigned> order;
        std::vector<int> pending{0};
        while (!pending.empty()) {
            const auto index = pending.back();
            pending.pop_back();
            require(!visited[index], "Cyclic or repeated S3D hierarchy link.");
            visited[index] = true;
            order.push_back(static_cast<unsigned>(index));
            if (result.joints[index].child >= 0) pending.push_back(result.joints[index].child);
            if (result.joints[index].sibling >= 0) pending.push_back(result.joints[index].sibling);
        }
        require(order.size() == jointCount, "Disconnected S3D hierarchy.");
        result.inverseBind.resize(jointCount);
        if (envelopeCount) {
            const auto matrices = dword(data, 56);
            range(data, matrices, jointCount * 48);
            for (unsigned joint = 0; joint < jointCount; ++joint) {
                for (unsigned component = 0; component < 12; ++component) result.inverseBind[joint][component] = number(data, matrices + joint * 48 + component * 4);
            }
            const auto counts = dword(data, 68), indices = dword(data, 64), weights = dword(data, 60);
            range(data, counts, envelopeCount);
            std::size_t cursor = 0;
            for (unsigned envelope = 0; envelope < envelopeCount; ++envelope) {
                std::vector<Weight> entries;
                for (unsigned index = 0; index < data[counts + envelope]; ++index, ++cursor) {
                    const auto joint = word(data, indices + cursor * 2);
                    require(joint < jointCount, "Invalid S3D envelope joint.");
                    entries.push_back({joint, number(data, weights + cursor * 4)});
                }
                result.envelopes.push_back(std::move(entries));
            }
        }
        result.textures.push_back({1,1,{255,255,255,255}});
        const auto textureTable = dword(data, 96);
        constexpr unsigned formats[]{8,9,10,0,1,2,3,4,5,6,14};
        for (unsigned index = 0; index < word(data, 32); ++index) {
            const auto start = dword(data, textureTable + index * 4);
            range(data, start, 32);
            require(data[start] < 11, "Invalid S3D texture format.");
            roommaterials::Texture texture;
            const std::vector<std::uint8_t> pixels(data.begin() + start + 32, data.end());
            if (!roommaterials::decodeTexture(pixels, word(data, start + 2), word(data, start + 4), formats[data[start]], texture, error)) throw std::runtime_error(error);
            result.textures.push_back(std::move(texture));
        }
        const auto materialTable = dword(data, 104), samplerTable = dword(data, 108);
        range(data, materialTable, materialCount * 288);
        for (unsigned index = 0; index < word(data, 36); ++index) {
            const auto start = samplerTable + index * 8;
            range(data, start, 8);
            Sampler sampler{word(data, start) + 1, data[start + 4], data[start + 5], data[start + 6]};
            require(sampler.texture < result.textures.size() && sampler.wrapS <= 2 && sampler.wrapT <= 2, "Invalid S3D sampler.");
            result.samplers.push_back(sampler);
        }
        for (unsigned index = 0; index < materialCount; ++index) {
            const auto start = materialTable + index * 288;
            Material material;
            for (unsigned component = 0; component < 4; ++component) material.color[component] = data[start + component] / 255.0f;
            material.mode = data[start + 6];
            material.sourceMode = material.mode;
            require(data[start + 4] <= 1 && data[start + 5] <= 1, "Unsupported S3D color source.");
            material.vertexColor = data[start + 4] != 0;
            material.vertexAlpha = data[start + 5] != 0;
            require(data[start + 7] <= 1 && !(data[start + 8] & 2), "Unsupported S3D material stages.");
            if (data[start + 7]) {
                const auto sampler = word(data, start + 34);
                require(sampler < word(data, 36), "Invalid S3D sampler index.");
                const auto texture = word(data, samplerTable + sampler * 8);
                range(data, samplerTable + sampler * 8, 8);
                require(texture + 1 < result.textures.size(), "Invalid S3D texture index.");
                material.texture = texture + 1;
                material.wrapS = data[samplerTable + sampler * 8 + 4];
                material.wrapT = data[samplerTable + sampler * 8 + 5];
                const auto mode = data[samplerTable + sampler * 8 + 6];
                if (!(mode & 4)) material.mode = std::max(material.mode, unsigned(mode & 3));
                require(material.wrapS <= 2 && material.wrapT <= 2, "Invalid S3D texture wrap mode.");
            }
            require(material.mode <= 2, "Unsupported S3D material mode.");
            result.materials.push_back(material);
        }
        std::array<unsigned, 10> palette{};
        for (unsigned shadow = 0; shadow <= unsigned(loadShadows); ++shadow) {
            palette.fill(0xffffffff);
            for (const auto joint : order) {
                const auto record = hierarchy + joint * 16;
                for (unsigned pair = 0; pair < word(data, record + 8); ++pair) {
                    const auto address = pairs + (word(data, record + 10) + pair) * 4;
                    const auto material = word(data, address), shape = word(data, address + 2);
                    require(material < materialCount && shape < shapeCount, "Invalid S3D joint shape.");
                    const auto shapeRecord = shapeTable + shape * 8;
                    require(!(dword(data, shapeRecord) & 0x02000000), "S3D NBT display lists are not supported.");
                    const auto first = word(data, shapeRecord + 6), count = word(data, shapeRecord + 4);
                    require(first <= packetCount && count <= packetCount - first, "Invalid S3D shape packet range.");
                    for (unsigned packet = first; packet < first + count; ++packet) {
                        const auto header = packets + (packet + shadow * packetCount) * 32;
                        const auto matrices = word(data, header + 10);
                        require(matrices <= palette.size(), "Invalid S3D packet palette.");
                        for (unsigned slot = 0; slot < matrices; ++slot) {
                            const auto matrix = word(data, header + 12 + slot * 2);
                            if (matrix != 0xffff) {
                                require(matrix < jointCount + envelopeCount, "Invalid S3D packet matrix.");
                                palette[slot] = matrix;
                            }
                        }
                        std::size_t cursor = dword(data, header);
                        const auto length = dword(data, header + 4);
                        range(data, cursor, length);
                        const auto end = cursor + length;
                        Batch batch;
                        batch.material = material;
                        batch.cull = word(data, header + 8);
                        require(batch.cull <= 3, "Invalid S3D culling mode.");
                        batch.lighting = !shadow && (dword(data, shapeRecord) & 0x01000000) != 0;
                        auto index = [&](unsigned section, unsigned countOffset, unsigned stride) {
                            require(cursor <= end && end - cursor >= 2, "Truncated S3D vertex index.");
                            const auto value = word(data, cursor);
                            cursor += 2;
                            require(value < word(data, countOffset), "Invalid S3D vertex array index.");
                            return static_cast<std::size_t>(dword(data, section)) + value * stride;
                        };
                        while (cursor < end) {
                            const auto command = data[cursor++];
                            if (command == 0) continue;
                            require(command == 0x90 || command == 0x98 || command == 0xa0 || command == 0x80, "Unsupported S3D primitive.");
                            require(end - cursor >= 2, "Truncated S3D primitive count.");
                            const auto vertices = word(data, cursor);
                            cursor += 2;
                            std::vector<Vertex> primitive;
                            for (unsigned vertexIndex = 0; vertexIndex < vertices; ++vertexIndex) {
                                require(end - cursor >= 3, "Truncated S3D matrix index.");
                                const auto matrix = data[cursor];
                                cursor += shadow ? 1 : 3;
                                require(matrix % 3 == 0 && matrix / 3 < palette.size() && palette[matrix / 3] != 0xffffffff, "Uninitialized S3D matrix slot.");
                                Vertex vertex;
                                vertex.matrix = palette[matrix / 3];
                                auto position = index(72, 16, 12);
                                vertex.positionIndex = static_cast<unsigned>((position - dword(data, 72)) / 12);
                                for (unsigned axis = 0; axis < 3; ++axis) vertex.position[axis] = number(data, position + axis * 4);
                                if (!shadow && word(data, 18)) {
                                    position = index(76, 18, 12);
                                    for (unsigned axis = 0; axis < 3; ++axis) vertex.normal[axis] = number(data, position + axis * 4);
                                }
                                if (shadow) {
                                    require(cursor < end, "Truncated S3D shadow color index.");
                                    ++cursor;
                                } else if (word(data, 20)) {
                                    position = index(80, 20, 4);
                                    range(data, position, 4);
                                    for (unsigned axis = 0; axis < 4; ++axis) vertex.color[axis] = data[position + axis] / 255.0f;
                                }
                                if (!shadow && word(data, 22)) {
                                    position = index(84, 22, 8);
                                    for (unsigned axis = 0; axis < 2; ++axis) vertex.coordinate[axis] = number(data, position + axis * 4);
                                }
                                primitive.push_back(vertex);
                            }
                            auto triangle = [&](unsigned a, unsigned b, unsigned c) {
                                batch.vertices.push_back(primitive[a]);
                                batch.vertices.push_back(primitive[b]);
                                batch.vertices.push_back(primitive[c]);
                            };
                            if (command == 0x90) {
                                require(vertices % 3 == 0, "Invalid S3D triangle count.");
                                for (unsigned i = 0; i < vertices; i += 3) triangle(i, i + 1, i + 2);
                            } else if (command == 0x80) {
                                require(vertices % 4 == 0, "Invalid S3D quad count.");
                                for (unsigned i = 0; i < vertices; i += 4) { triangle(i, i+1, i+2); triangle(i, i+2, i+3); }
                            } else {
                                for (unsigned i = 2; i < vertices; ++i) {
                                    if (command == 0xa0) triangle(0, i-1, i);
                                    else if (i & 1) triangle(i-1, i-2, i);
                                    else triangle(i-2, i-1, i);
                                }
                            }
                        }
                        (shadow ? result.shadowBatches : result.batches).push_back(std::move(batch));
                    }
                }
            }
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

unsigned s3dmodel::renderMode(const Material& material) {
    return material.texture != 0 && material.color[3] < 1 ? 2 : material.mode;
}

std::vector<s3dmodel::Batch> s3dmodel::Model::animate(const std::vector<s3danimation::Pose>& pose, unsigned flags,
    const std::vector<float>& trigonometry, bool shadow) const {
    require(pose.size() == joints.size() && trigonometry.size() >= 5120, "Missing S3D pose or trigonometry table.");
    std::vector<Matrix> matrices(joints.size() + envelopes.size());
    struct Pending { int index; Matrix parent; std::array<float, 3> scale; };
    std::vector<Pending> pending{{0, identity(), {1,1,1}}};
    require(!joints.empty(), "Missing S3D root joint.");
    while (!pending.empty()) {
        const auto entry = pending.back();
        pending.pop_back();
        const auto index = entry.index;
        const auto& parent = entry.parent;
        const auto& scale = entry.scale;
        auto local = localMatrix(pose[index], trigonometry);
        auto accumulated = scale;
        for (unsigned axis = 0; axis < 3; ++axis) {
            if (!(flags & 2)) local[axis * 4 + 3] *= scale[axis];
            accumulated[axis] *= pose[index].scale[axis];
            if (flags & 2) for (unsigned row = 0; row < 3; ++row) local[row * 4 + axis] *= pose[index].scale[axis];
        }
        const auto world = multiply(parent, local);
        matrices[index] = world;
        if (!(flags & 2)) for (unsigned axis = 0; axis < 3; ++axis) for (unsigned row = 0; row < 3; ++row) matrices[index][row * 4 + axis] *= accumulated[axis];
        if (joints[index].child >= 0) pending.push_back({joints[index].child, world, accumulated});
        if (joints[index].sibling >= 0) pending.push_back({joints[index].sibling, parent, scale});
    }
    for (std::size_t envelope = 0; envelope < envelopes.size(); ++envelope) {
        auto& matrix = matrices[joints.size() + envelope];
        for (const auto& weight : envelopes[envelope]) {
            const auto combined = multiply(matrices[weight.joint], inverseBind[weight.joint]);
            for (unsigned element = 0; element < 12; ++element) matrix[element] += combined[element] * weight.value;
        }
    }
    auto result = shadow ? shadowBatches : batches;
    for (auto& batch : result) {
        for (auto& vertex : batch.vertices) {
            const auto& matrix = matrices.at(vertex.matrix);
            const auto position = positions.empty() ? vertex.position : positions.at(vertex.positionIndex);
            for (unsigned row = 0; row < 3; ++row) vertex.position[row] = matrix[row * 4] * position[0] + matrix[row * 4 + 1] * position[1] + matrix[row * 4 + 2] * position[2] + matrix[row * 4 + 3];
            const std::array<float, 9> cofactors{
                matrix[5]*matrix[10]-matrix[6]*matrix[9], matrix[6]*matrix[8]-matrix[4]*matrix[10], matrix[4]*matrix[9]-matrix[5]*matrix[8],
                matrix[2]*matrix[9]-matrix[1]*matrix[10], matrix[0]*matrix[10]-matrix[2]*matrix[8], matrix[1]*matrix[8]-matrix[0]*matrix[9],
                matrix[1]*matrix[6]-matrix[2]*matrix[5], matrix[2]*matrix[4]-matrix[0]*matrix[6], matrix[0]*matrix[5]-matrix[1]*matrix[4]};
            const float determinant = matrix[0]*cofactors[0] + matrix[1]*cofactors[1] + matrix[2]*cofactors[2];
            const auto normal = vertex.normal;
            if (batch.lighting && determinant == 0) throw std::runtime_error("Singular S3D lighting matrix.");
            if (determinant != 0) for (unsigned row = 0; row < 3; ++row)
                vertex.normal[row] = (cofactors[row * 3]*normal[0] + cofactors[row * 3 + 1]*normal[1] + cofactors[row * 3 + 2]*normal[2]) / determinant;
        }
    }
    return result;
}
