#include "resources/glme01addresses.h"
#include "game/events/eventinfodecoder.h"
#include "game/enemies/enemycreationrequests.h"
#include "resources/gameassetpackage.h"
#include "game/events/missionresources.h"
#include "resources/roomarchivereader.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    void word(std::vector<std::uint8_t>& data, unsigned offset, unsigned value) {
        for (unsigned i = 0; i < 4; ++i) data[offset + i] = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }

    unsigned hash(const char* name) {
        unsigned value = 0;
        while (*name) value = (value * 256 + static_cast<unsigned char>(*name++)) % 33554393;
        return value;
    }

    std::vector<std::uint8_t> executable() {
        const unsigned tableSize = 381 * 28;
        std::vector<std::uint8_t> data(256 + tableSize + 10);
        word(data, 0, 256);
        word(data, 0x48, glme01::actorTypeTable);
        word(data, 0x90, tableSize + 10);
        for (unsigned i = 0; i < 381; ++i) {
            word(data, 256 + i * 28 + 24, glme01::actorTypeTable + tableSize);
            word(data, 256 + i * 28 + 8, 0x12340000);
        }
        std::memcpy(data.data() + 256 + tableSize, "luige\0boo\0", 10);
        word(data, 256 + 28 + 24, glme01::actorTypeTable + tableSize + 6);
        return data;
    }

    std::vector<std::uint8_t> eventTable() {
        const char* names[]{"pos_x", "pos_y", "pos_z", "EventNo", "EventIf", "EventArea", "EventTime", "EventTime2",
            "EventTime3", "EventTime4", "EventLoad", "EventFlag", "EventLock", "PlayerStop", "CharacterName",
            "disappear_flag", "event_parameter"};
        const unsigned start = 16 + 17 * 12;
        std::vector<std::uint8_t> data(start + 2 * 80);
        word(data, 0, 2);
        word(data, 4, 17);
        word(data, 8, start);
        word(data, 12, 80);
        for (unsigned i = 0; i < 17; ++i) {
            word(data, 16 + i * 12, hash(names[i]));
            word(data, 20 + i * 12, i == 12 ? 0x80 : 0xffffffff);
            word(data, 24 + i * 12, (i * 4 << 16) | (i == 12 ? 7 << 8 : 0));
        }
        for (unsigned row = 0; row < 2; ++row) {
            const unsigned offset = start + row * 80;
            word(data, offset, 0x3fc00000);
            word(data, offset + 4, 0xc0200000);
            word(data, offset + 8, 0x40e00000);
            word(data, offset + 12, 7 + row);
            word(data, offset + 16, 5);
            word(data, offset + 20, 400);
            word(data, offset + 24, row ? 0xffffffff : 2);
            word(data, offset + 28, row ? 61 : 3);
            word(data, offset + 32, 4);
            word(data, offset + 36, 5);
            word(data, offset + 40, 2);
            word(data, offset + 44, 89);
            word(data, offset + 48, row ? 0x40 : 0x80);
            word(data, offset + 52, 1);
            std::memcpy(data.data() + offset + 56, row ? "???" : "boo", 4);
            word(data, offset + 60, 77);
            word(data, offset + 64, 0xabcdef01);
        }
        return data;
    }
}

