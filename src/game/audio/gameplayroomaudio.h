#pragma once

#include <cstdint>
#include <functional>

namespace gameplaymusic {
    struct RoomServices {
        std::function<std::uint32_t()> primarySound;
        std::function<std::uint32_t()> auxiliarySound;
        std::function<void()> resumeMansion;
        std::function<void()> stopAuxiliary;
        std::function<void(unsigned)> specialRoom;
        std::function<void(unsigned)> categoryFourPort;
        std::function<void(unsigned)> updateRoomVolumes;
    };

    class RoomAudio {
    public:
        void enter(unsigned packedRoom, unsigned mapMode, bool roomVariant, const RoomServices& services);
        unsigned room() const;

    private:
        unsigned previousRoom = 0;
    };
}
