#pragma once

/**
 * ChooseAppsDialog - checklist of running apps for the hotkey filter.
 */

#include <optional>

#include <QStringList>

class QWidget;

struct ChooseAppsDialog {
    // Returns the new selection (empty list = allow all), or nullopt if cancelled.
    static std::optional<QStringList> pick(const QStringList& current, QWidget* parent);
};
