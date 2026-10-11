#include "game/menu/menulayout.h"

#include <stdexcept>
#include <utility>
#include <algorithm>

namespace {
    class Reader {
    public:
        const std::vector<std::uint8_t>& data;
        std::size_t position = 0, end = 0;

        unsigned byte() {
            if (position >= end) throw std::runtime_error("Truncated menu layout block.");
            return data[position++];
        }

        unsigned word() {
            const auto high = byte();
            return high * 256 + byte();
        }

        std::uint32_t dword() {
            const auto high = word();
            return (high << 16) | word();
        }

        int signedWord() { return static_cast<std::int16_t>(word()); }

        std::string string(std::size_t length) {
            if (position > end || length > end - position) throw std::runtime_error("Truncated menu layout string.");
            std::string result(data.begin() + position, data.begin() + position + length);
            position += length;
            return result;
        }

        void align() {
            while (position & 3) byte();
        }

        std::string resource() {
            const auto type = byte();
            const auto length = byte();
            auto result = string(length);
            if (type == 0 && length == 0) return {};
            if (type != 2) throw std::runtime_error("Unsupported menu resource reference.");
            return result;
        }
    };

    void require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }

    void readPane(Reader& reader, menulayout::Pane& pane) {
        const auto fields = reader.byte();
        require(fields >= 6 && fields <= 9, "Unsupported menu pane fields.");
        pane.visible = reader.byte() != 0;
        reader.word();
        pane.tag = reader.string(4);
        pane.x = reader.signedWord();
        pane.y = reader.signedWord();
        pane.width = reader.signedWord();
        pane.height = reader.signedWord();
        if (fields >= 7) pane.rotation = reader.word();
        if (fields >= 8) pane.anchor = reader.byte();
        if (fields >= 9) pane.alpha = reader.byte();
        require(pane.anchor <= 8 && pane.width >= 0 && pane.height >= 0, "Invalid menu pane bounds.");
        reader.align();
    }

    void readPicture(Reader& reader, menulayout::Pane& pane) {
        const auto fields = reader.byte();
        require(fields >= 3 && fields <= 7, "Unsupported menu picture fields.");
        pane.textures[0] = reader.resource();
        pane.palette = reader.resource();
        pane.binding = reader.byte();
        if (fields >= 4) pane.mirror = reader.byte();
        if (fields >= 5) pane.wrap = reader.byte();
        if (fields >= 6) pane.black = reader.dword();
        if (fields >= 7) pane.white = reader.dword();
        reader.align();
    }

    void readWindow(Reader& reader, menulayout::Pane& pane) {
        const auto fields = reader.byte();
        require(fields >= 14 && fields <= 17, "Unsupported menu window fields.");
        for (auto& value : pane.content) value = reader.word();
        for (unsigned index = 0; index < 4; ++index) pane.textures[index] = reader.resource();
        pane.palette = reader.resource();
        pane.mirror = reader.byte();
        for (auto& value : pane.colors) value = reader.dword();
        if (fields >= 15) pane.textures[4] = reader.resource();
        if (fields >= 16) pane.black = reader.dword();
        if (fields >= 17) pane.white = reader.dword();
        reader.align();
    }

    void readText(Reader& reader, menulayout::Pane& pane) {
        const auto fields = reader.byte();
        require(fields == 10 || fields == 11, "Unsupported menu text fields.");
        pane.font = reader.resource();
        pane.colors[0] = reader.dword();
        pane.colors[1] = reader.dword();
        pane.textAlignment = reader.byte();
        pane.characterSpacing = reader.signedWord();
        pane.lineSpacing = reader.signedWord();
        pane.fontWidth = reader.word();
        pane.fontHeight = reader.word();
        const auto length = reader.word();
        pane.text = reader.string(length);
        if (fields == 11) pane.textConnected = reader.byte() != 0;
        reader.align();
    }
}

void menulayout::Layout::raise(const std::string& tag) {
    auto* value = find(tag);
    if (!value) throw std::runtime_error("Missing menu pane: " + tag);
    std::uint64_t last = 0;
    for (const auto& sibling : panes) if (sibling.parent == value->parent) last = std::max(last, sibling.order);
    value->order = last + 1;
}

std::vector<std::size_t> menulayout::Layout::drawOrder() const {
    std::vector<std::vector<std::size_t>> children(panes.size() + 1);
    for (std::size_t i = 0; i < panes.size(); ++i) {
        const int parent = panes[i].parent;
        if (parent < -1 || parent >= static_cast<int>(i)) throw std::runtime_error("Invalid menu pane hierarchy.");
        children[parent + 1].push_back(i);
    }
    for (auto& siblings : children) std::stable_sort(siblings.begin(), siblings.end(), [&](std::size_t a, std::size_t b) {
        return panes[a].order < panes[b].order;
    });
    std::vector<std::size_t> pending(children[0].rbegin(), children[0].rend()), result;
    while (!pending.empty()) {
        const auto index = pending.back();
        pending.pop_back();
        result.push_back(index);
        const auto& descendants = children[index + 1];
        pending.insert(pending.end(), descendants.rbegin(), descendants.rend());
    }
    return result;
}

menulayout::Pane* menulayout::Layout::find(const std::string& tag) {
    for (auto& pane : panes) if (pane.tag == tag) return &pane;
    return nullptr;
}

bool menulayout::read(const std::vector<std::uint8_t>& data, Layout& layout, std::string& error) {
    error.clear();
    try {
        Reader reader{data, 0, data.size()};
        require(reader.string(8) == "SCRNblo1", "Unsupported menu layout signature.");
        const auto fileSize = reader.dword();
        require(fileSize == data.size() && fileSize >= 32, "Invalid menu layout length.");
        reader.position = 32;
        Layout result;
        std::vector<int> parents;
        int lastPane = -1;
        bool information = false, ended = false;
        while (reader.position < data.size()) {
            reader.end = data.size();
            const auto start = reader.position;
            const auto kind = reader.string(4);
            const auto size = reader.dword();
            require(size >= 8 && size <= data.size() - start, "Invalid menu block length.");
            reader.end = start + size;
            if (kind == "INF1") {
                require(!information && result.panes.empty(), "Duplicate menu canvas.");
                result.width = reader.word();
                result.height = reader.word();
                result.background = reader.dword();
                information = true;
            } else if (kind == "BGN1") {
                require(lastPane >= 0, "Menu hierarchy has no parent.");
                parents.push_back(lastPane);
                lastPane = -1;
            } else if (kind == "END1") {
                require(!parents.empty(), "Unbalanced menu hierarchy.");
                parents.pop_back();
                lastPane = -1;
            } else if (kind == "EXT1") {
                require(parents.empty(), "Unclosed menu hierarchy.");
                ended = true;
                break;
            } else {
                require(information, "Menu pane precedes canvas.");
                require(kind == "PAN1" || kind == "PIC1" || kind == "WIN1" || kind == "TBX1", "Unsupported menu block.");
                Pane pane;
                pane.kind = kind;
                pane.parent = parents.empty() ? -1 : parents.back();
                readPane(reader, pane);
                if (kind == "PIC1") readPicture(reader, pane);
                if (kind == "WIN1") readWindow(reader, pane);
                if (kind == "TBX1") readText(reader, pane);
                result.panes.push_back(std::move(pane));
                lastPane = static_cast<int>(result.panes.size() - 1);
            }
            reader.position = reader.end;
        }
        require(information && ended && result.width && result.height, "Incomplete menu layout.");
        layout = std::move(result);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
