#include "game/events/jmptooldata.h"
#include "game/events/jmpmessagesender.h"
#include "game/events/jmpeventstate.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"

#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
    void word(std::vector<std::uint8_t>& data, unsigned offset, unsigned value) {
        for (unsigned i = 0; i < 4; ++i) data[offset + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }
    unsigned hash(const char* name) {
        unsigned result = 0;
        while (*name) result = (result * 256 + static_cast<unsigned char>(*name++)) % 33554393;
        return result;
    }
    struct Receiver : jmp::MessageReceiver {
        std::function<bool(jmp::ToolDataRef&)> onCreate;
        std::string command;
        bool create(jmp::ToolDataRef& entry) override { return onCreate ? onCreate(entry) : false; }
        bool message(jmp::ToolDataRef&, const char* value) override { command = value; return true; }
    };
}

int main(int argc, char** argv) {
    try {
        std::vector<std::uint8_t> data(64 + 3 * 16);
        word(data, 0, 3); word(data, 4, 4); word(data, 8, 64); word(data, 12, 16);
        const char* names[]{"bits", "name", "value", "bits"};
        const unsigned offsets[]{0, 4, 12, 12};
        for (unsigned i = 0; i < 4; ++i) {
            word(data, 16 + i * 12, hash(names[i]));
            word(data, 20 + i * 12, i == 0 ? 0xf0 : 0xffffffff);
            word(data, 24 + i * 12, (offsets[i] << 16) | (i == 0 ? 4 << 8 : 0));
        }
        for (unsigned row = 0; row < 3; ++row) {
            word(data, 64 + row * 16, 0x800000b0);
            std::memcpy(data.data() + 68 + row * 16, row == 1 ? "boo" : "luige", row == 1 ? 4 : 6);
            word(data, 76 + row * 16, 0x3fc00000);
        }
        jmp::ToolData table;
        std::string error;
        require(table.attach(data, error), error.c_str());
        require(table.searchItemInfo("bits") == 0, "Hash matches preserve first field, including duplicates.");
        auto extendedName = data;
        word(extendedName, 16, 4991);
        jmp::ToolData extended;
        require(extended.attach(extendedName, error), error.c_str());
        require(extended.searchItemInfo(std::string(1, static_cast<char>(255))) == 0,
            "Original PowerPC hash sign-extends non-ASCII bytes before unsigned remainder.");
        std::uint32_t packed = 0;
        require(table.getValue(0, "bits", packed) && packed == 11, "Unsigned accessor masks and shifts.");
        std::int32_t raw = 0;
        require(table.getValue(0, "bits", raw) && raw == -2147483472, "Signed accessor preserves the raw word, ignoring mask and shift.");
        std::uint8_t flag = 9;
        require(table.getValue(0, "bits", flag) && flag == 1, "Byte accessor converts masked bits to a boolean.");
        float scalar = 0;
        require(table.getValue(0, "value", scalar) && scalar == 1.5f, "Float accessor preserves IEEE bits.");
        std::string_view name;
        require(table.getValue(0, "name", name) && name == "luige", "String accessor references attached storage.");
        require(!table.getValue(0, "absent", packed) && packed == 11, "Missing field leaves caller default unchanged.");
        require(table.findEntryByValue(0, 11U, 1) == 1, "Numeric search includes its start row.");
        require(table.findEntryByValue(1, "luige", 1) == 2 && table.findEntryByValue(1, "luige", 3) == -1,
            "String search skips earlier rows and preserves table order.");
        const auto retained = table;
        jmp::ToolData independent;
        require(independent.attach(data, error), error.c_str());
        require(jmp::sameEntry({&table, 1}, {&retained, 1}), "Copied wrappers share original entry identity.");
        require(!jmp::sameEntry({&table, 1}, {&independent, 1}) &&
            !jmp::sameEntry({&table, 1}, {&retained, 2}), "Identity requires the same backing resource and row, not equal bytes.");
        jmp::TableList tables;
        require(!tables.add(nullptr) && tables.find(&table) == -1, "Null tables are rejected.");
        require(tables.add(&table) && tables.add(&retained) && tables.add(&table), "Table registration retains duplicates.");
        require(tables.find(&retained) == 1 && tables.find(&table) == 0, "Table lookup compares wrapper addresses in order.");
        require(tables.erase(0) == 0 && tables.size() == 2 && tables.at(0) == &retained && tables.at(1) == &table,
            "Table removal shifts subsequent registrations forward.");
        while (tables.size() < 10) require(tables.add(&retained), "Original table capacity.");
        require(!tables.add(&table) && tables.size() == 10, "Eleventh table is rejected.");
        jmp::ActiveEntries active;
        require(active.find({&table, 1}) == -1, "Empty active collection.");
        require(active.append({1, {&retained, 1}, 9}) && active.append({7, {&independent, 2}, 17}) &&
            active.append({3, {&retained, 1}, 25}), "Active records retain duplicates and all fields.");
        require(active.find({&table, 1}) == 0, "Active lookup uses shared resource identity and first match.");
        require(active.erase(0) == 0 && active.size() == 2 && active.at(0).state == 7 &&
            active.at(0).source.table == &independent && active.at(0).source.row == 2 && active.at(0).roomId == 17 &&
            active.at(1).state == 3 && active.at(1).roomId == 25 && active.find({&table, 1}) == 1,
            "Active removal shifts complete records in original order.");
        while (active.size() < 200) require(active.append({1, {&retained, 0}, 4}), "Original active-entry capacity.");
        require(!active.append({8, {&table, 2}, 1}) && active.size() == 200, "Activation cannot exceed 200 entries.");
        while (active.size()) active.erase(active.size() - 1);
        bool rejected = false;
        try { active.erase(0); } catch (const std::out_of_range&) { rejected = true; }
        require(rejected && active.size() == 0, "Invalid native removal cannot underflow the active count.");
        require(!table.attach({}, error) && table.count() == 3, "Null-equivalent attach preserves the current table.");
        auto damaged = data;
        damaged.pop_back();
        require(!table.attach(damaged, error) && table.count() == 3, "Truncated attachment fails without replacing valid data.");
        table.reset();
        require(table.count() == 0 && retained.count() == 3 && name == "luige", "Shared backing storage outlives a reset of another view.");

        jmp::MessageSender sender;
        jmp::ToolDataRef entry{&retained, 0};
        Receiver first, second, third;
        std::vector<int> calls;
        first.onCreate = [&](jmp::ToolDataRef& ref) { calls.push_back(1); ++ref.row; return false; };
        second.onCreate = [&](jmp::ToolDataRef& ref) { calls.push_back(2); require(ref.row == 1, "Dispatch passes the same entry reference."); return true; };
        third.onCreate = [&](jmp::ToolDataRef&) { calls.push_back(3); return false; };
        require(!sender.dispatch(&jmp::MessageReceiver::create, entry), "Empty sender does not handle messages.");
        require(sender.addReceiver(&first) && sender.addReceiver(&second) && sender.addReceiver(&third), "Receiver registration.");
        require(sender.dispatch(&jmp::MessageReceiver::create, entry) && calls == std::vector<int>{1, 2},
            "First handled message stops dispatch in original registration order.");
        require(!sender.dispatch(&jmp::MessageReceiver::remove, entry), "Original base removal handler returns false.");
        require(sender.dispatch(&jmp::MessageReceiver::message, entry, "start") && first.command == "start" && second.command.empty(),
            "Command dispatch preserves payload and stops at first handler.");
        jmp::MessageSender full;
        for (unsigned i = 0; i < 12; ++i) require(full.addReceiver(&third), "Duplicate receivers are retained up to original capacity.");
        require(!full.addReceiver(&first), "Thirteenth receiver is rejected.");
        calls.clear();
        require(!full.dispatch(&jmp::MessageReceiver::create, entry) && calls.size() == 12, "Duplicate registrations each receive the message.");
        jmp::MessageSender growing;
        first.onCreate = [&](jmp::ToolDataRef&) { calls.push_back(1); growing.addReceiver(&third); return false; };
        growing.addReceiver(&first);
        calls.clear();
        require(!growing.dispatch(&jmp::MessageReceiver::create, entry) && calls == std::vector<int>{1},
            "Receiver count is captured before dispatch; appended receivers wait until the next message.");
        calls.clear();
        require(!growing.dispatch(&jmp::MessageReceiver::create, entry) && calls == std::vector<int>{1, 3},
            "Next dispatch includes receivers added by the previous message.");

        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> compressed, archive, characterInfo;
            require(package.open(argv[1], error) && package.read("files/Map/map2.szp", compressed, error) &&
                roomarchive::decompress(compressed, archive, error) && roomarchive::read(archive, "jmp/characterinfo", characterInfo, error), error.c_str());
            require(table.attach(characterInfo, error), error.c_str());
            int luigi = -1;
            unsigned start = 0;
            while (start < table.count()) {
                luigi = table.findEntryByValue(table.searchItemInfo("name"), "luige", start);
                if (luigi < 0) break;
                packed = 0;
                table.getValue(luigi, "appear_point", packed);
                if (!packed) break;
                start = static_cast<unsigned>(luigi) + 1;
            }
            require(luigi == 38, "Owned CharacterInfo first Luigi entry at appear point zero.");
            require(table.getValue(luigi, "dir_y", scalar) && scalar == 180, "Owned Luigi direction.");
            std::cout << "Original ToolData access verified against owned CharacterInfo.\n";
        }
        return 0;
    } catch (const std::exception& exception) { std::cerr << exception.what() << '\n'; return 1; }
}
