#include "resources/dolresources.h"
#include "game/menu/fileselectpanes.h"
#include "game/startup/startupresources.h"

#include <algorithm>
#include <stdexcept>

namespace {
    struct Message { unsigned layout; const char* tag; unsigned mode; };
    constexpr Message messages[]{
        {0,"m11a",0}, {0,"m21a",3}, {0,"m22a",4}, {0,"m23a",5},
        {0,"m31a",7}, {0,"m32a",8}, {0,"m33a",9},
        {1,"er1a",11}, {1,"er2a",12}, {1,"e31a",13}, {1,"e32a",14},
        {1,"e33a",15}, {1,"e34a",16}, {1,"e41a",18}, {1,"e42a",19}
    };
    constexpr const char* groups[5][7]{
        {"cur1","fra1","mov1","sha1","pic1","ti1a","ti1b"},
        {"cur2","fra2","mov2","sha2","pic2","ti2a","ti2b"},
        {"cur3","fra3","mov3","sha3","pic3","ti3a","ti3b"},
        {"curo","frao","movo","sha4","pico","ti4a","ti4b"},
        {"cur5","fra5","mov5","sha5","pic5","ti5a","ti5b"}
    };

    menulayout::Pane& pane(menulayout::Layout& layout, const char* tag) {
        auto* result = layout.find(tag);
        if (!result) throw std::runtime_error(std::string("Missing file-selection pane: ") + tag);
        return *result;
    }

    void fade(menulayout::Pane& value, bool shown, unsigned step = 16) {
        value.alpha = shown ? std::min(255U, value.alpha + step) : value.alpha - std::min(value.alpha, step);
    }

    void confirmation(menulayout::Layout& layout, const char* yes, const char* no, const char* yesCursor,
        const char* noCursor, bool active, unsigned selection) {
        fade(pane(layout, yes), active);
        fade(pane(layout, no), active);
        auto& first = pane(layout, yesCursor);
        auto& second = pane(layout, noCursor);
        if (active) {
            (selection == 0 ? first : second).alpha = 255;
            fade(selection == 0 ? second : first, false, 32);
        } else {
            fade(first, false);
            fade(second, false);
        }
    }

    void pulse(menulayout::Layout& layout, const char* tag, unsigned phase) {
        const auto alpha = pane(layout, tag).alpha;
        std::string paired = tag;
        paired.back() = 'b';
        auto& value = pane(layout, paired.c_str());
        value.alpha = phase < 60 ? phase * alpha / 60 : phase > 119 ? alpha * (210 - phase) / 90 : alpha;
    }
}

