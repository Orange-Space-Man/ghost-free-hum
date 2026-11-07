#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace gameevent {
    class EntranceServices {
    public:
        virtual ~EntranceServices() = default;
        virtual bool flag(unsigned index) const = 0;
        virtual void menu(bool enabled) = 0;
        virtual void stopPlayer() = 0;
        virtual void releasePlayer() = 0;
        virtual void music(unsigned index) = 0;
        virtual void camera(std::string_view resource) = 0;
        virtual void lighting(std::string_view resource) = 0;
        virtual void shadowOff(std::string_view actor) = 0;
        virtual void endCamera() = 0;
        virtual void closeMessage() = 0;
        virtual bool messageClosed() const = 0;
        virtual void finish() = 0;
    };

    class EntranceScript {
    public:
        explicit EntranceScript(std::string source);
        bool update(EntranceServices& services);
        bool finished() const;

    private:
        std::string source;
        std::size_t cursor = 0;
        std::uint32_t wait = 0;
        unsigned state = 1;
        bool completed = false;
        std::string_view token(char begin, char end);
        unsigned number();
    };
}
