#pragma once

#include <filesystem>
#include <string>

// User-selectable enhancements that deliberately deviate from the original hardware output.
// Every option defaults to off, which is the 1:1 behavior.
namespace enhancementsettings {
    void initialize(const std::filesystem::path& path);
    bool persist(std::string& error);
    const std::string& loadError();
    // Dolphin "Disable Copy Filter": the EFB copy skips the vertical row filter (6/52/6 for the shadow depth copy).
    bool disableCopyFilter();
    void setDisableCopyFilter(bool value);
}
