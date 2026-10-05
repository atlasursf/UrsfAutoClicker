#pragma once

/**
 * AppSettings - the single source of truth for every user setting.
 *
 * Plain data, no Qt Widgets. ConfigStore (de)serializes it, the UI pages
 * read/write it, ClickerController consumes it.
 */

#include <QString>
#include <QStringList>

#include "core/clicker_engine.h"

struct AppSettings {
    QString activationKey = "F6";   // QKeySequence text, e.g. "F6", "Esc"
    bool holdMode = false;          // false = Toggle
    int mouseButton = 0;            // 0=left, 1=right, 2=middle
    double cps = 20.0;
    bool randomize = false;
    int dutyCycle = 25;             // percent
    bool limitEnabled = false;
    int limit = 100;
    QString iconStyle = "logo1";    // "flat" | "logo1" | "logo2"
    QStringList selectedApps;       // empty = all applications
};

inline ClickerConfig toClickerConfig(const AppSettings& s) {
    ClickerConfig cfg;
    cfg.cps = s.cps;
    cfg.duty_cycle = s.dutyCycle;
    cfg.randomize = s.randomize;
    cfg.click_limit_enabled = s.limitEnabled;
    cfg.click_limit = s.limit;
    cfg.mouse_button = s.mouseButton;
    return cfg;
}