int main(int argc, char** argv) {
    try {
        std::string error;
        enemytypes::Catalog types;
        auto dol = executable();
        require(types.read(dol, error), error.c_str());
        require(types.find("luige") == 0 && types.find("boo") == 1 && types.find("Luige") == 0xffffffff &&
            types.find("missing") == 0xffffffff && types.flags(380) == 0x1234,
            "Type lookup retains order, exact case, first duplicate, flags and missing sentinel.");
        require(types.find(std::string_view("boo\0ignored", 11)) == 1, "Type names use original C string termination.");
        dol.pop_back();
        require(!types.read(dol, error) && types.find("boo") == 1, "Malformed catalog replacement preserves loaded data.");
        auto bytes = eventTable();
        jmp::ToolData table;
        require(table.attach(bytes, error), error.c_str());
        std::vector<gameevent::Definition> definitions;
        require(gameevent::readDefinitions(table, types, definitions, error), error.c_str());
        require(definitions.size() == 2 && definitions[0].eventNumber == 7 && definitions[1].eventNumber == 8 &&
            definitions[0].position == std::array<float, 3>{1.5f, -2.5f, 7.0f}, "Event rows retain original order and raw float positions.");
        const auto& first = definitions[0];
        require(first.trigger == 5 && first.area == 400 && first.conditions.startTime == 123 && first.conditions.endTime == 245 &&
            first.conditions.loadLimit == 2 && first.conditions.appearFlag == 89 && first.conditions.disappearFlag == 77 &&
            first.conditions.lock == 1 && first.conditions.playerStop == 1 && first.conditions.characterType == 1 &&
            first.conditions.parameter == 0xabcdef01, "EventInfo fields map into original constructor arguments.");
        require(definitions[1].conditions.startTime == 1 && definitions[1].conditions.lock == 0 &&
            definitions[1].conditions.characterType == 0xffffffff, "Time arithmetic wraps and lock uses the masked boolean accessor.");
        word(bytes, 16 + 4 * 12, hash("unused"));
        require(table.attach(bytes, error) && gameevent::readDefinitions(table, types, definitions, error), error.c_str());
        require(definitions[0].trigger == 0 && definitions[1].trigger == 0, "Absent initialized optional fields retain original defaults.");
        word(bytes, 16 + 14 * 12, hash("absent"));
        require(table.attach(bytes, error), error.c_str());
        require(!gameevent::readDefinitions(table, types, definitions, error) && definitions.size() == 2,
            "Missing uninitialized original input is rejected without replacing decoded records.");

        auto characters = eventTable();
        word(characters, 16 + 14 * 12, hash("name"));
        word(characters, 16 + 16 * 12, hash("create_name"));
        word(characters, 16 + 17 * 12 + 64, 0);
        require(table.attach(characters, error), error.c_str());
        const auto firstRequest = enemycreation::makeRequest({&table, 0}, types);
        const auto missingRequest = enemycreation::makeRequest({&table, 1}, types);
        require(firstRequest.type == 1 && firstRequest.source.table == &table && firstRequest.source.row == 0 &&
            missingRequest.type == 0xffffffff, "Pending records retain source and resolve the original name field.");
        require(enemycreation::createName(firstRequest).has_value() && enemycreation::createName(firstRequest)->empty(),
            "An empty create_name remains distinguishable from a missing field.");
        require(enemycreation::resolveType({nullptr, 0}, types) == 0xffffffff &&
            enemycreation::resolveType({&table, -1}, types) == 0xffffffff &&
            !enemycreation::createName({{&table, -1}, 1}), "Invalid references preserve original missing results.");
        enemycreation::Requests requests;
        requests.append(firstRequest);
        requests.append(missingRequest);
        requests.append(firstRequest);
        require(requests.erase(0) == 0 && requests.size() == 2 && requests.at(0).type == 0xffffffff &&
            requests.at(0).source.row == 1 && requests.at(1).type == 1 && requests.at(1).source.row == 0,
            "Pending removal shifts complete records in insertion order, preserving duplicate requests.");
        while (requests.size() < 128) requests.append(firstRequest);
        bool overflow = false;
        try { requests.append(firstRequest); } catch (const std::out_of_range&) { overflow = true; }
        require(overflow && requests.size() == 128, "Native guard rejects writes beyond original 128-entry storage.");
        while (requests.size()) requests.erase(requests.size() - 1);
        bool underflow = false;
        try { requests.erase(0); } catch (const std::out_of_range&) { underflow = true; }
        require(underflow && requests.size() == 0, "Invalid removal cannot underflow native count.");
        word(characters, 16 + 16 * 12, hash("absent"));
        require(table.attach(characters, error), error.c_str());
        require(!enemycreation::createName(firstRequest), "Create-name lookup reads the referenced table when called, not a cached name.");

        if (argc == 2) {
            gameassets::Package package;
            std::vector<std::uint8_t> original, compressed, archive;
            require(package.open(argv[1], error) && package.read("sys/main.dol", original, error) &&
                package.read("files/Map/map2.szp", compressed, error) && roomarchive::decompress(compressed, archive, error), error.c_str());
            require(types.read(original, error), error.c_str());
            mission::Resources resources;
            require(resources.read(archive, error), error.c_str());
            const auto* events = resources.getJmpResource("EventInfo");
            require(events != nullptr, "Owned mission supplies EventInfo.");
            require(gameevent::readDefinitions(*events, types, definitions, error), error.c_str());
            require(definitions.size() == 95 && definitions[0].eventNumber == 6 && definitions[0].trigger == 5 &&
                definitions[0].area == 1500 && definitions[0].position[1] == -550 &&
                definitions[0].conditions.disappearFlag == 66 && definitions[0].conditions.characterType == 0xffffffff,
                "Owned first event matches independently decoded fields.");
            require(definitions[1].eventNumber == 73 && definitions[1].conditions.lock == 1 && definitions[1].conditions.playerStop == 1 &&
                definitions[3].eventNumber == 16 && definitions[3].conditions.characterType == types.find("dltelesa") &&
                definitions[3].conditions.characterType != 0xffffffff && definitions[6].conditions.appearFlag == 89,
                "Owned later rows retain event order, character lookup and activation flags.");
            require(types.find("luige") == 0 && types.find("yapoo1") == 11, "Owned character catalog identities.");
            const auto* characterInfo = resources.getJmpResource("CharacterInfo");
            require(characterInfo != nullptr, "Owned mission supplies CharacterInfo.");
            const auto luigi = enemycreation::makeRequest({characterInfo, 38}, types);
            require(luigi.type == 0 && luigi.source.table == characterInfo && luigi.source.row == 38,
                "Owned pending-record construction preserves Luigi's original character identity and JMP reference.");
            std::cout << "95 owned EventInfo records decoded through the mission resource owner and original type lookup.\n";
        }
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
