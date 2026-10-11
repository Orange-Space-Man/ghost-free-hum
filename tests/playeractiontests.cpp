#include "game/player/playeractionstate.h"
#include "game/player/playeractionupdate.h"
#include "game/player/playerreactionframe.h"
#include "resources/gameassetpackage.h"
#include "game/actors/actortimingsampler.h"

#include <iostream>
#include <cstring>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    std::vector<std::uint8_t> fixture() {
        std::vector<std::uint8_t> data(0x2e0100);
        const auto word = [&](unsigned offset, unsigned value) {
            for (unsigned i = 0; i < 4; ++i) data.at(offset + i) = static_cast<std::uint8_t>(value >> (24 - i * 8));
        };
        word(0, 0x100);
        word(0x48, 0x80200000);
        word(0x90, 0x2e0000);
        word(0x1528dc, 1234);
        const std::array<float, 14> constants{0, 1, 17, 0, 176, 0, 13, 0.8f, 22.5f, 180, 360.0f / 65536, 360, 1.0f / 45, 0.03f};
        for (unsigned i = 0; i < constants.size(); ++i) {
            unsigned bits;
            std::memcpy(&bits, &constants[i], 4);
            word(0x2da980 + i * 4, bits);
        }
        const unsigned directions[]{18, 30, 24, 36, 21, 39, 27, 33};
        for (unsigned i = 0; i < 3; ++i) for (unsigned j = 0; j < 8; ++j) word(0x151e38 + i * 32 + j * 4, directions[j] + i);
        return data;
    }

    void check(const std::vector<std::uint8_t>& executable) {
        {
            playeractions::Actions actions(executable, 0);
            std::array<std::uint32_t, 12> values{};
            values[6] = 19;
            values[8] = 1;
            values[9] = 77;
            require(actions.request(12, &values) && actions.field314 == 32 && actions.fields31C[0] == 1 &&
                actions.field358 == 19 && actions.fields31C[11] == 1 && actions.fields31C[12] == 77,
                "Interaction requests map their payload into the original protected reaction fields.");
            actions.field318 = 8;
            actions.field2FC = 0;
            require(!actions.request(16) && actions.field314 == 32 && actions.field318 == 8 && !actions.field2FC,
                "Protected interactions reject unrelated action requests without resetting their clock.");
            require(actions.request(0x14) && actions.fields31C[0] == 2 && actions.field318 == 0 && actions.field2FC,
                "Accepted interaction requests reset the retained reaction clock and publish the change latch.");
            actions.fields31C[12] = 1;
            require(actions.request(0x15) && actions.fields31C[0] == 3 && actions.request(0x15) && actions.fields31C[0] == 8,
                "The original interaction completion advances through its two distinct states.");
            actions.field310 = 1;
            values[10] = 51;
            require(actions.request(13, &values) && actions.field314 == 33 && actions.fields31C[1] == 1,
                "The second protected interaction uses its own retained state field.");
            require(actions.request(0x1b) && actions.fields31C[1] == 2, "Protected stage one accepts its transition request.");
            actions.fields31C[1] = 3;
            require(actions.request(0x1e, &values) && actions.fields31C[1] == 6 && actions.fields31C[13] == 51,
                "Protected interaction payload publication preserves the original destination field.");
            actions.field310 = 1;
            require(!actions.request(14) && actions.field310 == 2 && actions.field314 == 34 && actions.fields31C[3] == 2,
                "Request fourteen changes reaction while retaining the original false return value.");
            actions.fields31C[3] = 3;
            require(actions.request(0x20) && actions.fields31C[3] == 4, "The third protected reaction completes through its own request.");
            actions.field310 = 1;
            require(actions.request(16) && actions.field314 == 30, "Environment inversion selects the original action reaction.");
            actions.field310 = 1;
            values[2] = 2;
            require(actions.request(5, &values) && actions.fields31C[7] == 2, "Action requests supply the otherwise unwritten reaction payload field.");
            playeractions::Reactions reactions(executable, 0);
            reactions.state = 4;
            reactions.field4 = 1.5f;
            unsigned sounds = 0;
            const auto sound = [&](unsigned id, const std::array<float, 3>& position) {
                require(id == 0x281c && position == std::array<float, 3>{1, 2, 3}, "Reaction expiry retains the original sound and position.");
                ++sounds;
            };
            playeractions::advanceState(reactions, &actions, false, false, {1, 2, 3}, sound);
            require(reactions.field4 == 1.5f, "Disabled reaction frames preserve the duration.");
            playeractions::advanceState(reactions, &actions, true, false, {1, 2, 3}, sound);
            require(reactions.field4 == 0.5f && reactions.state == 4 && sounds == 0,
                "Reaction clocks advance by one original simulation tick.");
            playeractions::advanceState(reactions, &actions, true, false, {1, 2, 3}, sound);
            require(reactions.state == 1 && reactions.field4 == 0 && actions.field314 == 6 && actions.field354 == 1 && sounds == 1,
                "Expired state four requests its original release animation and restores normal state before audio.");
            reactions.state = 2;
            reactions.field4 = 1;
            playeractions::advanceState(reactions, &actions, true, true, {}, sound);
            require(reactions.state == 1 && actions.field314 == 6 && sounds == 1,
                "Silent expiry resets the component without changing the action or playing sound.");
        }
        playeractions::ReactionPayload payload;
        require(!payload.fields[3] && !payload.fields[8] && payload.fields[15] == 0,
            "Original payload initialization leaves two fields unwritten.");
        payload.fields[3] = 71;
        payload.fields[8] = 72;
        payload.fields[4] = 73;
        payload.reset();
        require(payload.fields[3] == 71 && payload.fields[8] == 72 && payload.fields[4] == 0,
            "Payload reset must preserve the original untouched fields.");
        playeractions::Actions actions(executable, 0);
        for (unsigned i = 0; i < payload.fields.size(); ++i) payload.fields[i] = i + 10;
        actions.fields31C[4] = 2;
        actions.field318 = 31;
        actions.beginReaction(0x10, payload);
        require(actions.fields31C[4] == 2 && actions.field318 == 0 && actions.field2FC == 1,
            "Reaction sixteen retains normal mode two while restarting the reaction clock.");
        actions.beginReaction(0x20, payload);
        require(actions.fields31C[4] == 0 && actions.fields31C[0] == 10 && actions.field358 == 14 &&
            actions.fields31C[11] == 22 && actions.fields31C[12] == 23,
            "Reaction thirty-two copies its four payload fields and clears mode two.");
        actions.beginReaction(0x21, payload);
        require(actions.fields31C[1] == 11 && actions.fields31C[2] == 12 && actions.fields31C[13] == 24,
            "Reaction thirty-three retains its independent mode and variant fields.");
        actions.field30D = 1;
        payload.fields[7] = 4;
        actions.beginReaction(0x1f, payload);
        require(actions.field30D == 1, "Reaction thirty-one must not apply reaction four's latch reset.");
        actions.beginReaction(4, payload);
        require(actions.field30D == 0, "Reaction four resets the latch for payload kinds four through six.");
        actions.field30D = 1;
        payload.fields[6] = 3;
        actions.beginReaction(3, payload);
        require(!actions.field30D, "Reaction three resets the latch for kinds three and four.");
        bool missing = false;
        try { actions.beginReaction(8, playeractions::ReactionPayload{}); }
        catch (const std::bad_optional_access&) { missing = true; }
        require(missing && actions.field314 == 3, "Unknown original scratch fields must not silently become zero or mutate the action.");
        const std::array<unsigned, 35> fixed{0, 0x37, 0x38, 0, 0, 0, 0x77, 0, 0, 0x82, 0, 0x63,
            0, 0x66, 0x68, 0x69, 0x6b, 0, 0, 0, 0x39, 0x3b, 0x59, 0x5a, 0x5b, 0x64,
            0x65, 0x3c, 0x40, 7, 0x5c, 0x83, 0, 0, 0};
        for (unsigned reaction = 0; reaction < fixed.size(); ++reaction) if (fixed[reaction]) {
            actions.field314 = reaction;
            require(actions.reactionEntry() == fixed[reaction], "Fixed reaction animation index differs from the recovered selector.");
        }
        actions.field314 = 4;
        const unsigned expected[2][7]{{0, 0, 0x6c, 0x6e, 0x6f, 0x70, 0x72}, {0, 0, 0x6d, 0x6e, 0x6f, 0x71, 0x73}};
        for (unsigned variant = 1; variant <= 2; ++variant) for (unsigned kind = 0; kind <= 6; ++kind) {
            actions.fields31C[9] = variant;
            actions.fields31C[6] = kind;
            require(actions.reactionEntry() == expected[variant - 1][kind], "Reaction four variant mapping differs.");
        }
        actions.field314 = 8;
        for (unsigned latch = 0; latch < 2; ++latch) for (unsigned kind = 1; kind <= 5; ++kind) {
            actions.field30D = static_cast<std::uint8_t>(latch);
            actions.fields31C[7] = kind;
            require(actions.reactionEntry() == (kind >= 4 ? 0x79 + kind : (latch ? 0x7e : 0x79) + kind),
                "Reaction eight only changes its first three animations with the retained latch.");
        }
        actions.field314 = 0x20;
        actions.fields31C[0] = 9;
        actions.field30D = 1;
        actions.fields31C[11] = 1;
        require(actions.reactionEntry() == 0x94, "First variant of mode nine ignores the latch.");
        actions.fields31C[11] = 2;
        require(actions.reactionEntry() == 0x95, "Second variant of mode nine honors the latch.");
        actions.field314 = 0x21;
        actions.fields31C[1] = 6;
        actions.fields31C[13] = 1;
        require(actions.reactionEntry() == 0xa1, "Reaction thirty-three mode six retains its reversed variant order.");
        actions.fields31C[13] = 2;
        require(actions.reactionEntry() == 0x99, "Reaction thirty-three second variant selects its original index.");
        actions.fields31C[13] = 0xffffffff;
        require(actions.reactionEntry() == 0, "Unrecognized signed variant retains the original zero result.");

        actions.field310 = 1;
        actions.field30D = 0;
        actions.field11 = 1;
        actions.fields0[1] = 3;
        actions.fields31C[4] = 5;
        actions.fields40[0] = 9;
        actions.updateNormalState();
        require(actions.field314 == 10 && actions.field358 == 3 && actions.field30D == 1 && actions.fields31C[4] == 0,
            "New held action enters reaction ten before the normal-mode transition.");
        actions.field310 = 1;
        actions.field11 = 0;
        actions.field3F = 1;
        actions.fields0[3] = 2;
        actions.updateNormalState();
        require(actions.field314 == 12 && actions.field35C == 2 && !actions.field30D,
            "Released action without an attachment constructs the original release payload.");
        actions.field310 = 1;
        actions.fields31C[4] = 2;
        actions.fields40[0] = 9;
        actions.updateNormalState();
        require(actions.field314 == 0x14 && actions.fields31C[4] == 0,
            "Entering mode nine overwrites the old mode's exit reaction in the same tick.");
        actions.field310 = 1;
        actions.fields31C[4] = 0;
        actions.fields40[0] = 7;
        actions.updateNormalState();
        require(actions.field310 == 1 && actions.fields31C[4] == 7, "Ordinary mode changes do not invent a reaction.");

        actormodel::InitialPose pose(executable);
        playeractions::Update update(executable, pose);
        gameflags::State flags;
        actormodel::Catalog catalog;
        std::vector<unsigned> sounds, durationRequests;
        playeractions::UpdateServices services{flags, catalog,
            [&](unsigned id) { sounds.push_back(id); },
            [&](unsigned entry) { durationRequests.push_back(entry); return 3; }};
        playeractions::Input input;
        input.modes = {1, 1, 0, 1};
        input.thresholds = {0.1f, 0.5f};
        input.maximumSpeed = 10;
        input.useMotion = true;
        actions = playeractions::Actions(executable, 0);
        for (unsigned i = 0; i < actions.entries.size(); ++i) actions.entries[i] = 1000 + i;
        actions.parameters[2] = 999;
        require(update.run(actions, input, services) == 1001 && actions.fields304[2], "Zero motion selects the original idle entry.");
        input.motion = 0.1f;
        require(update.run(actions, input, services) == 1002 && actions.field300 == 1 && actions.fields304[0],
            "Walking starts at the inclusive first threshold and advances the locomotion counter.");
        input.motion = 0;
        require(update.run(actions, input, services) == 1002 && actions.fields304[2] && actions.field300 == 1,
            "Brief movement retains its previous animation until the appropriate foot marker.");
        input.animationFlags[0] = 1;
        require(update.run(actions, input, services) == 1001 && !actions.field300, "Foot completion releases the held locomotion animation.");
        input.animationFlags.fill(0);
        input.motion = 0.5f;
        require(update.run(actions, input, services) == 1004, "Second speed threshold selects the middle walking entry.");
        input.motion = 0.8f;
        require(update.run(actions, input, services) == 1006, "Original high-speed threshold selects the fast entry.");
        input.useMotion = false;
        input.velocity = {3, 99, 4};
        require(update.run(actions, input, services) == 1004 && actions.field50 == 0.5f,
            "Velocity-based selection excludes vertical speed and divides by the current maximum speed.");
        input.useMotion = true;
        input.direction = {0, 0, 1};
        input.velocity = {0, 0, 1};
        input.modes[0] = 3;
        input.motion = 0.8f;
        require(update.run(actions, input, services) == 1020, "Directional walking uses the original angle-table sector and fast animation table.");
        input.flags[2] = 1;
        require(update.run(actions, input, services) == 1045, "Directional override chooses its speed-specific animation.");
        input.flags[2] = 0;
        actions.beginReaction(5, playeractions::ReactionPayload{});
        input.animationFlags[1] = 1;
        update.run(actions, input, services);
        require(actions.field314 == 5, "A newly entered reaction cannot complete on the same update.");
        update.run(actions, input, services);
        require(actions.field314 == 6 && actions.field354 == 1, "Completed reaction five chains to six with its original payload.");
        payload = playeractions::ReactionPayload{};
        payload.fields[4] = 2; payload.fields[5] = 3;
        actions.beginReaction(14, payload);
        actions.field2FC = 0;
        update.advanceReaction(actions, input, services);
        require(actions.field314 == 12 && sounds == std::vector<unsigned>{0x80000032},
            "Suction reaction completion starts the original sound and release reaction in order.");
        actions.field35C = 4;
        actions.field2FC = 0;
        update.advanceReaction(actions, input, services);
        require(flags.value(15) && actions.field310 == 1, "Release kind four sets the original global flag.");
        actions.beginReaction(1, playeractions::ReactionPayload{});
        actions.field2FC = 0;
        actions.field318 = 2;
        input.animationFlags.fill(0);
        update.advanceReaction(actions, input, services);
        require(actions.field314 == 1 && actions.field318 == 1, "Reaction countdown advances one original simulation tick.");
        update.advanceReaction(actions, input, services);
        require(actions.field310 == 1 && actions.field314 == 0, "Countdown completion returns to normal state on its exact boundary.");
        actions.beginReaction(1, playeractions::ReactionPayload{});
        actions.entries[0x37] = actions.parameters[2];
        update.run(actions, input, services);
        require(actions.field318 == 3 && durationRequests.back() == 0x37,
            "Missing reaction animation requests the Luigi-table duration using the selected action index.");
        actions = playeractions::Actions(executable, 0);
        actions.parameters[2] = 999;
        for (unsigned i = 0; i < actions.entries.size(); ++i) actions.entries[i] = 1000 + i;
        input = {};
        input.maximumSpeed = 1;
        input.useMotion = true;
        input.flags[0] = 0;
        for (unsigned i = 0; i < 14; ++i) update.run(actions, input, services);
        require(actions.field60 == 0x35 && actions.field308 == 13, "Airborne selection changes after thirteen retained ticks.");
        for (unsigned i = 0; i < 3; ++i) update.run(actions, input, services);
        input.flags[0] = 1;
        update.run(actions, input, services);
        require(actions.field314 == 1 && actions.field308 == 17, "Landing reaction starts at the original seventeen-tick threshold.");
    }

    void checkMarkers() {
        actormodel::Resource timing;
        auto& bytes = timing.bytes;
        bytes.resize(116);
        const auto half = [&](unsigned offset, unsigned value) { bytes[offset] = static_cast<std::uint8_t>(value >> 8); bytes[offset + 1] = static_cast<std::uint8_t>(value); };
        const auto word = [&](unsigned offset, unsigned value) { half(offset, value >> 16); half(offset + 2, value); };
        half(0, 1); word(4, 80);
        const char name[] = "Lfoot_timing";
        std::memcpy(bytes.data() + 80, name, sizeof(name));
        half(110, 3); half(112, 0); half(114, 3);
        const float keys[]{0, 0, 0, 10, 1, 0, 20, 0, 0};
        for (unsigned i = 0; i < 9; ++i) {
            unsigned bits;
            std::memcpy(&bits, &keys[i], 4);
            word(8 + i * 4, bits);
        }
        require(actoranimation::timingCrossing(nullptr, name, 0, 1) == -1 &&
            actoranimation::timingCrossing(&timing, "missing", 0, 1) == -1, "Absent timing markers retain their original sentinel.");
        require(actoranimation::timingCrossing(&timing, name, 9, 10) == 0 &&
            actoranimation::timingCrossing(&timing, name, 10, 11) == 1,
            "Marker intervals include their beginning and exclude their ending key.");
        require(actoranimation::timingCrossing(&timing, name, 15, 5) == 0 &&
            actoranimation::timingCrossing(&timing, name, 11, 12) == 0,
            "Marker scanning preserves the original wrap behavior and avoids duplicate crossings.");
        word(12, 0x3f800000);
        word(24, 0);
        require(actoranimation::timingCrossing(&timing, name, 0, 1) == 10001,
            "A peak in the first key preserves the original missing-previous-key sentinel contribution.");
    }
}

int main(int argc, char** argv) {
    try {
        checkMarkers();
        check(fixture());
        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> executable;
            std::string error;
            require(package.open(argv[1], error) && package.read("sys/main.dol", executable, error), error.c_str());
            check(executable);
        }
        std::cout << "Original player reaction and normal-state transition checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
