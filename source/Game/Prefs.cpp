#include "Prefs.h"

#include "Audio/Audio.h"
#include "Base/FileUtils.h"
#include "Base/Finally.h"
#include "Base/IniUtils.h"
#include "Data.h"
#include "DoomDefines.h"
#include <cstdio>
#include <filesystem>
#include <SDL.h>
#include <string>

BEGIN_NAMESPACE(Prefs)

//----------------------------------------------------------------------------------------------------------------------
// Determines the path to the save .ini for preferences
//----------------------------------------------------------------------------------------------------------------------
static std::string determineIniFilePath() noexcept {
    char* const pCfgFilePath = SDL_GetPrefPath(SAVE_FILE_ORG, SAVE_FILE_PRODUCT);
    auto cleanupCfgFilePath = finally([&](){
        SDL_free(pCfgFilePath);
    });

    if (!pCfgFilePath) {
        FATAL_ERROR("Unable to create or determine the path to the configuration for the game!");
        return std::string();
    }

    std::string path = pCfgFilePath;
    path += "save.ini";   // Note: path is guaranteed to have a separator at the end, as per SDL docs!
    return path;
}

//-------------------------------------------------------------------------------------------------
// Clear out all prefs settings to the defaults
//-------------------------------------------------------------------------------------------------
static void resetPrefsToDefaults() noexcept {
    gStartSkill = sk_medium;                        // Init the basic skill level
    gStartMap = 1;                                  // Only allow playing from map #1
    Audio::setSoundVolume(Audio::MAX_VOLUME - 3);   // Init the sound effects volume (make slightly less than music by default)
    Audio::setMusicVolume(Audio::MAX_VOLUME);       // Init the music volume
    gMaxLevel = 1;                                  // Only allow level 1 to select from
    gScreenSize = 0;                                // Default screen size
}

//----------------------------------------------------------------------------------------------------------------------
// Handle a prefs file entry.
// This is not a particularly elegant or fast implementation, but it gets the job done... 
//----------------------------------------------------------------------------------------------------------------------
static void handlePrefsFileEntry(const IniUtils::Entry& entry) noexcept {
    if (!entry.section.empty())
        return;
    
    if (entry.key == "StartSkill") {
        skill_e newSkill = (skill_e) entry.getUintValue((uint32_t) gStartSkill);
        const bool bValidSkill = (
            (gStartSkill == sk_baby) ||
            (gStartSkill == sk_easy) ||
            (gStartSkill == sk_medium) ||
            (gStartSkill == sk_hard) ||
            (gStartSkill == sk_nightmare)
        );

        if (bValidSkill) {
            gStartSkill = newSkill;
        }
    }
    else if (entry.key == "StartMap") {
        gStartMap = entry.getUintValue((uint32_t) gStartMap);
        gStartMap = std::max(gStartMap, 1u);
        gStartMap = std::min(gStartMap, 24u);
    }
    else if (entry.key == "MaxLevel") {
        gMaxLevel = entry.getUintValue((uint32_t) gMaxLevel);
        gMaxLevel = std::max(gMaxLevel, 1u);
        gMaxLevel = std::min(gMaxLevel, 24u);
    }
    else if (entry.key == "ScreenSize") {
        gScreenSize = entry.getUintValue((uint32_t) gScreenSize);
        gScreenSize = std::min(gScreenSize, 5u);
    }
    else if (entry.key == "SoundVolume") {
        const uint32_t curVolume = Audio::getSoundVolume();
        uint32_t newVolume = entry.getUintValue(curVolume);
        newVolume = std::min(newVolume, Audio::MAX_VOLUME);
        Audio::setSoundVolume(newVolume);
    }
    else if (entry.key == "MusicVolume") {
        const uint32_t curVolume = Audio::getMusicVolume();
        uint32_t newVolume = entry.getUintValue(curVolume);
        newVolume = std::min(newVolume, Audio::MAX_VOLUME);
        Audio::setMusicVolume(newVolume);
    }
}

void load() noexcept {
    // Start off with the default settings initially
    resetPrefsToDefaults();

    // Determine if the prefs file exists.
    // If it doesn't then we use use the default settings and write out a default prefs file.
    std::string iniFilePath = determineIniFilePath();
    bool bPrefsFileExists = false;

    try {
        bPrefsFileExists = std::filesystem::exists(iniFilePath);
    } catch (...) {
        // Ignore, we can recover...
    }

    if (!bPrefsFileExists) {
        save();
        return;
    }
    
    // Read the ini file bytes and parse if we can
    std::byte* pIniFileData = nullptr;
    size_t iniFileDataSize = 0;

    auto cleanupIniFileData = finally([&](){
        delete[] pIniFileData;
    });
    
    if (!FileUtils::getContentsOfFile(iniFilePath.c_str(), pIniFileData, iniFileDataSize))
        return;
    
    IniUtils::parseIniFromString((const char*) pIniFileData, iniFileDataSize, handlePrefsFileEntry);

    // Make sure this does not conflict:
    gStartMap = std::min(gStartMap, gMaxLevel);
}

void save() noexcept {
    // Makeup the string for the prefs file
    std::string text;
    text.reserve(4096);

    text.append("# Save file generated by Pheonix Doom!");
    text.append("\nStartSkill = ");
    text.append(std::to_string((uint32_t) gStartSkill));
    text.append("\nStartMap = ");
    text.append(std::to_string(gStartMap));
    text.append("\nMaxLevel = ");
    text.append(std::to_string(gMaxLevel));
    text.append("\nScreenSize = ");
    text.append(std::to_string(gScreenSize));
    text.append("\nSoundVolume = ");
    text.append(std::to_string(Audio::getSoundVolume()));
    text.append("\nMusicVolume = ");
    text.append(std::to_string(Audio::getMusicVolume()));
    
    // Write it out to the prefs file
    const std::string iniFilePath = determineIniFilePath();
    FILE* const pFile = std::fopen(iniFilePath.c_str(), "w");

    auto cleanupFile = finally([&]() {
        if (pFile) {
            std::fclose(pFile);
        }
    });

    if (std::fwrite(text.data(), text.length(), 1, pFile) != 0) {
        std::fflush(pFile);
    }
}

END_NAMESPACE(Prefs)
