#pragma once

#include "game/player/playervacuumentries.h"
#include "game/actors/actorcollisionowner.h"
#include "game/actors/actorinitialpose.h"
#include "game/player/playervacuummodel.h"
#include <array>
#include <functional>
#include <vector>

namespace vacuumcapture {
    struct VacuumInput;
    // One live enemy slot as 8008487C sees it (kind 2 slots of the EnManager list).
    struct Candidate {
        actors::MoveActor* actor = nullptr;
        std::function<std::uint32_t(characterevent::Event&)> event; // 800C0F08 on the slot's owner
    };

    struct ScanServices {
        Controller& entries;
        const actorcollision::Catalog& shapes;
        const actormodel::InitialPose& pose;
        // 800E98E0 + the 800E9DC8 filter: the live slots (kind 2) whose type group is not -1.
        std::function<std::vector<Candidate>()> candidates;
        std::function<int(unsigned)> typeGroup;          // 8006A3D8 + 0x14
        std::function<int(unsigned)> typeLife;           // 800DAD90 (parameter index 0)
        std::function<bool(unsigned, unsigned)> roomVisible; // 8001CE1C(room, buffer)
        std::function<void(unsigned, const Vector*)> sound; // 8018A1E0 / 80189F78 (null = no position)
        std::function<void(float, const Vector&, unsigned)> loopSound; // 8018ACF8
        // 800C0F08 on the owner of an arbitrary actor (80097F00 finds the owner).
        std::function<void(actors::MoveActor&, characterevent::Event&)> ownerEvent;
        std::function<void(const Vector&)> particle;     // 80164F5C(3, ...)
        std::function<void(const struct VacuumInput&)> beginBeam; // 800847E8 (virtual +0xC of the beam model)
        std::function<void(float, const struct VacuumInput&)> updateBeam; // 80084CE8 (virtual +0x34)
    };

    // 80084550 input record (copied to +0x60..+0x7C of the vacuum object).
    struct VacuumInput {
        actors::MoveActor* player = nullptr; // [0] -> +0x60
        const Vector* hand = nullptr;        // [1] -> +0x64
        float strength = 0;                  // [2] -> +0x68
        bool scan = false;                   // [3] -> +0x6C
        std::int32_t mode = 0;               // [4] -> +0x70 (3 = capture button held)
        const playermodels::VacuumVolume* volume = nullptr; // [5] -> +0x74 (overrides the beam model when set)
        const playermodels::VacuumParameters* parameters = nullptr; // [6] -> +0x78
        const playermodels::VacuumParameters* target = nullptr;     // [7] -> +0x7C
    };

    // The player's vacuum object built by 80084070 (native `playercomponents::Vacuum` keeps these as raw words).
    class Vacuum {
    public:
        Vacuum(ScanServices services, const playermodels::VacuumVolume& beam, std::uint32_t debugFlags = 0xd);
        // 80084550: returns 1 when a target was admitted (uVar3 of the original).
        unsigned update(const VacuumInput& input);
        // 80084BC0 needs the attached actors; the writer of the original +0xC list is not traced yet.
        void attach(actors::MoveActor& actor);
        float strength = 0;                       // +0x04
        bool active = false;                      // +0x08
        bool started = false;                     // +0x09
        bool holding = false;                     // +0x0A
        std::array<actors::MoveActor*, 16> attached{}; // +0x0C
        std::array<float, 5> tint{0, 0, 0, 1, 2};      // +0x4C..+0x5C (native fields4C)
        void reset();                                  // 80084268
        const playermodels::VacuumVolume& beam() const { return beam_; }
        VacuumInput input;                        // +0x60..+0x7C

    private:
        ScanServices services_;
        const playermodels::VacuumVolume& beam_;
        std::uint32_t debugFlags_;
        unsigned scan();
        unsigned admit(const Candidate& candidate);
        bool inside(const actors::MoveActor& actor, const playermodels::VacuumVolume& volume) const;
        void releaseAttached();
    };
}
