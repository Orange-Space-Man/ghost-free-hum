#include "game/audio/gameplayroomaudio.h"
#include "game/audio/gameplaymusicrouting.h"

void gameplaymusic::RoomAudio::enter(unsigned packedRoom, unsigned mapMode, bool roomVariant, const RoomServices& services) {
    const unsigned nextRoom = packedRoom & 255;
    if (previousRoom != nextRoom && mapMode == 2) {
        const auto resume = [&] {
            const auto primary = services.primarySound();
            if (primary != mansionSequence && primary != 0x80000034) services.resumeMansion();
        };
        const auto stop = [&] { if (services.auxiliarySound()) services.stopAuxiliary(); };
        switch (previousRoom) {
        case 0x29: resume(); stop(); break;
        case 0x18: stop(); resume(); break;
        case 0x15: case 0x46: resume(); break;
        case 0x39: services.categoryFourPort(0x7f); break;
        }
        switch (nextRoom) {
        case 2: case 0x1a: case 0x1e: case 0x23: case 0x28:
            break;
        case 10: case 0x15: case 0x18:
            if (!roomVariant) services.specialRoom(nextRoom);
            break;
        case 0x39:
            services.categoryFourPort(0x50);
            break;
        case 0x46:
            services.specialRoom(nextRoom);
            break;
        default:
            resume();
            if (services.auxiliarySound() != 0x8000004e) stop();
            break;
        }
    }
    previousRoom = nextRoom;
    services.updateRoomVolumes(nextRoom);
}

unsigned gameplaymusic::RoomAudio::room() const { return previousRoom; }