bool fileselect::OptionsPanes::read(const std::vector<std::uint8_t>& executable, std::string& error) {
    error.clear();
    try {
        OptionsPanes result;
        for (unsigned i = 0; i < result.helpText.size(); ++i) {
            std::vector<std::uint8_t> pointer, text;
            if (!dolresources::read(executable, 0x803346d8 + i * 4, 4, pointer, error)) throw std::runtime_error(error);
            const unsigned address = (unsigned(pointer[0]) << 24) | (pointer[1] << 16) | (pointer[2] << 8) | pointer[3];
            if (!dolresources::read(executable, address, 256, text, error)) throw std::runtime_error(error);
            const auto end = std::find(text.begin(), text.end(), 0);
            if (end == text.end()) throw std::runtime_error("Unterminated options help text.");
            result.helpText[i].assign(text.begin(), end);
        }
        const unsigned addresses[]{0x80334144,0x804d05c8,0x804d05d0,0x80334208,0x804d05d8,0x804d05e0};
        for (unsigned group = 0; group < 6; ++group) {
            std::vector<std::uint8_t> data;
            const unsigned count = group % 3 == 0 ? 3 : 2;
            if (!dolresources::read(executable, addresses[group], count * 4, data, error)) throw std::runtime_error(error);
            for (unsigned entry = 0; entry < count; ++entry) {
                const auto offset = entry * 4;
                const std::uint32_t address = (std::uint32_t(data[offset]) << 24) | (data[offset + 1] << 16) |
                    (data[offset + 2] << 8) | data[offset + 3];
                std::vector<std::uint8_t> name;
                if (!dolresources::read(executable, address, 128, name, error)) throw std::runtime_error(error);
                const auto end = std::find(name.begin(), name.end(), 0);
                if (end == name.end()) throw std::runtime_error("Unterminated options texture path.");
                const std::string path(name.begin(), end);
                const auto slash = path.find_last_of('/');
                result.choices[group].push_back(path.substr(slash == std::string::npos ? 0 : slash + 1));
            }
        }
        *this = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

std::vector<std::string> fileselect::OptionsPanes::textures() const {
    std::vector<std::string> result;
    for (const auto& group : choices) result.insert(result.end(), group.begin(), group.end());
    return result;
}

void fileselect::OptionsPanes::reset(menulayout::Layout& layout) {
    phase = 255;
    for (const auto* tag : {"o1sa","o1va","o1ca","o1ba","o1oa","o1pa","d1sa","d1va","d1ca","o2sa","o2va","o2ca"}) {
        pane(layout, tag).alpha = 0;
        std::string paired = tag;
        paired.back() = 'b';
        pane(layout, paired.c_str()).alpha = 0;
    }
    for (const auto* tag : {"oc1s","oc1v","oc1c","oc1b","oc1o","oc1p","arsl","arvl","arcl","arsr","arvr","arcr", "smpl","chg1","chg2","chg3","chl1","frao"}) pane(layout, tag).visible = false;
    pane(layout, "dlby").alpha = 0;
    auto& message = pane(layout, "mesw");
    message.alpha = 0;
    message.colors = {0x6ea0d4ff,0x6ea0d4ff,0x6ea0d420,0x6ea0d420};
    const int x = message.x, y = message.y;
    for (unsigned i = 0; i < 3; ++i) {
        const char* tag = i == 0 ? "helpPortrait" : i == 1 ? "helpShadow" : "helpText";
        if (!layout.find(tag)) { menulayout::Pane added; added.tag = tag; layout.panes.push_back(added); }
        auto& help = pane(layout, tag);
        help.visible = false;
        help.order = 100000 + i;
        help.kind = i == 0 ? "PIC1" : "TBX1";
        help.x = x + (i == 0 ? 22 : i == 1 ? 62 : 60);
        help.y = y + (i == 0 ? 24 : i == 1 ? 24 : 22);
        help.width = i == 0 ? 32 : 1000;
        help.height = i == 0 ? 32 : 48;
        help.fontWidth = help.fontHeight = 23;
        help.lineSpacing = 24;
        help.textAlignment = 10;
        help.colors.fill(i == 1 ? 0x000000ff : 0xffffffff);
        if (i == 0) help.textures[0] = "optionsPortrait";
    }
}

void fileselect::OptionsPanes::update(menulayout::Layout& layout, const State& state) {
    if (state.optionSelection > 5 || state.options[0] > 2 || state.options[1] > 1 || state.options[2] > 1)
        throw std::runtime_error("Invalid options presentation state.");
    const bool shown = state.mode == 20;
    const char* labels[]{"o1sa","o1va","o1ca","o1ba","o1oa","o1pa"};
    const char* cursors[]{"oc1s","oc1v","oc1c","oc1b","oc1o","oc1p"};
    for (unsigned i = 0; i < 6; ++i) {
        fade(pane(layout, labels[i]), shown);
        pane(layout, cursors[i]).visible = shown && state.optionSelection == i;
    }
    const char* descriptions[]{"d1sa","d1va","d1ca"};
    const char* values[]{"o2sa","o2va","o2ca"};
    const char* left[]{"arsl","arvl","arcl"};
    const char* right[]{"arsr","arvr","arcr"};
    for (unsigned i = 0; i < 3; ++i) {
        fade(pane(layout, descriptions[i]), shown);
        fade(pane(layout, values[i]), shown);
        pane(layout, left[i]).visible = pane(layout, right[i]).visible = shown && state.optionSelection == i;
        if (!choices[i].empty()) {
            pane(layout, values[i]).textures[0] = choices[i].at(state.options[i]);
            std::string overlay = values[i];
            overlay.back() = 'b';
            pane(layout, overlay.c_str()).textures[0] = choices[i + 3].at(state.options[i]);
        }
    }
    for (const auto* tag : {"smpl","chg1","chg2","chg3","chl1","frao"}) pane(layout, tag).visible = shown && state.optionSelection == 3;
    fade(pane(layout, "mesw"), shown);
    for (const auto* tag : {"helpPortrait", "helpShadow", "helpText"}) {
        auto& help = pane(layout, tag);
        help.visible = pane(layout, "mesw").alpha == 255;
        help.text = helpText[state.optionSelection < 2 ? 0 : state.optionSelection - 1];
    }
    fade(pane(layout, "dlby"), shown && state.options[0] == 2 && state.optionSelection != 3);
    phase = (phase + 1) % 210;
    for (const auto* tag : labels) pulse(layout, tag, phase);
    for (const auto* tag : descriptions) pulse(layout, tag, phase);
    for (const auto* tag : values) pulse(layout, tag, phase);
}

void fileselect::SelectionPanes::reset(std::array<menulayout::Layout, 3>& layouts) {
    initialized = false;
    phase = 255;
    scales.fill(1);
    for (unsigned group = 0; group < 5; ++group) {
        for (unsigned part = 0; part < 7; ++part) {
            auto& value = pane(layouts[group == 4 ? 1 : 0], groups[group][part]);
            original[group][part] = value;
            if (group < 4 && part == 0) cursorAlpha = value.alpha;
            value.alpha = 0;
            if (part == 2 || part == 4) value.visible = true;
        }
    }
    pane(layouts[1], "pico").visible = false;
    const char* locations[3][4]{{"curo","frao","movo","sha4"}, {"curo","frao","movo","shao"}, {"frao","frao","movo","shao"}};
    for (unsigned layout = 0; layout < 3; ++layout) for (unsigned part = 0; part < 4; ++part) {
        const auto& value = pane(layouts[layout], locations[layout][part]);
        destinations[layout][part] = {(value.x * 2 + value.width) / 2, (value.y * 2 + value.height) / 2};
    }
    centers = destinations[0];
    initialized = true;
}

void fileselect::SelectionPanes::update(std::array<menulayout::Layout, 3>& layouts, const State& state,
    const std::array<bool, 3>& used) {
    if (!initialized || state.selection > 4 || state.copyDestination > 2) throw std::runtime_error("Invalid file-selection pane state.");
    const unsigned mode = state.mode;
    const bool fileMode = mode == 0 || (mode >= 3 && mode <= 10) || mode == 21;
    const bool copying = mode >= 3 && mode <= 6;
    const unsigned destination = fileMode ? 0 : mode == 20 ? 2 : 1;
    for (unsigned part = 0; part < 4; ++part) for (unsigned axis = 0; axis < 2; ++axis) {
        auto& current = centers[part][axis];
        const int target = destinations[destination][part][axis];
        const float step = (target - current) * 0.1f;
        if (step >= 1 || step <= -1) current = static_cast<int>(float(current) + step);
        else if (current < target) ++current;
        else if (current > target) --current;
    }
    for (unsigned group = 0; group < 5; ++group) {
        auto& layout = layouts[group == 4 ? 1 : 0];
        const bool selected = state.selection == group;
        const bool copyTarget = copying && state.copyDestination == group;
        if (selected && !copying) {
            for (unsigned part : {3U,2U,4U,1U,0U,6U,5U}) layout.raise(groups[group][part]);
        }
        const bool shown = group < 3 ? fileMode : group == 3 ? !(mode >= 3 && mode <= 10) && !(mode >= 13 && mode <= 17) :
            mode == 11 || mode == 12 || mode == 18 || mode == 19;
        for (unsigned part : {1U,2U,3U,4U,5U}) {
            auto& value = pane(layout, groups[group][part]);
            const bool visible = shown && (part != 4 || (!selected && !copyTarget));
            fade(value, visible);
            if (part == 3 && visible) value.alpha = std::min(value.alpha, original[group == 4 ? 3 : group][part].alpha);
        }
        auto& cursor = pane(layout, groups[group][0]);
        if (selected) scales[group] = std::min(1.3f, scales[group] + (1.3f - 1.0f) / 25.0f);
        else if (copyTarget) scales[group] = std::min(1.2f, scales[group] + 0.20000005f / 25.0f);
        else scales[group] = 1;
        cursor.visible = selected ? !copying && !(mode >= 13 && mode <= 17) && mode != 20 : copyTarget;
        if (cursor.visible) {
            unsigned triangle = state.cursorPhase % 512;
            if (triangle > 255) triangle = 511 - triangle;
            cursor.alpha = triangle * cursorAlpha / 255;
        }
        for (unsigned part = 0; part < 4; ++part) {
            auto& value = pane(layout, groups[group][part]);
            const auto& base = original[group][part];
            if (group == 3) {
                const int halfWidth = static_cast<int>(base.width * scales[group] * 0.5f);
                const int halfHeight = static_cast<int>(base.height * scales[group] * 0.5f);
                value.x = centers[part][0] - halfWidth;
                value.y = centers[part][1] - halfHeight;
                value.width = halfWidth * 2;
                value.height = halfHeight * 2;
            } else {
                const int expansionX = static_cast<int>(base.width * (scales[group] - 1) * 0.5f);
                const int expansionY = static_cast<int>(base.height * (scales[group] - 1) * 0.5f);
                value.x = base.x - expansionX;
                value.y = base.y - expansionY;
                value.width = base.width + expansionX * 2;
                value.height = base.height + expansionY * 2;
            }
        }
        const auto& preview = pane(layout, groups[group][2]);
        auto& cover = pane(layout, groups[group][4]);
        cover.x = preview.x; cover.y = preview.y; cover.width = preview.width; cover.height = preview.height;
        const auto& frame = pane(layout, groups[group][1]);
        for (unsigned part : {5U,6U}) {
            auto& title = pane(layout, groups[group][part]);
            const auto& base = original[group][5];
            title.x = selected || copyTarget || group == 3 ? frame.x : base.x;
            title.y = selected || copyTarget || group == 3 ? frame.y + frame.height : base.y;
            title.width = base.width;
            title.height = base.height;
        }
        if (!selected && copying && !copyTarget && group < 3 && used[group]) fade(pane(layout, groups[group][5]), false, 32);
    }
    phase = (phase + 1) % 210;
    for (unsigned group = 0; group < 5; ++group) pulse(layouts[group == 4 ? 1 : 0], groups[group][5], phase);
}

bool fileselect::SelectionPanes::expanded(unsigned group) const {
    return initialized && group < scales.size() && scales[group] == 1.3f;
}

void fileselect::HelpPanes::reset(menulayout::Layout& layout) {
    timer = previousMode = 0;
    for (const auto* tag : {"tg11","tg12","tg20","tg31","tg32"}) {
        auto& value = pane(layout, tag);
        value.alpha = 0;
        value.visible = true;
    }
}

void fileselect::HelpPanes::update(menulayout::Layout& layout, const State& state) {
    if (state.mode != previousMode && (state.mode == 0 || state.mode == 3 || state.mode == 7)) timer = 0;
    previousMode = state.mode;
    auto& first = pane(layout, "tg11");
    auto& second = pane(layout, "tg12");
    if (state.mode == 0) {
        if (timer < 301) {
            if (first.alpha == 255) ++timer;
            else fade(first, true, 4);
            fade(second, timer < 225, 4);
        } else {
            fade(first, false, 4);
            fade(second, false, 4);
            if (first.alpha == 0 && ++timer > 375) timer = 0;
        }
    } else {
        fade(first, false);
        fade(second, false);
    }
    auto& copy = pane(layout, "tg20");
    if (state.mode == 3) {
        if (timer < 301) {
            fade(copy, true, 4);
            if (copy.alpha == 255) ++timer;
        } else {
            fade(copy, false, 4);
            if (copy.alpha == 0) timer = 0;
        }
    } else fade(copy, false);
    auto& erase = pane(layout, "tg31");
    auto& warning = pane(layout, "tg32");
    if (state.mode == 7) {
        auto& outgoing = timer < 301 ? warning : erase;
        auto& incoming = timer < 301 ? erase : warning;
        fade(outgoing, false, 4);
        if (outgoing.alpha == 0) fade(incoming, true, 4);
        if (++timer > 600) timer = 0;
    } else {
        fade(erase, false);
        fade(warning, false);
    }
}

void fileselect::bindPreviews(std::array<menulayout::Layout, 3>& layouts, const State& state,
    const std::array<bool, 3>& used, const std::array<int, 4>& textures) {
    for (unsigned group = 0; group < 5; ++group) {
        const unsigned model = group < 3 ? (used[group] ? 1 : 0) : group == 3 ? (state.mode == 20 ? 3 : 2) : 0;
        pane(layouts[group == 4 ? 1 : 0], groups[group][2]).previewTexture = textures[model];
    }
}

void fileselect::MessagePanes::reset(std::array<menulayout::Layout, 3>& layouts) {
    phase = 255;
    damagedPhase = 0;
    for (const auto& message : messages) {
        auto& value = pane(layouts[message.layout], message.tag);
        value.alpha = 0;
        value.visible = true;
        std::string paired = message.tag;
        paired.back() = 'b';
        auto& overlay = pane(layouts[message.layout], paired.c_str());
        overlay.alpha = 0;
        overlay.visible = true;
    }
    for (const auto* tag : {"note", "sv1a", "sv2a", "ye1a", "no1a", "oc1y", "oc1n", "m12a", "m13a", "me5a", "me6a"}) {
        auto& value = pane(layouts[0], tag);
        value.alpha = 0;
        value.visible = true;
    }
    for (const auto* tag : {"ye2a", "no2a", "oc2y", "oc2n"}) {
        auto& value = pane(layouts[1], tag);
        value.alpha = 0;
        value.visible = true;
    }
    if (auto* value = layouts[1].find("es1a")) { value->alpha = 0; value->visible = true; }
    for (unsigned layout = 0; layout < 2; ++layout) {
        const char* first[]{"sv1b", "sv2b", "ye1b", "no1b", "m12b", "m13b", "me5b", "me6b"};
        const char* second[]{"ye2b", "no2b"};
        const auto* tags = layout == 0 ? first : second;
        for (unsigned i = 0; i < (layout == 0 ? 8U : 2U); ++i) {
            auto& value = pane(layouts[layout], tags[i]);
            value.alpha = 0;
            value.visible = true;
        }
    }
    if (auto* value = layouts[1].find("es1b")) { value->alpha = 0; value->visible = true; }
}

void fileselect::MessagePanes::update(std::array<menulayout::Layout, 3>& layouts, const State& state,
    const std::array<bool, 3>& used) {
    if (state.confirmation > 1 || state.selection > 4) throw std::runtime_error("Invalid file-selection presentation state.");
    for (const auto& message : messages) fade(pane(layouts[message.layout], message.tag), state.mode == message.mode);
    confirmation(layouts[0], "ye1a", "no1a", "oc1y", "oc1n", state.mode == 7, state.confirmation);
    confirmation(layouts[1], "ye2a", "no2a", "oc2y", "oc2n", state.mode == 13 || state.mode == 14, state.confirmation);
    fade(pane(layouts[0], "note"), state.mode == 18 || state.mode == 19);
    fade(pane(layouts[0], "me6a"), state.mode == 11 || state.mode == 12 || state.mode == 18 || state.mode == 19);
    if (auto* value = layouts[1].find("es1a")) fade(*value, state.mode == 21);
    const bool saving = state.mode == 6 || state.mode == 10 || state.mode == 17;
    fade(pane(layouts[0], "sv1a"), saving);
    fade(pane(layouts[0], "sv2a"), saving);
    const bool selectedFile = state.mode == 0 && state.selection < 3 && used[state.selection];
    bool destination = false;
    for (unsigned i = 0; i < 3; ++i) if (i != state.selection && !used[i]) destination = true;
    fade(pane(layouts[0], "m13a"), selectedFile);
    fade(pane(layouts[0], "m12a"), selectedFile && destination);
    fade(pane(layouts[0], "me5a"), state.mode == 0 && state.selection == 3);
    phase = (phase + 1) % 210;
    damagedPhase = (damagedPhase + 1) % 210;
    for (const auto& message : messages) pulse(layouts[message.layout], message.tag, phase);
    for (const auto* tag : {"sv1a", "sv2a", "ye1a", "no1a", "m12a", "m13a", "me5a", "me6a"}) pulse(layouts[0], tag, phase);
    for (const auto* tag : {"ye2a", "no2a"}) pulse(layouts[1], tag, phase);
    if (layouts[1].find("es1a")) pulse(layouts[1], "es1a", damagedPhase);
}
