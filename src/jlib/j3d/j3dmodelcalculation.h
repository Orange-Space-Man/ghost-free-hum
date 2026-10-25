#pragma once

#include "jlib/j3d/j3dmodelinstance.h"

namespace j3dmodel {
    class TransformTables {
    public:
        explicit TransformTables(const std::vector<std::uint8_t>& executable);
        Matrix transform(const Joint& joint) const;

    private:
        std::array<float, 4096> sine, cosine;
    };

    Matrix concatenateMatrix(const Matrix& parent, const Matrix& local);
    void calculateBasic(ModelInstance& instance, const TransformTables& tables);
    bool inverseTranspose(const Matrix& matrix, NormalMatrix& output);
    void calculateViewMatrices(ModelInstance& instance, const Matrix& camera);
}
