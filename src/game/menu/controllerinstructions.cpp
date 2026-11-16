#include "game/menu/controllerinstructions.h"
#include "resources/gameassetpackage.h"
#include "resources/roomarchivereader.h"
#include "resources/dolresources.h"

#include <stdexcept>
#include <utility>

bool controllerinstructions::Screen::load(const std::filesystem::path& path, std::string& error) {
    try {
        Screen result;
        gameassets::Package package;
        std::vector<std::uint8_t> data, archive, executable, period;
        if (!package.open(path, error) || !package.read("files/Kawano/ENGLISH/res_cont.szp", data, error) ||
            !roomarchive::decompress(data, archive, error) ||
            !roomarchive::read(archive, "controller_2.blo", data, error) || !menulayout::read(data, result.panes, error) ||
            !package.read("sys/main.dol", executable, error) || !dolresources::read(executable, 0x804d0638, 2, period, error))
            throw std::runtime_error(error);
        result.pulsePeriod = (unsigned(period[0]) << 8) | period[1];
        if (result.pulsePeriod < 2 || result.pulsePeriod >= 0x8000)
            throw std::runtime_error("Invalid original controller instruction pulse period.");
        for (const auto* tag : {"ROOT", "aorb", "mesm", "mesc", "mess"})
            if (!result.panes.find(tag)) throw std::runtime_error("Controller instruction pane is absent.");
        for (auto& pane : result.panes.panes) {
            if (pane.kind != "PIC1" || pane.textures[0].empty()) continue;
            bool missing = false;
            if (!roomarchive::read(archive, "timg/" + pane.textures[0], data, error, &missing)) {
                if (!missing) throw std::runtime_error(error);
                pane.textures[0].clear();
            }
        }
        result.initialColor = result.panes.find("aorb")->white;
        if (!result.images.load(archive, result.panes, error, {"e_con_type_a.bti", "e_con_type_b.bti",
            "e_con_message_3.bti", "e_con_message_3b.bti"})) throw std::runtime_error(error);
        result.loaded = true;
        *this = std::move(result);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

controllerinstructions::Result controllerinstructions::Screen::update(std::uint32_t pressed, unsigned option,
    std::uint64_t frame, bool gameplay) {
    if (!loaded) throw std::logic_error("Controller instruction resources are not loaded.");
    if (option > 1) throw std::out_of_range("Controller instruction option is outside its original two choices.");
    Result result;
    if (pressed & 0x03000003) {
        option ^= 1;
        result.changed = true;
        result.sounds.push_back(0x3801);
        if (gameplay) result.playerEvent = 0x24;
    }
    result.scheme = option + 1;
    unsigned weight = static_cast<unsigned>(frame % pulsePeriod);
    const unsigned half = pulsePeriod / 2;
    if (weight > half) weight = pulsePeriod - weight;
    std::uint32_t color = 255;
    for (unsigned shift : {24u, 16u, 8u}) {
        const unsigned original = (initialColor >> shift) & 255;
        const unsigned yellow = shift == 8 ? 0 : 255;
        color |= ((original * weight + yellow * (half - weight)) / half) << shift;
    }
    auto& type = *panes.find("aorb");
    type.white = color;
    type.textures[0] = option ? "e_con_type_b.bti" : "e_con_type_a.bti";
    panes.find("mesm")->textures[0] = option ? "e_con_message_3b.bti" : "e_con_message_3.bti";
    unsigned blink = static_cast<unsigned>(frame % 60);
    if (blink > 30) blink = 60 - blink;
    panes.find("mess")->alpha = (blink + 30) * 255 / 60;
    if (pressed & 0x1300) {
        result.close = true;
        result.sounds.push_back(0x3802);
    }
    return result;
}

void controllerinstructions::Screen::reset() { *this = Screen{}; }
const menulayout::Layout& controllerinstructions::Screen::layout() const { return panes; }
const menulayout::Graphics& controllerinstructions::Screen::graphics() const { return images; }
