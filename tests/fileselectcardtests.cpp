#include "game/menu/fileselectcard.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
}

int main() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("polterport-menu-card-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        gamesave::State defaults, saves;
        std::string error;
        require(gamesave::initialize({0xff}, defaults, error), "Default save initialization.");
        saves = defaults;
        saves.working[13] = 1;
        saves.working[14] = 0x55;
        saves.working[24] = 0xff;
        saves.working[12 + 0x3a7] = 2;
        const unsigned destination = 12 + 0x3b0;
        saves.working[destination + 12] = 0xa0;
        saves.working[destination + 13] = 0x7b;
        gamecard::Storage card;
        require(card.open(directory / "slot-a.gci", saves, error) == gamecard::LoadResult::empty, "Empty card.");
        gamecard::Block banner{};
        if (!card.save(saves, banner, 1, 1, error)) throw std::runtime_error(error);
        if (!fileselect::writeCardAction({fileselect::Action::copy,0,1}, saves, defaults, card, banner, 2, 2, error))
            throw std::runtime_error(error);
        require(gamesave::slotUsed(saves, 1) && saves.working[destination + 2] == 0x55, "Copy did not preserve source flags.");
        require(saves.working[destination + 12] == 0xbf && saves.working[destination + 13] == 0x7b,
            "Copy overwrote destination bits excluded by original serialization.");
        require((saves.working[destination + 1] & 3) == 3, "Completed-game flag.");
        auto before = saves;
        require(!fileselect::writeCardAction({fileselect::Action::copy,0,1}, saves, defaults, card, banner, 3, 3, error),
            "Copy overwrote an occupied destination.");
        require(saves.working == before.working, "Failed action changed memory.");
        if (!fileselect::writeCardAction({fileselect::Action::erase,1}, saves, defaults, card, banner, 4, 4, error))
            throw std::runtime_error(error);
        require(!gamesave::slotUsed(saves, 1) && gamesave::slotUsed(saves, 0), "Erase touched the wrong slot.");
        require(std::equal(saves.working.begin() + destination, saves.working.begin() + destination + 0x3b0,
            defaults.working.begin() + destination), "Erase did not restore the original defaults.");
        gamecard::Storage reload;
        gamesave::State loaded = defaults;
        require(reload.open(directory / "slot-a.gci", loaded, error) == gamecard::LoadResult::loaded && loaded.working == saves.working,
            "Card actions did not survive reload.");
        before = saves;
        require(card.save(saves, banner, 5, 5, error), "Advance external writer.");
        require(!fileselect::writeCardAction({fileselect::Action::erase,0}, loaded, defaults, reload, banner, 6, 6, error),
            "Stale card action overwrote another writer.");
        require(loaded.working == before.working, "Failed disk write changed live save.");
        std::filesystem::remove_all(directory);
        std::cout << "Menu card action checks passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        std::filesystem::remove_all(directory);
        return 1;
    }
}
