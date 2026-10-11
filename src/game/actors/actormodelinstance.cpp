#include "game/actors/actormodelinstance.h"

#include <stdexcept>

namespace {
    unsigned half(const std::vector<std::uint8_t>& bytes, unsigned offset) {
        return (unsigned(bytes.at(offset)) << 8) | bytes.at(offset + 1);
    }

    constexpr actormodel::Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
}

actormodel::Instance::Instance(ResourceRef resource, unsigned capacity) : model(std::move(resource)), count(capacity) {
    const unsigned required = model ? half(model->bytes, 8) + half(model->bytes, 12) : capacity;
    if (!capacity) {
        count = required;
    } else if (capacity < required) {
        throw std::runtime_error("Actor model capacity is smaller than the original model requires.");
    }
    if (count > 0x7fffffff / 0xcc) {
        throw std::runtime_error("Actor model workspace exceeds the original allocation range.");
    }
    matrices.reset(new Matrix[std::size_t(count) * 4]);
    attachments.reset(new Attachment[count]);
    for (unsigned i = 0; i < count; ++i) {
        matrix(0, i) = identity;
        matrix(3, i) = identity;
    }
}

unsigned actormodel::Instance::capacity() const {
    return count;
}

void actormodel::Instance::bindKey(ResourceRef resource) {
    if (key == resource) {
        return;
    }
    key = std::move(resource);
    resetCursors();
}

void actormodel::Instance::resetCursors() {
    if (!model) {
        return;
    }
    const unsigned nodes = half(model->bytes, 8);
    for (unsigned i = 0; i < nodes; ++i) {
        auto& value = attachment(i);
        value.field0 = 0;
        value.field2 = 0;
        value.field4 = 0;
    }
}

actormodel::Matrix& actormodel::Instance::matrix(unsigned bank, unsigned index) {
    if (bank >= 4 || index >= count) {
        throw std::out_of_range("Actor model matrix index.");
    }
    return matrices[std::size_t(bank) * count + index];
}

const actormodel::Matrix& actormodel::Instance::matrix(unsigned bank, unsigned index) const {
    if (bank >= 4 || index >= count) {
        throw std::out_of_range("Actor model matrix index.");
    }
    return matrices[std::size_t(bank) * count + index];
}

actormodel::Attachment& actormodel::Instance::attachment(unsigned index) {
    if (index >= count) {
        throw std::out_of_range("Actor model attachment index.");
    }
    return attachments[index];
}
