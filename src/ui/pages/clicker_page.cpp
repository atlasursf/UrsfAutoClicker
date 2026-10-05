#include "ui/pages/clicker_page.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "ui/choose_apps_dialog.h"

ClickerPage::ClickerPage(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);  // so key capture works after pressing SELECT
    buildUi();
}

void ClickerPage::buildUi() {
    auto* main_layout = new QVBoxLayout(this);

    // ============ Activation ============
    auto* activation_group = new QGroupBox("Activation", this);
    auto* activation_layout = new QVBoxLayout();

    auto* key_layout = new QHBoxLayout();
    key_layout->addWidget(new QLabel("Activation Key:"));
    activation_label_ = new QLabel(activation_key_);
    activation_label_->setStyleSheet("border: 1px solid gray; padding: 5px; font-weight: bold;");
    activation_label_->setAlignment(Qt::AlignCenter);
    activation_label_->setFixedWidth(180);
    key_layout->addWidget(activation_label_);
    select_button_ = new QPushButton("SELECT...");
    connect(select_button_, &QPushButton::clicked, this, &ClickerPage::startKeySelection);
    key_layout->addWidget(select_button_);
    key_layout->addStretch();
    activation_layout->addLayout(key_layout);

    auto* mode_layout = new QHBoxLayout();
    mode_layout->addWidget(new QLabel("Activation Mode:"));
    auto* mode_group = new QButtonGroup(this);
    hold_radio_ = new QRadioButton("Hold");
    toggle_radio_ = new QRadioButton("Toggle");
    toggle_radio_->setChecked(true);
    mode_group->addButton(hold_radio_, 0);
    mode_group->addButton(toggle_radio_, 1);
    mode_layout->addWidget(hold_radio_);
    mode_layout->addWidget(toggle_radio_);
    choose_apps_button_ = new QPushButton("CHOOSE APPS");
    connect(choose_apps_button_, &QPushButton::clicked, this, &ClickerPage::chooseApps);
    mode_layout->addWidget(choose_apps_button_);
    mode_layout->addStretch();
    activation_layout->addLayout(mode_layout);

    app_status_label_ = new QLabel("Applications: All");
    app_status_label_->setStyleSheet("color: gray;");
    activation_layout->addWidget(app_status_label_);

    activation_group->setLayout(activation_layout);
    main_layout->addWidget(activation_group);

    // ============ Clicks ============
    auto* clicks_group = new QGroupBox("Clicks", this);
    auto* clicks_layout = new QVBoxLayout();

    auto* button_group = new QButtonGroup(this);
    left_radio_ = new QRadioButton("Left Mouse Button");
    left_radio_->setChecked(true);
    right_radio_ = new QRadioButton("Right Mouse Button");
    middle_radio_ = new QRadioButton("Middle Mouse Button");
    button_group->addButton(left_radio_, 0);
    button_group->addButton(right_radio_, 1);
    button_group->addButton(middle_radio_, 2);

    auto* button_layout = new QHBoxLayout();
    button_layout->addWidget(left_radio_);
    button_layout->addWidget(right_radio_);
    button_layout->addWidget(middle_radio_);
    button_layout->addStretch();
    clicks_layout->addLayout(button_layout);

    clicks_group->setLayout(clicks_layout);
    main_layout->addWidget(clicks_group);

    // ============ Click Rate ============
    auto* rate_group = new QGroupBox("Click Rate", this);
    auto* rate_layout = new QVBoxLayout();

    auto* cps_layout = new QHBoxLayout();
    cps_layout->addWidget(new QLabel("Clicks per second:"));
    cps_spin_ = new QDoubleSpinBox();
    cps_spin_->setMinimum(1);
    cps_spin_->setMaximum(100);
    cps_spin_->setValue(20);
    cps_spin_->setDecimals(1);
    cps_spin_->setFixedWidth(80);
    cps_layout->addWidget(cps_spin_);
    cps_layout->addStretch();
    rate_layout->addLayout(cps_layout);

    randomize_check_ = new QCheckBox("Randomize timing");
    rate_layout->addWidget(randomize_check_);

    auto* duty_layout = new QHBoxLayout();
    duty_layout->addWidget(new QLabel("Click duty cycle:"));
    duty_spin_ = new QSpinBox();
    duty_spin_->setMinimum(5);
    duty_spin_->setMaximum(95);
    duty_spin_->setValue(25);
    duty_spin_->setFixedWidth(80);
    duty_layout->addWidget(duty_spin_);
    duty_layout->addWidget(new QLabel("%"));
    duty_layout->addStretch();
    rate_layout->addLayout(duty_layout);

    auto* rate_hint = new QLabel("1–100 CPS");
    rate_hint->setStyleSheet("color: gray;");
    rate_layout->addWidget(rate_hint);

    rate_group->setLayout(rate_layout);
    main_layout->addWidget(rate_group);

    // ============ Click Limit ============
    auto* limit_group = new QGroupBox("Click Limit", this);
    auto* limit_layout = new QVBoxLayout();

    limit_check_ = new QCheckBox("Enable Click Limit");
    limit_layout->addWidget(limit_check_);

    limit_spin_ = new QSpinBox();
    limit_spin_->setMinimum(1);
    limit_spin_->setMaximum(1000000);
    limit_spin_->setValue(100);
    limit_spin_->setEnabled(false);
    limit_layout->addWidget(limit_spin_);
    connect(limit_check_, &QCheckBox::toggled, limit_spin_, &QWidget::setEnabled);

    auto* limit_hint = new QLabel("Maximum number of clicks");
    limit_hint->setStyleSheet("color: gray;");
    limit_layout->addWidget(limit_hint);

    limit_group->setLayout(limit_layout);
    main_layout->addWidget(limit_group);

    // ============ Status ============
    main_layout->addStretch();
    status_label_ = new QLabel();
    status_label_->setStyleSheet("font-weight: bold; font-size: 10pt;");
    main_layout->addWidget(status_label_);

    // ============ Live-change notifications ============
    connect(cps_spin_, &QDoubleSpinBox::valueChanged, this, [this](double) { notifyChanged(); });
    connect(duty_spin_, &QSpinBox::valueChanged, this, [this](int) { notifyChanged(); });
    connect(limit_spin_, &QSpinBox::valueChanged, this, [this](int) { notifyChanged(); });
    connect(randomize_check_, &QCheckBox::toggled, this, [this](bool) { notifyChanged(); });
    connect(limit_check_, &QCheckBox::toggled, this, [this](bool) { notifyChanged(); });

    for (QRadioButton* radio : {left_radio_, right_radio_, middle_radio_}) {
        connect(radio, &QRadioButton::toggled, this, [this](bool checked) {
            if (checked) notifyChanged();
        });
    }

    connect(hold_radio_, &QRadioButton::toggled, this, [this](bool) {
        updateStatus();
        notifyChanged();
    });

    updateStatus();
}

