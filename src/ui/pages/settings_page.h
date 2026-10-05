#pragma once

/**
 * SettingsPage - application-level settings (currently: tray icon style).
 */

#include <QWidget>

#include "core/app_settings.h"

class QComboBox;

class SettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget* parent = nullptr);

    void loadFrom(const AppSettings& settings);   // does not emit changed()
    void applyTo(AppSettings& settings) const;    // writes only the fields this page owns

signals:
    void changed();

private:
    QComboBox* icon_style_combo_;
};
