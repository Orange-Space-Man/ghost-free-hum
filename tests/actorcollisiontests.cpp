#include "game/actors/actorcollisionquery.h"
#include "game/actors/actorcollisionvolume.h"
#include "game/effects/elementparticlepool.h"
#include "game/player/playerresourcepools.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
}

int main() {
    try {
        using namespace actorcollision;
        {
            elements::Pool pool(1500, 15);
            require(pool.particles.free.count == 1500 && pool.emitters.free.count == 15 && !pool.active.count,
                "Original elemental pool capacities own their free and active rings.");
            auto* first = pool.emitters.free.take();
            auto* second = pool.emitters.free.take();
            first->type = second->type = 10;
            pool.active.append(*first);
            pool.active.append(*second);
            auto* particle = pool.particles.free.take();
            particle->position = {};
            particle->age = 11;
            second->particles.append(*particle);
            const Cylinder river{350, 10, {}};
            const Shape target{1, &river, nullptr};
            require(!pool.intersects(target, {}, 10) && pool.intersects(*second, target, {}),
                "Element query returns the first matching emitter's miss without checking later matching emitters.");
            first->type = 9;
            require(pool.intersects(target, {}, 10) && !pool.intersects(target, {}, 2),
                "Element query skips other types and excludes types two and below.");
            particle->age = 10;
            require(!pool.intersects(target, {}, 10), "Element particle collision begins strictly after age ten.");
            particle->age = -1;
            require(!pool.intersects(target, {}, 10), "Element age comparison is signed.");
            elements::Projectile ice{{}, 1}, water{{1000, 0, 0}, 1};
            require(elements::intersects(ice, water, target, {}, 10) && !elements::intersects(ice, water, target, {}, 11) &&
                !elements::intersects(ice, water, target, {}, 9), "Element projectile queries select the original ice and water owners.");
            ice.active = 3;
            require(!elements::intersects(ice, water, target, {}, 10),
                "Element projectile activation is an exact integer-one comparison, not an active-bit test.");
        }
        const Cylinder cylinder{2, 4, {}};
        const Box box{{4, 4, 4}, {}};
        Vector response{9, 8, 7};
        require(!intersect(response, cylinder, {}, cylinder, {4, 0, 0}) && response == Vector{9, 8, 7},
            "Cylinder horizontal tangency does not overlap and preserves the response.");
        require(intersect(response, cylinder, {}, cylinder, {0, 4, 0}) && response == Vector{},
            "Cylinder vertical tangency overlaps with zero displacement.");
        require(intersect(response, cylinder, {}, cylinder, {}) && response == Vector{4, 0, 0},
            "Coincident cylinders resolve horizontally along positive X on a penetration tie.");
        require(intersect(response, cylinder, {3, 0, 0}, cylinder, {}) && response == Vector{1, 0, 0},
            "Cylinder side overlap returns the original outward displacement.");
        require(intersect(response, cylinder, {0, -3, 0}, cylinder, {}) && response == Vector{0, -1, 0},
            "Cylinder lower overlap resolves downward.");
        require(intersect(response, cylinder, {0, 3, 0}, cylinder, {}) && response == Vector{0, 1, 0},
            "Cylinder upper overlap resolves upward.");
        require(intersect(response, cylinder, {}, Cylinder{2, 1, {}}, {0, 1, 0}) && response == Vector{4, 0, 0},
            "Contained cylinder retains the original summed-height candidate rather than a replacement minimum penetration.");
        response = {9, 8, 7};
        require(!intersect(response, box, {}, box, {4, 0, 0}) && response == Vector{9, 8, 7},
            "Box tangency is excluded and preserves prior output.");
        require(intersect(response, box, {1, 1, 1}, box, {}) && response == Vector{0, 0, 3},
            "Equal box penetration chooses Z before Y and X.");
        require(intersect(response, box, {1, 1, 0}, box, {}) && response == Vector{0, 3, 0},
            "A box X/Y penetration tie chooses Y.");
        require(intersect(response, box, {}, box, {}) && response == Vector{},
            "Coincident boxes overlap with zero response in the original direction calculation.");
        require(intersect(response, cylinder, {3, -1, 0}, box, {}) && response == Vector{1, 0, 0},
            "Cylinder-box side overlap chooses the X response.");
        require(intersect(response, cylinder, {0, -1, 3}, box, {}) && response == Vector{0, 0, 1},
            "Cylinder-box central X region chooses the Z response.");
        require(intersect(response, cylinder, {0, -1, 0}, box, {}) && response == Vector{},
            "Centered cylinder-box overlap preserves the original zero-direction quirk.");
        require(!intersect(response, cylinder, {3.5f, -1, 3.5f}, box, {}),
            "Cylinder-box expanded rectangle corner is rejected outside the circular radius.");
        require(intersect(response, cylinder, {3, -1, 3}, box, {}) && response[1] == 0 &&
            std::abs(response[0] - 0.41421356f) < 0.000001f && response[0] == response[2],
            "Cylinder-box corner overlap preserves the radial response direction.");
        require(intersect(response, cylinder, {0, -6, 0}, box, {}) && response == Vector{},
            "Cylinder-box vertical tangency is included.");
        const Shape round{1, &cylinder, nullptr}, square{2, nullptr, &box};
        require(intersect(response, square, {}, round, {3, -1, 0}) && response == Vector{-1, 0, 0},
            "Reversed mixed-shape dispatch negates the cylinder response.");
        response = {9, 8, 7};
        require(!intersect(response, square, {}, round, {8, -1, 0}) && response == Vector{-9, -8, -7},
            "Reversed mixed-shape dispatch negates prior output even when no intersection occurs.");
        response = {9, 8, 7};
        require(!intersect(response, Shape{}, {}, round, {}) && response == Vector{9, 8, 7},
            "Unsupported original shape kinds return false without touching output.");
        const Cylinder offsetCylinder{2, 4, {1, 2, 3}};
        require(intersect(response, offsetCylinder, {2, -2, -3}, cylinder, {}) && response == Vector{1, 0, 0},
            "Original local shape offsets are added before collision tests.");
        Volume roundRecord{1, Record{0x40000000, 0x40800000, 0, 0, 0}, {}};
        Volume boxRecord{2, {}, ExtendedRecord{0x40800000, 0x40800000, 0x40800000, 0, 0, 0}};
        require(intersect(response, roundRecord, {3, 0, 0}, boxRecord, {}) && response == Vector{1, 0, 0},
            "Native collision records decode cylinder and box dimensions in original word order.");
        require(intersect(response, boxRecord, {}, roundRecord, {3, 0, 0}) && response == Vector{-1, 0, 0},
            "Native volume dispatch preserves reversed mixed-shape response.");
        {
            playerresources::RecordPool44 pool(2);
            require(!pool.contact(response, roundRecord, {}) && response == Vector{},
                "An empty player collision pool returns zero displacement.");
            require(pool.append(boxRecord, {}) && pool.append(boxRecord, {1, 0, 0}) && !pool.append(boxRecord, {}),
                "Player collision registration retains the original fixed capacity.");
            boxRecord.extended->at(0) = 0;
            require(pool.contact(response, roundRecord, {3, 0, 0}) && response == Vector{2, 0, 0},
                "Registered volumes own their data and accumulate each correction before testing the next record.");
            require(pool.field8 == 2 && pool.fieldC == 0,
                "Contact traversal consumes registered entries rather than the separate display count.");
            pool.finishContacts();
            require(pool.field8 == 0 && pool.fieldC == 2, "The contact pass publishes the display count and releases active registrations.");
            require(!pool.contact(response, roundRecord, {}) && response == Vector{},
                "Resetting the active count makes retained records inactive.");
        }
        response = {7, 8, 9};
        require(!intersect(response, Volume{0, {}, {}}, {}, Volume{1, {}, {}}, {}) && response == Vector{7, 8, 9},
            "Inactive volume kind returns before reading either shape record.");
        std::cout << "Actor collision query checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
