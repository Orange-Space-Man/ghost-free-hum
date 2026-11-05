#pragma once

#include <atomic>
#include <cstdint>
#include <functional>

namespace jaudio {
    class InterfaceClock {
    public:
        void mixed();
        bool update(const std::function<void()>& work);
        std::uint32_t frame() const;
        void reset();

    private:
        std::atomic<std::uint32_t> dacFrame{0};
        std::uint32_t previousDacFrame = 0, interfaceFrame = 0;
    };
}
