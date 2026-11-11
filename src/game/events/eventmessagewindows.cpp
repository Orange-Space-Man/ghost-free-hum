#include "game/events/eventmessagewindows.h"
#include "resources/dolresources.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

gameevent::MessageWindows::MessageWindows(const std::vector<std::uint8_t>& executable, Services services) :
    services(std::move(services)) {
    if (!this->services.measure || !this->services.sound || !this->services.dialogueAudio)
        throw std::runtime_error("Message windows require original text metrics and audio services.");
    auto read = [&](unsigned address, unsigned count) {
        std::vector<std::uint8_t> bytes;
        std::string error;
        if (!dolresources::read(executable, address, count, bytes, error)) throw std::runtime_error(error);
        return bytes;
    };
    constexpr unsigned windowTiming = 0x804d054c, textScale = 0x804d0518, textLineHeight = 0x804da378;
    const auto timing = read(windowTiming, 12);
    auto word = [&](unsigned offset) { return static_cast<std::int16_t>((unsigned(timing[offset]) << 8) | timing[offset + 1]); };
    widthPadding = word(0);
    openingFrames = word(6);
    closingFrames = word(8);
    scrollStep = word(10);
    auto number = [&](unsigned address) {
        const auto bytes = read(address, 4);
        const std::uint32_t bits = (unsigned(bytes[0]) << 24) | (unsigned(bytes[1]) << 16) |
            (unsigned(bytes[2]) << 8) | bytes[3];
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    };
    scale = number(textScale);
    lineHeight = number(textLineHeight);
    for (unsigned i = 0; i < windows.size(); ++i) windows[i].color = i;
    windows[2].x = 19;
    windows[2].y = 353;
}

void gameevent::MessageWindows::applyColor(Window& window) {
    if (window.pendingColor == -1) return;
    window.color = window.pendingColor;
    window.pendingColor = -1;
}

void gameevent::MessageWindows::measure(Window& window) {
    const auto size = services.measure(window.text);
    window.width = static_cast<std::int16_t>(static_cast<int>(widthPadding + scale * size[0]));
    window.height = static_cast<std::int16_t>(static_cast<int>(scale * size[1]));
}

void gameevent::MessageWindows::speak(unsigned index, std::string text) {
    auto& entry = windows.at(index);
    const auto end = text.find('\0');
    if (end != std::string::npos) text.resize(end);
    if (text.size() >= 512) throw std::runtime_error("Original message-window text exceeds its buffer.");
    while (!text.empty() && static_cast<unsigned char>(text.back()) <= 31) text.pop_back();
    entry.text = std::move(text);
    switch (entry.phase) {
    case Phase::Hidden:
        applyColor(entry);
        entry.phase = Phase::Opening;
        entry.timer = openingFrames;
        break;
    case Phase::Revealing:
    case Phase::PageWait:
    case Phase::Complete:
        entry.phase = Phase::Revealing;
        entry.timer = 0;
        applyColor(entry);
        break;
    case Phase::Closing:
        entry.phase = Phase::Replacing;
        break;
    default:
        entry.phase = Phase::Revealing;
        break;
    }
    entry.first = entry.revealed = entry.scroll = 0;
    active = index;
    if (entry.phase != Phase::Replacing) measure(entry);
}

void gameevent::MessageWindows::color(unsigned index, int value) {
    windows.at(index).pendingColor = value < 0 || value > 20 ? 0 : value;
}

void gameevent::MessageWindows::close(unsigned index) {
    auto& entry = windows.at(index);
    if (entry.phase == Phase::Hidden || entry.phase == Phase::Closing) return;
    entry.phase = Phase::Closing;
    entry.timer = 0;
}

void gameevent::MessageWindows::closeAll() {
    for (unsigned i = 0; i < windows.size(); ++i) close(i);
}

