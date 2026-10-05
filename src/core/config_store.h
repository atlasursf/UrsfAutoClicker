#pragma once

/**
 * ConfigStore - JSON persistence for AppSettings.
 *
 *   Linux:   ~/.config/UrsfAutoClicker/config.json
 *   macOS:   ~/Library/Preferences/UrsfAutoClicker/config.json
 *   Windows: %LOCALAPPDATA%/UrsfAutoClicker/config.json
 *
 * QCoreApplication::setApplicationName() must be called before use
 * (it decides the config directory name).
 */

#include <QString>

#include "core/app_settings.h"

class ConfigStore {
public:
    static QString path();

    // Never fails: missing or corrupt file -> defaults. Values are clamped.
    static AppSettings load();

    // Returns false on I/O failure; callers may ignore it (saving must
    // never crash the app).
    static bool save(const AppSettings& settings);
};
