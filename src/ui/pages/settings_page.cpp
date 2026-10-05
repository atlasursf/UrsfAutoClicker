#include "ui/pages/settings_page.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "ui/icon_theme.h"

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    auto* icon_group = new QGroupBox("Tray Icon", this);
    auto* icon_layout = new QHBoxLayout();
    icon_layout->addWidget(new QLabel("Style:"));

    icon_style_combo_ = new QComboBox();
    for (const IconStyleInfo& s : IconTheme::styles()) {
        icon_style_combo_->addItem(s.label, s.id);
    }
    icon_layout->addWidget(icon_style_combo_);
    icon_layout->addStretch();
    icon_group->setLayout(icon_layout);

    layout->addWidget(icon_group);
    layout->addStretch();

    connect(icon_style_combo_, &QComboBox::currentIndexChanged, this, [this](int) {
        emit changed();
    });
}

void SettingsPage::loadFrom(const AppSettings& s) {
    const QSignalBlocker blocker(icon_style_combo_);
    const int index = icon_style_combo_->findData(s.iconStyle);
    if (index >= 0) icon_style_combo_->setCurrentIndex(index);
}

void SettingsPage::applyTo(AppSettings& s) const {
    s.iconStyle = icon_style_combo_->currentData().toString();
}
