#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace gameevent {
    class MessageWindows {
    public:
        enum class Phase { Hidden, Opening, Revealing, PageWait, Complete, Scrolling, Closing, Replacing };
        struct Window {
            Phase phase = Phase::Hidden;
            std::string text;
            int color = 0, pendingColor = -1;
            std::int16_t first = 0, revealed = 0, scroll = 0, timer = 0, width = 0, height = 0;
            int x = 28, y = 21;
        };
        struct Services {
            std::function<std::array<float, 2>(const std::string&)> measure;
            std::function<void(unsigned)> sound;
            std::function<void(bool)> dialogueAudio;
        };
        MessageWindows(const std::vector<std::uint8_t>& executable, Services services);
        void speak(unsigned index, std::string text);
        void color(unsigned index, int value);
        void close(unsigned index);
        void closeAll();
        void advance(unsigned index);
        bool acknowledge();
        void update(int eventMode);
        const Window& window(unsigned index) const;
        bool speaking() const;
        bool visible() const;
        unsigned activeWindow() const;

    private:
        static std::int16_t nextCharacter(const std::string& text, std::int16_t cursor);
        static void applyColor(Window& window);
        void measure(Window& window);
        Services services;
        std::array<Window, 4> windows;
        std::int16_t openingFrames = 0, closingFrames = 0, scrollStep = 0, widthPadding = 0;
        float scale = 0, lineHeight = 0;
        unsigned active = 0;
        bool talking = false, audioActive = false;
    };
}
