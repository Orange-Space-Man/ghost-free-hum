#include "game/actors/characterownership.h"
#include "game/actors/moveactor.h"
#include "game/world/dooranimationpose.h"

#include <cmath>
#include <stdexcept>

namespace {
    std::array<float, 3> transform(const actormodel::Matrix& matrix, const std::array<float, 3>& point) {
        std::array<float, 3> result;
        for (unsigned row = 0; row < 3; ++row) {
            float value = point[0] * matrix[row * 4];
            value = std::fma(point[1], matrix[row * 4 + 1], value);
            value = std::fma(point[2], matrix[row * 4 + 2], value);
            result[row] = value + matrix[row * 4 + 3];
        }
        return result;
    }
}

void characterownership::Ownership::updateAttachments(const actors::MoveActor& actor) {
    if (attachmentCount > attachments.size()) throw std::runtime_error("Invalid original character attachment count.");
    unsigned index = 0;
    while (index < attachmentCount) {
        auto& attachment = attachments[index];
        if (!attachment.resource) throw std::runtime_error("Original character attachment is null.");
        if (attachment.resource->attachmentState()) {
            for (unsigned next = index + 1; next < attachmentCount; ++next) attachments[next - 1] = attachments[next];
            --attachmentCount;
            continue;
        }
        const auto local = actor.model ? transform(actor.model->matrix(0, attachment.joint), attachment.position) :
            std::array<float, 3>{};
        const auto root = actor.model ? dooranimation::concatenate(actor.matrix, actor.model->matrix(0, 0)) : actor.matrix;
        const actormodel::Matrix translation{1, 0, 0, local[0], 0, 1, 0, local[1], 0, 0, 1, local[2]};
        const auto matrix = dooranimation::concatenate(root, translation);
        const auto position = transform(root, local);
        attachment.resource->setAttachmentPosition(position, {});
        attachment.resource->setMatrix(matrix);
        attachment.resource->setVisible((actor.fieldE0 & 255) && !(actor.flags & 0xc000));
        ++index;
    }
}
