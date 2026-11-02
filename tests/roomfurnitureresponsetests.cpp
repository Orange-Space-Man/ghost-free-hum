#include "../recovered/roomfurnitureresponse.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"

#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "The imported package is required.");
        gameassets::Package package;
        std::string error;
        std::vector<std::uint8_t> executable, compressed, bytes;
        require(package.open(argv[1], error) && package.read("sys/main.dol", executable, error), error.c_str());
        require(package.read("files/Map/map2.szp", compressed, error) && roomarchive::decompress(compressed, bytes, error), error.c_str());
        mission::Resources mission;
        enemytypes::Catalog types;
        require(mission.read(bytes, error) && types.read(executable, error), error.c_str());
        const characteritems::Admission admission(executable, types);
        const roomfurniture::Response response(executable, admission);
        const dooranimation::PoseTables trig = [&] {
            dooranimation::PoseTables result;
            require(result.read(executable, error), error.c_str());
            return result;
        }();
        roomfurniture::Object object;
        object.row = 99;
        object.room = 0x02000102;
        object.position = {570, 0, -80};
        object.moveLevel = 3;
        roomfurniture::RowState row;
        auto selected = characteritems::find(mission, "nothing");
        unsigned selections = 0, creations = 0, releases = 0, saves = 0, animations = 0;
        unsigned result = 7;
        bool cleared = false;
        std::vector<unsigned> effects, sounds, scatter;
        std::string order;
        roomfurniture::ResponseServices services;
        services.roomCleared = [&](unsigned room) { require(room == 2, "Clear-state query must use the room index."); return cleared; };
        services.selectItem = [&](unsigned) { ++selections; order += 'q'; return selected; };
        services.createItem = [&](const auto&, const auto&) { ++creations; order += 'c'; return result; };
        services.createType = [&](unsigned type, const auto&) { scatter.push_back(type); };
        services.scatter = [&](bool bills, float speed, const auto& point, const auto& direction, unsigned count) {
            require(speed > 0 && point[0] == 570 && point[2] == -80 && direction[1] != 0 && count == 15,
                "Scatter must retain original launch parameters and truncate its count to one byte.");
            scatter.push_back(bills ? 2 : 1);
        };
        services.effect = [&](unsigned id, const auto&) { effects.push_back(id); order += 'e'; };
        services.sound = [&](unsigned id, const auto& point, const float* amplitude) {
            require(point == object.position, "Furniture audio must use the furniture position.");
            if (amplitude) require(*amplitude == 6, "Suction audio must retain strength times movement level.");
            sounds.push_back(id);
            order += 's';
        };
        services.releaseOccupant = [&](unsigned index, const auto&) { require(index == 99, "Release must use placement row."); ++releases; order += 'r'; };
        services.persistCollected = [&] { require(row.flags & 8, "Collection flag must precede persistence."); ++saves; order += 'p'; };
        services.animation = [&](auto& target, unsigned animation) { target.animation = static_cast<std::uint8_t>(animation); target.frame = 0; ++animations; order += 'a'; };

        row.flags = 1;
        for (unsigned tick = 0; tick < 19; ++tick) {
            object.updates = tick;
            require(response.strike(object, row, true, false, services) == 1 && selections == 0,
                "Strike must not collect before update twenty.");
        }
        require(response.strike(object, row, true, false, services) == 2 && !(row.flags & 1) && selections == 1 &&
            object.collectionFlags == 1 && effects == std::vector<unsigned>{0x61} && sounds == std::vector<unsigned>{0x1877} &&
            animations == 1 && order == "qesa", "Strike completion must collect before selecting the reaction animation.");
        require(response.strike(object, row, true, false, services) == 0 && object.strikeTicks == 0,
            "Inactive strike must reset its retained timer.");

        object.collectionFlags = 0;
        object.animation = 0;
        row.flags = 1;
        row.reactionTwo = true;
        sounds.clear();
        for (unsigned tick = 0; tick < 20; ++tick) {
            object.updates = tick + 2;
            response.strike(object, row, false, true, services);
            require(object.angles[2] == static_cast<int>(300.0f * trig.sineAngle(object.updates * 2000)),
                "Second reaction must use the original sine-table phase.");
        }
        require(sounds.size() == 2 && selections == 1 && object.collectionFlags == 0,
            "Suppressed strike collection still plays sounds at timer zero and sixteen.");
        sounds.clear();
        for (unsigned tick = 0; tick < 30; ++tick) {
            object.updates = tick;
            response.suction(object, 2, true, tick, services);
        }
        require(object.suctionTicks == 30 && object.animation == 1 && animations == 2 && sounds.size() == 8,
            "Suction must select animation on update thirty and sound on every fourth global update.");
        object.angles[2] = -600;
        response.suction(object, 0, true, 32, services);
        require(object.angles[2] == -344 && object.suctionTicks == 0, "Stopped suction must approach zero by the original short-angle step.");

        selected = characteritems::find(mission, "money");
        require(admission.money(selected), "Original money record must be available.");
        object.generate = 3;
        object.generateCount = 271;
        object.collectionFlags = 0;
        row.flags = 0;
        effects.clear();
        response.collect(object, row, true, services);
        require(scatter == std::vector<unsigned>({1, 2}) && effects == std::vector<unsigned>({0x54, 0x55}) &&
            (row.flags & 8) && object.collectionFlags == 1 && saves == 1, "Mixed loot must scatter coins before bills and persist once.");
        const auto previousSelections = selections;
        response.collect(object, row, true, services);
        require(selections == previousSelections && scatter.size() == 2 && effects.back() == 0x61,
            "Repeated ordinary collection must not select or duplicate loot.");
        object.collectionFlags = 0;
        response.collect(object, row, true, services);
        require(selections == previousSelections + 1 && scatter.size() == 2 && saves == 1,
            "Persisted money flag must prevent re-created furniture from duplicating money.");

        const auto* info = mission.getJmpResource("iteminfotable");
        bool checkedRepeatable = false;
        for (unsigned index = 0; index < info->count(); ++index) {
            const jmp::ToolDataRef item{info, static_cast<int>(index)};
            if (admission.nothing(item) || admission.money(item) || !admission.redirected(item)) continue;
            selected = item;
            object.collectionFlags = 0;
            response.collect(object, row, true, services);
            require(object.collectionFlags == 2 && creations == 1, "Redirected item must mark the visibility-local attempt without consuming ordinary loot.");
            response.collect(object, row, true, services);
            require(creations == 1 && effects.back() == 0x61, "Repeated redirected attempt must not create a duplicate.");
            object.collectionFlags &= ~2u;
            result = 0xffffffff;
            response.collect(object, row, true, services);
            require(creations == 2 && object.collectionFlags == 2 && effects.back() == 0x61,
                "Failed redirected creation must still mark its attempt.");
            checkedRepeatable = true;
            break;
        }
        require(checkedRepeatable, "Original map must provide a redirected item fixture.");
        row.occupant = 3;
        row.flags = 6;
        cleared = true;
        response.occupant(object, row, true, services);
        require(releases == 0 && row.flags == 6, "Covered occupant must remain owned despite a forced strike.");
        row.flags = 4;
        response.occupant(object, row, false, services);
        require(releases == 1 && !(row.flags & 4), "Uncovered occupant notification must consume its release flag.");
        object.suctionTicks = 30;
        response.occupant(object, row, false, services);
        require(releases == 1, "Suction occupant release uses a strict greater-than-thirty boundary.");
        object.suctionTicks = 31;
        response.occupant(object, row, false, services);
        require(releases == 2, "Suction must release the occupant after update thirty.");
        const auto beforeBlocked = selections;
        response.collect(object, row, true, services);
        require(selections == beforeBlocked, "Cleared-room occupied furniture must not select loot.");
        cleared = false;
        row.flags = 4;
        response.occupant(object, row, true, services);
        require(releases == 2 && row.flags == 0, "Uncleared room must clear its request without releasing the occupant.");
        row.occupant = -1;
        row.reactionTwo = true;
        row.flags = 1;
        object.strikeTicks = 0;
        object.collectionFlags = 3;
        object.behavior = 0;
        object.updates = 5;
        object.counter = 0x123;
        object.suctionTicks = 29;
        unsigned messages = 0;
        services.scanMessage = [&](unsigned message) {
            require(message == 0x23 && (row.flags & 0x20), "Scan callback must see its pending row and truncated counter.");
            ++messages;
        };
        response.basic(object, row, false, false, 0, 1, services);
        require(object.collectionFlags == 1 && object.strikeTicks == 0 && object.suctionTicks == 29,
            "Invisible basic furniture must only reset the repeat-attempt flag.");
        row.flags |= 0x20;
        response.basic(object, row, true, false, 0, 1, services);
        const auto strikeAngle = static_cast<int>(300.0f * trig.sineAngle(object.updates * 2000));
        const auto expected = strikeAngle > 256 ? strikeAngle - 256 : strikeAngle < -256 ? strikeAngle + 256 : 0;
        require(object.angles[2] == expected && object.suctionTicks == 0 && object.strikeTicks == 1 &&
            messages == 1 && !(row.flags & 0x20), "Basic handler must relax after its strike and consume a scanned message.");
        object.behavior = 15;
        object.strikeTicks = 0;
        object.frame = 5;
        response.basic(object, row, true, false, 0, 1, services);
        require(object.angles[2] == strikeAngle && object.frame == 6,
            "Animated handler must strike after relaxation and advance its animation frame.");
        object.behavior = 22;
        object.collectionFlags = 3;
        response.basic(object, row, false, false, 0, 1, services);
        require(object.collectionFlags == 3, "Mirror handler must preserve repeat flags while invisible.");
        row.flags = 0x20;
        response.basic(object, row, true, false, 0, 1, services);
        require(messages == 1 && (row.flags & 0x20), "Mirror handler must not consume the basic handler's scan-message request.");
        std::cout << "Furniture strike, suction, loot persistence, repeatable attempts and occupant release verified.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
