#include "game/actors/actorcollisionvolume.h"

#include <cstring>
#include <stdexcept>

namespace {
    float real(std::uint32_t bits) {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    struct Decoded {
        actorcollision::Cylinder cylinder;
        actorcollision::Box box;
        actorcollision::Shape shape;

        Decoded(unsigned kind, const actorcollision::Record* record, const actorcollision::ExtendedRecord* extended) {
            shape.kind = static_cast<int>(kind);
            if (kind == 1) {
                if (!record) throw std::runtime_error("Original cylinder collision record is absent.");
                cylinder = {real((*record)[0]), real((*record)[1]), {real((*record)[2]), real((*record)[3]), real((*record)[4])}};
                shape.cylinder = &cylinder;
            } else if (kind == 2) {
                if (!extended) throw std::runtime_error("Original box collision record is absent.");
                box = {{real((*extended)[0]), real((*extended)[1]), real((*extended)[2])},
                    {real((*extended)[3]), real((*extended)[4]), real((*extended)[5])}};
                shape.box = &box;
            }
        }

        explicit Decoded(const actorcollision::Volume& volume) : Decoded(volume.field8,
            volume.secondary ? &*volume.secondary : nullptr, volume.extended ? &*volume.extended : nullptr) {}
    };
}

bool actorcollision::intersect(Vector& displacement, const Volume& first, const Vector& firstPosition,
    const Volume& second, const Vector& secondPosition) {
    if (first.field8 < 1 || first.field8 > 2 || second.field8 < 1 || second.field8 > 2) return false;
    const Decoded a(first), b(second);
    return intersect(displacement, a.shape, firstPosition, b.shape, secondPosition);
}