std::int16_t gameevent::MessageWindows::nextCharacter(const std::string& text, std::int16_t cursor) {
    std::size_t position = static_cast<unsigned short>(cursor);
    while (position < text.size()) {
        const auto value = static_cast<unsigned char>(text[position]);
        if (value > 31) {
            position += value < 128 ? 1 : 2;
            if (position > text.size()) throw std::runtime_error("Truncated message-window character.");
            return static_cast<std::int16_t>(position);
        }
        if (value == 27) {
            if (position + 3 > text.size()) throw std::runtime_error("Truncated message-window escape command.");
            if (position + 3 < text.size() && text[position + 3] == '[') {
                const auto end = text.find(']', position + 4);
                if (end == std::string::npos) throw std::runtime_error("Unterminated message-window escape argument.");
                position = end + 1;
            } else position += 3;
        } else {
            ++position;
            if (value == 10) return static_cast<std::int16_t>(position);
        }
    }
    return static_cast<std::int16_t>(position);
}

void gameevent::MessageWindows::advance(unsigned index) {
    auto& entry = windows.at(index);
    if (entry.phase == Phase::PageWait) {
        entry.phase = Phase::Scrolling;
        services.sound(0x380b);
    } else if (entry.phase == Phase::Revealing) {
        unsigned lines = 0;
        for (std::size_t i = entry.first + 1; i < entry.text.size(); ++i) {
            if (entry.text[i] == '\n') ++lines;
            if (lines > 1) {
                entry.revealed = static_cast<std::int16_t>(i + 1);
                entry.phase = Phase::PageWait;
                return;
            }
        }
        entry.revealed = static_cast<std::int16_t>(entry.text.size());
        entry.phase = Phase::Complete;
    }
}

bool gameevent::MessageWindows::acknowledge() {
    for (const auto& entry : windows) {
        if (entry.phase == Phase::Complete) {
            services.sound(0x380b);
            return true;
        }
    }
    return false;
}

void gameevent::MessageWindows::update(int eventMode) {
    for (auto& entry : windows) {
        switch (entry.phase) {
        case Phase::Opening:
            if (--entry.timer < 1) entry.phase = Phase::Revealing;
            break;
        case Phase::Revealing:
            for (unsigned step = 0; step < 3 && entry.phase == Phase::Revealing; ++step) {
                entry.revealed = nextCharacter(entry.text, entry.revealed);
                if (entry.revealed == entry.text.size()) entry.phase = Phase::Complete;
                else if (entry.revealed && entry.text[entry.revealed - 1] == '\n') {
                    const auto lines = std::count(entry.text.begin(), entry.text.begin() + entry.revealed, '\n');
                    if (lines > 1) entry.phase = lines % 2 == 0 && eventMode == 3 ? Phase::PageWait : Phase::Scrolling;
                }
            }
            break;
        case Phase::Scrolling:
            entry.scroll += scrollStep;
            if (entry.scroll >= static_cast<int>(double(lineHeight) * scale)) {
                entry.scroll = 0;
                entry.phase = Phase::Revealing;
                const auto newline = entry.text.find('\n', entry.first);
                entry.first = newline == std::string::npos ? 0 : static_cast<std::int16_t>(newline + 1);
            }
            break;
        case Phase::Closing:
        case Phase::Replacing:
            if (++entry.timer > closingFrames) {
                applyColor(entry);
                if (entry.phase == Phase::Replacing) {
                    entry.phase = Phase::Opening;
                    entry.timer = openingFrames;
                    measure(entry);
                } else entry.phase = Phase::Hidden;
            }
            break;
        default:
            break;
        }
    }
    talking = std::any_of(windows.begin(), windows.end(), [](const Window& entry) { return entry.phase == Phase::Revealing; });
    const bool shown = visible();
    if (audioActive != shown) {
        audioActive = shown;
        services.dialogueAudio(shown);
    }
}

const gameevent::MessageWindows::Window& gameevent::MessageWindows::window(unsigned index) const { return windows.at(index); }
bool gameevent::MessageWindows::speaking() const { return talking; }
bool gameevent::MessageWindows::visible() const {
    return std::any_of(windows.begin(), windows.end(), [](const Window& entry) { return entry.phase != Phase::Hidden; });
}
unsigned gameevent::MessageWindows::activeWindow() const { return active; }
