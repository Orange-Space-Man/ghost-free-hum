#pragma once

#include "game/menu/controllerinstructions.h"

namespace controllerinstructions {
    class Services {
    public:
        virtual ~Services() = default;
        virtual unsigned option() const = 0;
        virtual void setOption(unsigned value) = 0;
        virtual void playerScheme(unsigned scheme) = 0;
        virtual void sound(unsigned id) = 0;
        virtual void saveOptions() = 0;
    };

    class Owner {
    public:
        bool open(const std::filesystem::path& package, std::string& error);
        void update(std::uint32_t pressed, std::uint64_t presentationFrame, Services& services);
        bool active() const;
        const Screen& screen() const;
    private:
        Screen contents;
        bool opened = false;
        bool closing = false;
    };
}
