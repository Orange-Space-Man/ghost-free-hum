#include "resources/glme01addresses.h"
#include "resources/dolresources.h"
#include "game/menu/fileselectdetails.h"
#include "game/startup/startupresources.h"

#include <algorithm>
#include <stdexcept>

namespace {
    const char* tags[3][23]{
        {"plce","1cnm","1cnw","me4a","me4b","tere","tnsl","tnn5","tnn0","tnm2","tnm1","gcm2","gcm1","gnmg","gnm9","gnm8","gnm7","gnm6","gnm5","gnm4","gnm3","gnm2","gnm1"},
        {"2lce","2cnm","2cnw","2e4a","2e4b","2ere","2nsl","2nn5","2nn0","2tm2","2tm1","2cm2","2cm1","2nmg","2nm9","2nm8","2nm7","2nm6","2nm5","2nm4","2nm3","2nm2","2nm1"},
        {"3lce","3cnm","3cnw","3e4a","3e4b","3ere","3nsl","3nn5","3nn0","3tm2","3tm1","3cm2","3cm1","3nmg","3nm9","3nm8","3nm7","3nm6","3nm5","3nm4","3nm3","3nm2","3nm1"}
    };

    menulayout::Pane& pane(menulayout::Layout& layout, unsigned slot, unsigned part) {
        auto* value = layout.find(tags[slot][part]);
        if (!value) throw std::runtime_error(std::string("Missing save-detail pane: ") + tags[slot][part]);
        return *value;
    }

    void fade(menulayout::Pane& value, bool shown) {
        value.alpha = shown ? std::min(255U, value.alpha + 16) : value.alpha - std::min(value.alpha, 16U);
    }

    std::uint32_t word(const std::uint8_t* data) {
        return (std::uint32_t(data[0]) << 24) | (std::uint32_t(data[1]) << 16) | (data[2] << 8) | data[3];
    }

    void hide(menulayout::Layout& layout, unsigned slot) {
        for (unsigned part = 0; part < 23; ++part) {
            if (part >= 11 && part <= 13) continue;
            auto& value = pane(layout, slot, part);
            value.visible = false;
            if (part < 3) value.alpha = 0;
        }
    }

    void number(menulayout::Layout& layout, unsigned slot, unsigned first, unsigned count,
        std::int32_t amount, const std::array<std::string, 10>& textures) {
        unsigned limit = 1;
        for (unsigned i = 0; i < count; ++i) limit *= 10;
        unsigned value = amount < 0 ? 0 : std::min(static_cast<unsigned>(amount), limit - 1);
        unsigned remaining = value;
        unsigned place = 1;
        for (unsigned i = 0; i < count; ++i) {
            auto& digit = pane(layout, slot, first + i);
            fade(digit, true);
            digit.textures[0] = textures[remaining % 10];
            digit.visible = i == 0 || value >= place;
            remaining /= 10;
            place *= 10;
        }
    }
}

