#include "game/models/modelshadowgeometry.h"

namespace modelshadow {
    s3dmodel::Batch prepare(const doors::ModelResource& resource, const modeldraw::DrawCommand& command,
        const dooranimation::Matrix& matrix) {
        s3dmodel::Batch result;
        result.cull = resource.drawing.batch(command.batch).cull;
        const auto vertex = [&](const modeldraw::Vertex& source) {
            s3dmodel::Vertex value;
            for (unsigned row = 0; row < 3; ++row)
                value.position[row] = matrix[row * 4] * source.position[0] + matrix[row * 4 + 1] * source.position[1] +
                    matrix[row * 4 + 2] * source.position[2] + matrix[row * 4 + 3];
            result.vertices.push_back(value);
        };
        for (const auto& primitive : resource.geometry.at(command.batch)) {
            const auto count = primitive.vertices.size();
            const auto triangle = [&](unsigned a, unsigned b, unsigned c) {
                vertex(primitive.vertices[a]); vertex(primitive.vertices[b]); vertex(primitive.vertices[c]);
            };
            switch (primitive.command) {
            case 0x80:
                for (unsigned i = 0; i + 3 < count; i += 4) { triangle(i, i + 1, i + 2); triangle(i, i + 2, i + 3); }
                break;
            case 0x90:
                for (unsigned i = 0; i + 2 < count; i += 3) triangle(i, i + 1, i + 2);
                break;
            case 0x98:
                for (unsigned i = 0; i + 2 < count; ++i) triangle(i + (i & 1), i + 1 - (i & 1), i + 2);
                break;
            case 0xa0:
                for (unsigned i = 1; i + 1 < count; ++i) triangle(0, i, i + 1);
                break;
            }
        }
        return result;
    }
}
