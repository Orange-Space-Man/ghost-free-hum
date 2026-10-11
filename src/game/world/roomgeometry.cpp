#include "game/world/roomgeometry.h"
#include "game/world/roommodelreader.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace {
    using Matrix = std::array<float, 16>;

    void require(bool condition, const char* error) {
        if (!condition) {
            throw std::runtime_error(error);
        }
    }

    void range(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t size, std::size_t end) {
        require(end <= data.size() && offset <= end && size <= end - offset, "Geometry range lies outside its section.");
    }

    std::uint16_t read16(const std::vector<std::uint8_t>& data, std::size_t offset) {
        range(data, offset, 2, data.size());
        return static_cast<std::uint16_t>((data[offset] << 8) | data[offset + 1]);
    }

    std::uint32_t read32(const std::vector<std::uint8_t>& data, std::size_t offset) {
        return (static_cast<std::uint32_t>(read16(data, offset)) << 16) | read16(data, offset + 2);
    }

    Matrix identity() {
        Matrix matrix{};
        matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1;
        return matrix;
    }

    Matrix multiply(const Matrix& left, const Matrix& right) {
        Matrix result{};
        for (std::size_t column = 0; column < 4; ++column) {
            for (std::size_t row = 0; row < 4; ++row) {
                for (std::size_t index = 0; index < 4; ++index) {
                    result[column * 4 + row] += left[index * 4 + row] * right[column * 4 + index];
                }
            }
        }
        return result;
    }

    Matrix transform(const roommodel::Node& node) {
        Matrix result = identity();
        result[12] = node.translation[0];
        result[13] = node.translation[1];
        result[14] = node.translation[2];
        for (int axis = 2; axis >= 0; --axis) {
            Matrix rotation = identity();
            const float angle = node.rotation[axis] * 0.01745329252f;
            const float cosine = std::cos(angle);
            const float sine = std::sin(angle);
            const int a = (axis + 1) % 3;
            const int b = (axis + 2) % 3;
            rotation[a * 4 + a] = rotation[b * 4 + b] = cosine;
            rotation[a * 4 + b] = sine;
            rotation[b * 4 + a] = -sine;
            result = multiply(result, rotation);
        }
        for (std::size_t axis = 0; axis < 3; ++axis) {
            for (std::size_t row = 0; row < 3; ++row) {
                result[axis * 4 + row] *= node.scale[axis];
            }
        }
        return result;
    }

    std::size_t sectionEnd(const roommodel::Model& model, std::size_t start) {
        std::size_t end = model.fileSize;
        for (const auto& section : model.sections) {
            if (section.fileOffset > start) {
                end = std::min(end, static_cast<std::size_t>(section.fileOffset));
            }
        }
        return end;
    }

    void readBatch(const std::vector<std::uint8_t>& data, const roommodel::Model& model,
        std::uint16_t batch, std::uint16_t material, const Matrix& matrix, roomgeometry::Mesh& mesh) {
        const std::size_t batches = read32(data, 0x38);
        const std::size_t batchEnd = sectionEnd(model, batches);
        const std::size_t header = batches + static_cast<std::size_t>(batch) * 24;
        range(data, header, 24, batchEnd);
        const std::size_t size = static_cast<std::size_t>(read16(data, header + 2)) * 32;
        const std::uint32_t attributes = read32(data, header + 4);
        require((attributes & (1 << 9)) != 0 && (attributes & ~0x1ffe00U) == 0, "Unsupported vertex attribute layout.");
        std::size_t stride = 0;
        std::size_t coordinateOffset = 0;
        for (unsigned attribute = 9; attribute <= 20; ++attribute) {
            if ((attributes & (1U << attribute)) != 0) {
                if (attribute == 13) coordinateOffset = stride;
                stride += attribute == 10 && data[header + 11] != 0 ? 6 : 2;
            }
        }
        const std::size_t positions = read32(data, 0x14);
        const std::size_t positionEnd = sectionEnd(model, positions);
        require(positions >= 0x60, "Missing position section.");
        std::size_t cursor = batches + static_cast<std::size_t>(read32(data, header + 12));
        range(data, cursor, size, batchEnd);
        const std::size_t end = cursor + size;
        struct Vertex {
            roomgeometry::Position position{};
            std::array<float, 2> coordinate{};
        };
        const auto triangle = [&](const Vertex& a, const Vertex& b, const Vertex& c) {
            require(mesh.triangles.size() < 2000000, "Room exceeds the triangle limit.");
            mesh.triangles.push_back({{a.position, b.position, c.position}, material, {a.coordinate, b.coordinate, c.coordinate}});
        };
        while (cursor < end) {
            const std::uint8_t command = data[cursor++];
            if (command == 0) {
                continue;
            }
            require((command & 7) == 0, "Unsupported vertex format index.");
            const unsigned primitive = command & 0xf8;
            require(primitive == 0x80 || primitive == 0x90 || primitive == 0x98 || primitive == 0xa0, "Unsupported display-list command.");
            range(data, cursor, 2, end);
            const std::size_t count = read16(data, cursor);
            cursor += 2;
            require(count >= 3 && (primitive != 0x80 || count % 4 == 0) && (primitive != 0x90 || count % 3 == 0), "Invalid primitive vertex count.");
            range(data, cursor, count * stride, end);
            std::vector<Vertex> vertices;
            vertices.reserve(count);
            for (std::size_t index = 0; index < count; ++index) {
                const std::size_t offset = positions + static_cast<std::size_t>(read16(data, cursor)) * 6;
                range(data, offset, 6, positionEnd);
                roomgeometry::Position raw{};
                roomgeometry::Position vertex{};
                for (std::size_t axis = 0; axis < 3; ++axis) {
                    const std::uint16_t value = read16(data, offset + axis * 2);
                    raw[axis] = static_cast<float>(value < 0x8000 ? static_cast<int>(value) : static_cast<int>(value) - 0x10000);
                }
                for (std::size_t row = 0; row < 3; ++row) {
                    vertex[row] = matrix[12 + row];
                    for (std::size_t axis = 0; axis < 3; ++axis) {
                        vertex[row] += matrix[axis * 4 + row] * raw[axis];
                    }
                    require(std::isfinite(vertex[row]), "Invalid transformed position.");
                }
                Vertex output{vertex, {}};
                if ((attributes & (1U << 13)) != 0) {
                    const std::size_t coordinates = read32(data, 0x24);
                    require(coordinates >= 0x60, "Missing texture coordinate section.");
                    const std::size_t coordinate = coordinates + static_cast<std::size_t>(read16(data, cursor + coordinateOffset)) * 8;
                    range(data, coordinate, 8, sectionEnd(model, coordinates));
                    for (std::size_t axis = 0; axis < 2; ++axis) {
                        const std::uint32_t bits = read32(data, coordinate + axis * 4);
                        std::memcpy(&output.coordinate[axis], &bits, sizeof(bits));
                        require(std::isfinite(output.coordinate[axis]), "Invalid texture coordinate.");
                    }
                }
                vertices.push_back(output);
                cursor += stride;
            }
            if (primitive == 0x90) {
                for (std::size_t index = 0; index < count; index += 3) {
                    triangle(vertices[index], vertices[index + 1], vertices[index + 2]);
                }
            } else if (primitive == 0x80) {
                for (std::size_t index = 0; index < count; index += 4) {
                    triangle(vertices[index], vertices[index + 1], vertices[index + 2]);
                    triangle(vertices[index], vertices[index + 2], vertices[index + 3]);
                }
            } else {
                for (std::size_t index = 2; index < count; ++index) {
                    if (primitive == 0xa0) {
                        triangle(vertices[0], vertices[index - 1], vertices[index]);
                    } else if (index % 2 == 0) {
                        triangle(vertices[index - 2], vertices[index - 1], vertices[index]);
                    } else {
                        triangle(vertices[index - 1], vertices[index - 2], vertices[index]);
                    }
                }
            }
        }
        ++mesh.batches;
    }
}