bool fileselect::DetailPanes::read(const std::vector<std::uint8_t>& executable, std::string& error) {
    error.clear();
    try {
        DetailPanes result;
        const auto strings = [&](unsigned address, auto& values, bool texture) {
            std::vector<std::uint8_t> table;
            if (!dolresources::read(executable, address, values.size() * 4, table, error)) throw std::runtime_error(error);
            for (std::size_t i = 0; i < values.size(); ++i) {
                std::vector<std::uint8_t> data;
                if (!dolresources::read(executable, word(table.data() + i * 4), 128, data, error)) throw std::runtime_error(error);
                const auto end = std::find(data.begin(), data.end(), 0);
                if (end == data.end()) throw std::runtime_error("Unterminated save-detail resource string.");
                std::string text(data.begin(), end);
                if (texture) text = text.substr(text.find_last_of('/') + 1);
                values[i] = std::move(text);
            }
        };
        strings(glme01::fileDetailDigits, result.digits, true);
        strings(glme01::fileDetailAreas, result.areas, false);
        std::vector<std::uint8_t> table;
        if (!dolresources::read(executable, 0x8033456c, 36, table, error)) throw std::runtime_error(error);
        for (unsigned i = 0; i < 9; ++i) result.offsets[i] = static_cast<std::int32_t>(word(table.data() + i * 4));
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

std::vector<std::string> fileselect::DetailPanes::textures() const { return {digits.begin(), digits.end()}; }

void fileselect::DetailPanes::reset(menulayout::Layout& layout) {
    moneyDigits = 0;
    for (unsigned slot = 0; slot < 3; ++slot) {
        moneyX[slot] = pane(layout, slot, 13).x;
        for (unsigned part = 0; part < 23; ++part) pane(layout, slot, part).alpha = 0;
        hide(layout, slot);
    }
    initialized = true;
}

void fileselect::DetailPanes::update(menulayout::Layout& layout, const State& state, const gamesave::State& saves, bool expanded) {
    if (!initialized || state.selection > 4 || digits[0].empty()) throw std::runtime_error("Save-detail presentation is not initialized.");
    const auto selected = state.selection;
    std::uint32_t money = 0;
    unsigned boos = 0, area = 0;
    if (selected < 3) {
        const auto* record = saves.working.data() + 12 + selected * 0x3b0;
        money = word(record + 0x3a8);
        for (unsigned bit = 0; bit < 60; ++bit) if (record[0x368 + bit / 8] & (1U << (bit % 8))) ++boos;
        area = record[0x353] & 0x10 ? 3 : record[0x352] & 0x20 ? 2 : record[0x340] & 1 ? 1 : 0;
        if (gamesave::slotUsed(saves, selected)) {
            moneyDigits = 0;
            for (auto value = static_cast<std::int32_t>(money); value / 10 != 0 && moneyDigits < 8; value /= 10) ++moneyDigits;
        }
    }
    for (unsigned slot = 0; slot < 3; ++slot) {
        if (slot == selected) {
            layout.raise(tags[slot][0]);
            layout.raise(tags[slot][3]);
        }
        if (slot != selected || !expanded) {
            hide(layout, slot);
        } else if (state.mode != 0 && !(state.mode >= 3 && state.mode <= 7) && state.mode != 10) {
            for (unsigned part = 0; part < 23; ++part) if (part != 4) fade(pane(layout, slot, part), false);
        } else {
            const auto* record = saves.working.data() + 12 + slot * 0x3b0;
            const bool showBoos = (record[0xb] & 8) != 0;
            for (unsigned part = 5; part < 11; ++part) {
                auto& value = pane(layout, slot, part);
                if (showBoos) value.visible = true;
                if (part < 9 || !showBoos) fade(value, showBoos);
            }
            if (showBoos) number(layout, slot, 9, 2, boos, digits);
            if (!gamesave::slotUsed(saves, slot)) {
                for (unsigned part = 0; part < 3; ++part) pane(layout, slot, part).alpha = 0;
                pane(layout, slot, 3).visible = true;
                fade(pane(layout, slot, 3), true);
            } else {
                fade(pane(layout, slot, 11), money >= 1000);
                fade(pane(layout, slot, 12), money >= 1000000);
                fade(pane(layout, slot, 13), true);
                number(layout, slot, 14, 9, static_cast<std::int32_t>(money), digits);
                auto& place = pane(layout, slot, 0);
                place.text = areas[area];
                place.visible = true;
                fade(place, true);
                const unsigned completed = record[0x3a7];
                for (unsigned part = 1; part < 3; ++part) {
                    auto& value = pane(layout, slot, part);
                    if (completed) { value.visible = true; fade(value, true); }
                    else value.alpha = 0;
                }
                pane(layout, slot, 1).text = std::to_string(completed);
            }
        }
        pane(layout, slot, 13).x = moneyX[slot] + offsets[moneyDigits];
    }
}
