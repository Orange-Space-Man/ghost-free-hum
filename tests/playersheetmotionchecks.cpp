#include "game/player/playersheetmotion.h"

#include <cmath>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
    bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }
}

void checkSheetMotion(const std::vector<std::uint8_t>& executable) {
    playersheets::Builder builder(executable);
    playersheets::MotionParameters parameters(executable);
    require(parameters.iterations == 1 && near(parameters.attachedDamping, 0.9f) &&
        near(parameters.detachedDamping, 0.8f) && parameters.gravity == -2 &&
        near(parameters.positionResponse, 0.7f) && near(parameters.velocityResponse, 0.4f),
        "Sheet update parameters are recovered from the original executable.");
    playerscanner::SheetInput input{0, 0, 0, 1, 0, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}, {100, 50, 100}, false, false};
    auto layout = builder.build(input, 256, 0, 13, false);
    layout.response = 100;
    playersheets::Grid grid(256);
    grid.initialize(layout);
    playersheets::Motion motion(grid, executable);
    motion.drops = {};
    motion.pullDirection = {1, 0};
    motion.pulling = true;
    motion.retract(true);
    require(motion.drops[0] == -100 && !motion.halfway && !motion.detached && !motion.pulling,
        "Exactly half-width does not cross the original strict retraction threshold.");
    const auto originalPosition = grid.points()[4 * 16 + 4].position;
    require(motion.crop() && motion.cropBounds[0] == 0.5f, "Negative hanging length crops the original surface.");
    require(!grid.points()[4 * 16].active && grid.points()[4 * 16 + 4].fixed &&
        grid.points()[4 * 16 + 4].position == originalPosition && grid.points()[4 * 16 + 4].anchor[0] == 0,
        "Cropping disables the withdrawn edge and moves anchors without resetting simulated positions.");
    require(!motion.crop(), "Unchanged attached crop preserves the original early return.");
    motion.pulling = true;
    motion.retract(true);
    require(motion.halfway && !motion.detached && motion.drops[0] == -200,
        "Exactly full-width still does not detach the sheet.");
    motion.pulling = true;
    motion.retract(true);
    require(motion.detached && motion.pulling && motion.drops[0] == -200,
        "Crossing full-width clamps the edge and retains the original pulling flag.");
    motion.crop();
    require(!grid.points()[4 * 16 + 4].fixed && !grid.points()[4 * 16 + 4].edge,
        "Detached sheets release both fixed and edge flags.");
    require(motion.crop(), "Detached sheets recalculate even an unchanged crop.");

    grid.initialize(layout);
    playersheets::Motion allEdges(grid, executable);
    allEdges.drops.fill(100);
    allEdges.retracting = true;
    allEdges.retract(true);
    require(!allEdges.detached && allEdges.retracting && allEdges.drops == std::array<float, 4>{},
        "Reaching zero on every edge does not complete uniform retraction.");
    allEdges.retract(true);
    require(allEdges.detached, "Uniform retraction completes only after every edge crosses zero.");
    playersheets::Motion lostOwner(grid, executable);
    lostOwner.drops.fill(300);
    lostOwner.retracting = true;
    lostOwner.retract(false);
    require(!lostOwner.retracting && !lostOwner.detached && lostOwner.drops[0] == 200,
        "Losing suction ownership clears retraction after applying this update.");

    auto& freePoint = grid.points()[0];
    auto& edgePoint = grid.points()[4 * 16 + 4];
    auto& centerPoint = grid.points()[7 * 16 + 7];
    freePoint.velocity = edgePoint.velocity = centerPoint.velocity = {10, 10, 10};
    playersheets::Motion forces(grid, executable);
    forces.damp(parameters);
    forces.applyGravity(parameters);
    require(near(freePoint.velocity[1], 7) && edgePoint.velocity[1] == 10 && near(centerPoint.velocity[1], 9),
        "Attached damping excludes border points; gravity also excludes fixed interior points.");
    forces.detached = true;
    forces.damp(parameters);
    const auto detachedVelocity = freePoint.velocity;
    forces.applyGravity(parameters);
    require(freePoint.velocity == detachedVelocity && near(edgePoint.velocity[1], 8),
        "Detached damping includes border points while the original gravity stage stops.");
    forces.detached = false;
    centerPoint.position = centerPoint.anchor;
    centerPoint.position[0] += 14;
    centerPoint.velocity = {1, 2, 3};
    forces.constrainAnchors(parameters);
    require(centerPoint.position == centerPoint.anchor && centerPoint.velocity == playerscanner::Vector{},
        "Sub-threshold unsucked anchor displacement snaps back and clears velocity.");
    centerPoint.position[0] += 15;
    forces.constrainAnchors(parameters);
    require(near(centerPoint.position[0] - centerPoint.anchor[0], 4.5f) && near(centerPoint.velocity[0], -6),
        "The anchor threshold uses position and velocity response instead of snapping.");

    const playersheets::Plane floor{{0, 1, 0}, 0};
    playersheets::Point collision;
    collision.position = {2, -3, 4};
    collision.velocity = {5, -10, 6};
    require(forces.collide(collision, floor, false) == 10 && collision.position[1] == 0 && collision.velocity[1] == 0 &&
        collision.velocity[0] == 5, "Cloth-plane collision removes only inward normal velocity.");
    collision.position[1] = -3;
    collision.velocity[1] = -10;
    require(forces.collide(collision, floor, true) == 10 && near(collision.velocity[1], 7),
        "Elastic plane response preserves the original 1.7 multiplier.");
    collision.position[1] = -1;
    collision.velocity[1] = 2;
    require(forces.collide(collision, floor, false) == 1 && collision.velocity[1] == 2,
        "Penetration with outward velocity returns the original sentinel one.");
    require(forces.collide(collision, floor, false) == 0, "Exact plane contact is not penetration.");
    require(forces.collisionPlane(0) == nullptr && forces.collisionPlane(7 * 16 + 7) == &grid.planes()[0] &&
        forces.collisionPlane(7 * 16) == &grid.planes()[1] && forces.collisionPlane(7) == &grid.planes()[2] &&
        forces.collisionPlane(7 * 16 + 15) == &grid.planes()[3] && forces.collisionPlane(15 * 16 + 7) == &grid.planes()[4],
        "Original cloth plane selection excludes corner overhangs and retains all five face choices.");

    grid.initialize(layout);
    playersheets::Motion captured(grid, executable);
    grid.points()[0].touched = true;
    const playerscanner::Vector suction{20, 30, 40};
    captured.constrainNeighbors(parameters, &suction);
    require(grid.points()[0].position == suction && grid.points()[0].velocity == playerscanner::Vector{},
        "Captured points end the original constraint sweep at their owner's suction position.");
    captured.integrate();
    require(grid.points()[0].position == suction, "Integration leaves captured points at the suction owner.");

    for (unsigned kind = 1; kind <= 24; ++kind) {
        input.kind = kind;
        grid.initialize(builder.build(input, 256, 0, 13, false));
        playersheets::Motion stage(grid, executable);
        parameters.reciprocalConstraints = kind % 2 != 0;
        for (unsigned frame = 0; frame < 60; ++frame) {
            stage.damp(parameters);
            stage.applyGravity(parameters);
            stage.constrainAnchors(parameters);
            stage.constrainNeighbors(parameters, nullptr);
            stage.integrate();
            stage.updateNormals();
        }
        for (const auto& point : grid.points()) {
            for (float value : point.position) require(std::isfinite(value), "All original sheet orientations retain finite numerical positions.");
            for (float value : point.normal) require(std::isfinite(value), "All original sheet orientations retain finite numerical normals.");
        }
    }
}
