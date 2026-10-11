#include "game/events/jmpentryactivation.h"
#include "game/world/roomdependencies.h"
#include "game/world/roomlocation.h"
#include "resources/gameassetpackage.h"
#include "game/events/missionresources.h"
#include "resources/roomarchivereader.h"

#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

    void word(std::vector<std::uint8_t>& bytes, unsigned offset, unsigned value) {
        for (unsigned i = 0; i < 4; ++i) bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }

    jmp::ToolData table(unsigned count, unsigned stay, unsigned appear, unsigned disappear, unsigned event) {
        const char* fields[]{"pos_x", "pos_y", "pos_z", "stay", "appear_flag", "disappear_flag", "event_set_no"};
        std::vector<std::uint8_t> bytes(100 + count * 28);
        word(bytes, 0, count);
        word(bytes, 4, 7);
        word(bytes, 8, 100);
        word(bytes, 12, 28);
        for (unsigned i = 0; i < 7; ++i) {
            unsigned hash = 0;
            for (const char* ch = fields[i]; *ch; ++ch) hash = (hash * 256 + *ch) % 33554393;
            word(bytes, 16 + i * 12, hash);
            word(bytes, 20 + i * 12, 0xffffffff);
            word(bytes, 24 + i * 12, (i * 4) << 16);
        }
        for (unsigned row = 0; row < count; ++row) {
            word(bytes, 112 + row * 28, stay);
            word(bytes, 116 + row * 28, appear);
            word(bytes, 120 + row * 28, disappear);
            word(bytes, 124 + row * 28, event);
        }
        jmp::ToolData result;
        std::string error;
        require(result.attach(bytes, error), error.c_str());
        return result;
    }

    struct Runtime : jmp::ActivationRuntime {
        bool visible = true;
        unsigned resolveRoom(const std::array<float, 3>&) override { return 0x02000102; }
        bool roomVisible(unsigned room) override {
            require(room == 2, "Visibility receives the resolved room's low byte.");
            return visible;
        }
    };

    struct Receiver : jmp::MessageReceiver {
        std::function<void(jmp::ToolDataRef&, bool)> callback;
        bool create(jmp::ToolDataRef& source) override { callback(source, true); return false; }
        bool remove(jmp::ToolDataRef& source) override { callback(source, false); return false; }
    };
}