void ClickerPage::loadFrom(const AppSettings& s) {
    loading_ = true;

    activation_key_ = s.activationKey;
    activation_label_->setText(activation_key_);

    (s.holdMode ? hold_radio_ : toggle_radio_)->setChecked(true);
    (s.mouseButton == 1 ? right_radio_ : s.mouseButton == 2 ? middle_radio_ : left_radio_)
        ->setChecked(true);

    // The spin boxes clamp out-of-range values themselves.
    cps_spin_->setValue(s.cps);
    randomize_check_->setChecked(s.randomize);
    duty_spin_->setValue(s.dutyCycle);
    limit_check_->setChecked(s.limitEnabled);
    limit_spin_->setValue(s.limit);

    selected_apps_ = s.selectedApps;
    updateAppStatusLabel();

    loading_ = false;
    updateStatus();
}

void ClickerPage::applyTo(AppSettings& s) const {
    s.activationKey = activation_key_;
    s.holdMode = hold_radio_->isChecked();
    s.mouseButton = left_radio_->isChecked() ? 0 : right_radio_->isChecked() ? 1 : 2;
    s.cps = cps_spin_->value();
    s.randomize = randomize_check_->isChecked();
    s.dutyCycle = duty_spin_->value();
    s.limitEnabled = limit_check_->isChecked();
    s.limit = limit_spin_->value();
    s.selectedApps = selected_apps_;
}

void ClickerPage::setState(ClickerController::State state) {
    state_ = state;
    updateStatus();
}

void ClickerPage::showHotkeyError(const QString& message) {
    status_label_->setText("● Hotkey error: " + message);
}

void ClickerPage::notifyChanged() {
    if (!loading_) emit changed();
}

void ClickerPage::updateAppStatusLabel() {
    if (selected_apps_.isEmpty()) {
        app_status_label_->setText("Applications: All");
    } else {
        app_status_label_->setText(QString("Applications: %1 selected").arg(selected_apps_.size()));
    }
}

// Clicking... / Stopped — limit reached / "Ready — Hold F6" (Hold) /
// "Stopped — Press F6" (Toggle).
void ClickerPage::updateStatus() {
    switch (state_) {
    case ClickerController::State::Clicking:
        status_label_->setText("● Clicking...");
        break;
    case ClickerController::State::StoppedByLimit:
        status_label_->setText("● Stopped — limit reached");
        break;
    case ClickerController::State::Idle:
        status_label_->setText(
            hold_radio_->isChecked()
                ? QString("● Ready — Hold %1").arg(activation_key_)
                : QString("● Stopped — Press %1").arg(activation_key_));
        break;
    }
}

void ClickerPage::startKeySelection() {
    selecting_key_ = true;
    activation_label_->setText("Press a key...");
    select_button_->setEnabled(false);
    setFocus();
    emit keySelectionStarted();
}

void ClickerPage::keyPressEvent(QKeyEvent* event) {
    if (!selecting_key_) {
        QWidget::keyPressEvent(event);
        return;
    }

    const QString name = QKeySequence(event->key()).toString();
    if (name.isEmpty()) return;  // unknown key: keep waiting

    activation_key_ = name;
    activation_label_->setText(name);
    selecting_key_ = false;
    select_button_->setEnabled(true);
    updateStatus();   // "Press F7" / "Hold F7" follows the new key

    emit changed();
    emit keySelectionFinished();
}

void ClickerPage::chooseApps() {
    const auto result = ChooseAppsDialog::pick(selected_apps_, this);
    if (!result) return;
    selected_apps_ = *result;
    updateAppStatusLabel();
    notifyChanged();
}