bool roomgeometry::decode(const std::vector<std::uint8_t>& data, Mesh& mesh, std::string& error,
    const std::vector<roommodel::Node>& pose) {
    mesh = {};
    roommodel::Model model;
    if (!roommodel::inspect(data, model, error)) {
        return false;
    }
    try {
        Mesh result;
        const std::size_t hierarchy = read32(data, 0x3c);
        const std::size_t hierarchyEnd = sectionEnd(model, hierarchy);
        struct Visit {
            std::int32_t index = 0;
            Matrix parent = identity();
        };
        std::vector<Visit> pending{{0, identity()}};
        std::vector<bool> visited(0x8000, false);
        while (!pending.empty()) {
            const Visit visit = pending.back();
            pending.pop_back();
            require(visit.index >= 0 && visit.index < 0x8000 && !visited[visit.index], "Invalid or shared hierarchy node.");
            visited[visit.index] = true;
            const auto found = std::find_if(model.nodes.begin(), model.nodes.end(), [&](const roommodel::Node& node) { return node.index == visit.index; });
            require(found != model.nodes.end(), "Missing hierarchy node.");
            require(pose.empty() || static_cast<std::size_t>(visit.index) < pose.size(), "Missing model pose node.");
            const Matrix matrix = multiply(visit.parent, transform(pose.empty() ? *found : pose[visit.index]));
            const std::size_t node = hierarchy + static_cast<std::size_t>(visit.index) * 0x8c;
            const std::size_t count = read16(data, node + 0x4c);
            const std::size_t parts = hierarchy + static_cast<std::size_t>(read32(data, node + 0x50));
            range(data, parts, count * 4, hierarchyEnd);
            for (std::size_t index = 0; index < count; ++index) {
                readBatch(data, model, read16(data, parts + index * 4 + 2), read16(data, parts + index * 4), matrix, result);
            }
            if (found->link4 >= 0) {
                pending.push_back({found->link4, visit.parent});
            }
            if (found->link2 >= 0) {
                pending.push_back({found->link2, matrix});
            }
        }
        require(!result.triangles.empty(), "Room contains no supported triangles.");
        result.minimum = result.maximum = result.triangles.front().positions[0];
        for (const auto& triangle : result.triangles) {
            for (const auto& position : triangle.positions) {
                for (std::size_t axis = 0; axis < 3; ++axis) {
                    result.minimum[axis] = std::min(result.minimum[axis], position[axis]);
                    result.maximum[axis] = std::max(result.maximum[axis], position[axis]);
                }
            }
        }
        mesh = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