int main(int argc, char** argv) {
    try {
        Runtime runtime;
        gameflags::State flags;
        gameevent::ActivationState events;
        jmp::MessageSender sender;
        jmp::EntryActivation activation(sender, runtime, flags, events);
        Receiver receiver;
        sender.addReceiver(&receiver);
        unsigned creates = 0, removes = 0;
        receiver.callback = [&](jmp::ToolDataRef& source, bool create) {
            require(activation.entries().find(source) >= 0, "Entry exists during both creation and removal dispatch.");
            if (create) ++creates;
            else ++removes;
        };
        auto ordinary = table(1, 0, 0, 0, 0);
        auto alias = ordinary;
        activation.add(&ordinary);
        activation.add(&alias);
        activation.update();
        require(creates == 1 && activation.entries().size() == 1, "Shared-resource registrations activate once even when dispatch returns false.");
        require(activation.entries().at(0).roomId == 0x02000102, "Activation retains the complete packed room ID.");
        runtime.visible = false;
        activation.update();
        require(removes == 1 && activation.entries().size() == 0, "Invisible entry is removed despite an unhandled callback.");
        auto persistent = table(1, 0x80000000, 0, 0, 0);
        activation.scan(&persistent, true);
        require(activation.entries().size() == 1, "Stay uses any masked nonzero bit, independently of room visibility.");
        activation.scan(&persistent, false, true);
        auto conditional = table(1, 0, 0x101, 2, 7);
        runtime.visible = true;
        activation.scan(&conditional, true);
        require(activation.entries().size() == 0, "Appearance gate rejects inactive entries.");
        flags.setOn(1);
        activation.scan(&conditional, true);
        require(activation.entries().size() == 1, "Nonzero flag identifiers narrow to their original byte.");
        flags.setOn(2);
        activation.scan(&conditional, false);
        require(activation.entries().size() == 0, "Disappearance overrides the appearance flag.");
        gameevent::ConditionState state;
        state.eventNumber = 7;
        events.configure(state, {}, 0, flags);
        activation.scan(&conditional, true);
        require(activation.entries().size() == 1, "Active event overrides both flag gates.");
        runtime.visible = false;
        activation.scan(&conditional, false);
        require(activation.entries().size() == 0, "Active events do not override room visibility.");
        activation.scan(&conditional, true, true);
        require(activation.entries().size() == 1, "Forced activation bypasses eligibility.");
        activation.scan(&conditional, false, true);
        auto many = table(201, 1, 0, 0, 0);
        activation.scan(&many, true);
        require(activation.entries().size() == 200, "Activation enforces the original shared 200-entry limit.");
        activation.scan(&many, false, true);
        require(activation.entries().size() == 0, "Removal preserves iteration while shifting records.");
        require(activation.remove(&ordinary) && !activation.remove(&ordinary), "Removal unregisters the first exact wrapper address.");
        runtime.visible = true;
        activation.update();
        require(activation.entries().size() == 1, "A separately registered shared-resource alias remains registered.");
        require(activation.remove(&alias) && activation.entries().size() == 0, "Unregistration forces active removal before table release.");

        auto entering = table(1, 1, 3, 0, 0);
        auto leaving = table(1, 1, 0, 3, 0);
        jmp::MessageSender orderedSender;
        jmp::EntryActivation ordered(orderedSender, runtime, flags, events);
        Receiver orderedReceiver;
        std::vector<bool> order;
        orderedReceiver.callback = [&](jmp::ToolDataRef&, bool create) { order.push_back(create); };
        orderedSender.addReceiver(&orderedReceiver);
        ordered.add(&entering);
        ordered.add(&leaving);
        ordered.update();
        order.clear();
        flags.setOn(3);
        ordered.update();
        require(order == std::vector<bool>{false, true}, "Every table's removals precede any table's creations.");

        std::vector<std::uint8_t> executable(0x200);
        word(executable, 0, 0x100);
        word(executable, 0x48, 0x802ff218);
        word(executable, 0x90, 0x100);
        word(executable, 0x100, 0x802ff228);
        executable[0x110] = 2;
        word(executable, 0x118, 0x802ff248);
        word(executable, 0x130, 0x802ff258);
        word(executable, 0x134, 0x802ff260);
        word(executable, 0x140, 0x00010001);
        word(executable, 0x144, 0xfffeffff);
        word(executable, 0x148, 0xffff0000);
        roomdependencies::Catalog synthetic(executable, 0);
        require(*synthetic.list(0x12340000) == std::vector<std::int16_t>{1, 1, -2} && synthetic.list(1)->empty(),
            "Dependency lists preserve original order, duplicates, signed halfwords and the first sentinel.");
        require(synthetic.contains(0, 0xfffffffe) && !synthetic.contains(0, 65534), "Dependency comparison sign-extends original halfwords.");
        word(executable, 0x134, 0);
        bool rejected = false;
        try { roomdependencies::Catalog invalid(executable, 0); } catch (const std::exception&) { rejected = true; }
        require(rejected, "Invalid dependency pointers fail at executable bounds.");

        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> executable, compressed, archive;
            std::string error;
            require(package.open(argv[1], error) && package.read("sys/main.dol", executable, error) &&
                package.read("files/Map/map2.szp", compressed, error) && roomarchive::decompress(compressed, archive, error), error.c_str());
            roomdependencies::Catalog dependencies(executable, 2);
            require(*dependencies.list(0x02000102) == std::vector<std::int16_t>{2, 30, 6}, "Imported first-room dependency order.");
            require(!dependencies.list(0xffffffff) && dependencies.contains(2, 30) && !dependencies.contains(2, 18),
                "Room sentinel and original dependency membership.");
            mission::Resources mission;
            require(mission.read(archive, error), error.c_str());
            struct ImportedRuntime : jmp::ActivationRuntime {
                roomlocation::Rooms rooms;
                roomdependencies::Catalog dependencies;
                ImportedRuntime(const std::vector<std::uint8_t>& executable) : rooms(executable, 2), dependencies(executable, 2) {}
                unsigned resolveRoom(const std::array<float, 3>& position) override { return rooms.resolve(position); }
                bool roomVisible(unsigned room) override { return dependencies.contains(2, room); }
            } importedRuntime(executable);
            gameflags::State importedFlags;
            gameevent::ActivationState importedEvents;
            jmp::MessageSender importedSender;
            jmp::EntryActivation imported(importedSender, importedRuntime, importedFlags, importedEvents);
            imported.add(mission.getJmpResource("characterinfo"));
            imported.update();
            require(imported.entries().size() == 2 && imported.entries().at(0).source.row == 38 &&
                imported.entries().at(1).source.row == 50, "Cleared-state placement scan admits only the two Luigi records.");
            importedFlags.setOn(3);
            imported.update();
            require(imported.entries().size() == 4, "Flag three enables the original two demo placements.");
            importedFlags.setOn(4);
            imported.update();
            require(imported.entries().size() == 2, "Flag four removes both demo placements.");
            importedFlags.setOn(35);
            imported.update();
            require(imported.entries().size() == 3 && imported.entries().at(2).source.row == 29,
                "Butler stay placement remains outside room visibility and requires its original flag.");
        }
        std::cout << "JMP entry activation checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
