/**
 * URSF AutoClicker - Qt6 Frontend
 *
 * Single-process build: the click engine (clicker_engine.h/.cpp) runs
 * in-process on its own std::thread. No TCP loopback, no child process.
 *
 * Cross-platform UI with system tray icon.
 *
 * Build:
 *   mkdir build && cd build
 *   cmake ..
 *   make
 *
 * Dependencies:
 *   - Qt6 (Core, Gui, Widgets)
 *   - CMake
 */

#include <iostream>
#include <memory>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX  // keep windows.h from macro-defining min/max (breaks std::min/max)
#endif
#include <windows.h>
#elif defined(__linux__)
#include "x11_hotkey.h"
#endif

#include "clicker_engine.h"
#include "app_filter.h"

#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QButtonGroup>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QKeySequence>
#include <QKeyEvent>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDialog>
#include <QScrollArea>
#include <QMap>
#include <QSet>
#include <QAbstractItemView>

// =========================================================================
// Main Application Window
// =========================================================================

class AutoClickerWindow : public QMainWindow {
    Q_OBJECT

public:
    AutoClickerWindow(QWidget* parent = nullptr)
        : QMainWindow(parent),
        selecting_key_(false), activation_key_(qt_key_to_vk(Qt::Key_F6)),
        prev_key_state_(false), ui_thinks_clicking_(false) {
        setWindowTitle("URSF AutoClicker");
        setWindowIcon(QIcon(":/icons/off.png"));

        resize(520, 550);

        // Setup UI
        setup_ui();
        setup_tray_icon();
        connect_live_settings();
        load_config();

        // Settings are saved when the app exits, however that happens
        // (window close or tray "Quit", which never triggers closeEvent).
        connect(qApp, &QCoreApplication::aboutToQuit, this, &AutoClickerWindow::save_config);

        // Polls the engine so the UI notices when a click-limit stop
        // happens on the background thread (nothing else pushes that
        // change to the UI, since there's no socket/signal for it now).
        sync_timer_ = new QTimer(this);
        connect(sync_timer_, &QTimer::timeout, this, &AutoClickerWindow::sync_with_engine);
        sync_timer_->start(150);

#ifdef __linux__
        // Real global hotkey via X11 (works even when window isn't focused)
        hotkey_listener_ = new X11HotkeyListener(this);
        connect(hotkey_listener_, &X11HotkeyListener::hotkeyPressed,
                this, &AutoClickerWindow::on_hotkey_pressed);
        connect(hotkey_listener_, &X11HotkeyListener::hotkeyReleased,
                this, &AutoClickerWindow::on_hotkey_released);
        connect(hotkey_listener_, &X11HotkeyListener::errorOccurred,
                this, [this](const QString& msg) {
                    status_label_->setText("● Hotkey error: " + msg);
                });
        hotkey_listener_->set_key_name(x11_key_name(activation_key_name_));
        hotkey_listener_->start();
#else
        // Start global hotkey listener (10ms poll) — Windows GetAsyncKeyState
        hotkey_timer_ = new QTimer(this);
        connect(hotkey_timer_, &QTimer::timeout, this, &AutoClickerWindow::check_hotkey);
        hotkey_timer_->setTimerType(Qt::PreciseTimer);
        hotkey_timer_->start(10);
#endif
    }

private:
    void setup_ui() {
        QWidget* central = new QWidget(this);
        setCentralWidget(central);

        QVBoxLayout* main_layout = new QVBoxLayout(central);

        // ============ Activation Group ============
        QGroupBox* activation_group = new QGroupBox("Activation", this);
        QVBoxLayout* activation_layout = new QVBoxLayout();

        QHBoxLayout* key_layout = new QHBoxLayout();
        key_layout->addWidget(new QLabel("Activation Key:"));
        activation_label_ = new QLabel("F6");
        activation_label_->setStyleSheet("border: 1px solid gray; padding: 5px; font-weight: bold;");
        activation_label_->setAlignment(Qt::AlignCenter);
        activation_label_->setFixedWidth(180);
        key_layout->addWidget(activation_label_);
        select_button_ = new QPushButton("SELECT...");
        connect(select_button_, &QPushButton::clicked, this, &AutoClickerWindow::on_select_key);
        key_layout->addWidget(select_button_);
        key_layout->addStretch();
        activation_layout->addLayout(key_layout);

        QHBoxLayout* mode_layout = new QHBoxLayout();
        mode_layout->addWidget(new QLabel("Activation Mode:"));
        QButtonGroup* mode_group = new QButtonGroup(this);
        hold_radio_ = new QRadioButton("Hold");
        toggle_radio_ = new QRadioButton("Toggle");
        toggle_radio_->setChecked(true);
        mode_group->addButton(hold_radio_, 0);
        mode_group->addButton(toggle_radio_, 1);
        mode_layout->addWidget(hold_radio_);
        mode_layout->addWidget(toggle_radio_);
        choose_apps_button_ = new QPushButton("CHOOSE APPS");
        connect(choose_apps_button_, &QPushButton::clicked, this, &AutoClickerWindow::on_choose_apps);
        mode_layout->addWidget(choose_apps_button_);
        mode_layout->addStretch();
        activation_layout->addLayout(mode_layout);

        app_status_label_ = new QLabel("Applications: All");
        app_status_label_->setStyleSheet("color: gray;");
        activation_layout->addWidget(app_status_label_);

        activation_group->setLayout(activation_layout);
        main_layout->addWidget(activation_group);

        // ============ Clicks Group ============
        QGroupBox* clicks_group = new QGroupBox("Clicks", this);
        QVBoxLayout* clicks_layout = new QVBoxLayout();

        QButtonGroup* button_group = new QButtonGroup(this);
        left_radio_ = new QRadioButton("Left Mouse Button");
        left_radio_->setChecked(true);
        right_radio_ = new QRadioButton("Right Mouse Button");
        middle_radio_ = new QRadioButton("Middle Mouse Button");
        button_group->addButton(left_radio_, 0);
        button_group->addButton(right_radio_, 1);
        button_group->addButton(middle_radio_, 2);

        QHBoxLayout* button_layout = new QHBoxLayout();
        button_layout->addWidget(left_radio_);
        button_layout->addWidget(right_radio_);
        button_layout->addWidget(middle_radio_);
        button_layout->addStretch();
        clicks_layout->addLayout(button_layout);

        clicks_group->setLayout(clicks_layout);
        main_layout->addWidget(clicks_group);

        // ============ Click Rate Group ============
        QGroupBox* rate_group = new QGroupBox("Click Rate", this);
        QVBoxLayout* rate_layout = new QVBoxLayout();

        QHBoxLayout* cps_layout = new QHBoxLayout();
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

        QHBoxLayout* duty_layout = new QHBoxLayout();
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

        QLabel* rate_hint = new QLabel("1–100 CPS");
        rate_hint->setStyleSheet("color: gray;");
        rate_layout->addWidget(rate_hint);

        rate_group->setLayout(rate_layout);
        main_layout->addWidget(rate_group);

        // ============ Click Limit Group ============
        QGroupBox* limit_group = new QGroupBox("Click Limit", this);
        QVBoxLayout* limit_layout = new QVBoxLayout();

        limit_check_ = new QCheckBox("Enable Click Limit");
        connect(limit_check_, &QCheckBox::toggled, this, [this](bool checked) {
            limit_spin_->setEnabled(checked);
        });
        limit_layout->addWidget(limit_check_);

        limit_spin_ = new QSpinBox();
        limit_spin_->setMinimum(1);
        limit_spin_->setMaximum(1000000);
        limit_spin_->setValue(100);
        limit_spin_->setEnabled(false);
        limit_layout->addWidget(limit_spin_);

        QLabel* limit_hint = new QLabel("Maximum number of clicks");
        limit_hint->setStyleSheet("color: gray;");
        limit_layout->addWidget(limit_hint);

        limit_group->setLayout(limit_layout);
        main_layout->addWidget(limit_group);

        // ============ Tray Icon Group ============
        QGroupBox* icon_group = new QGroupBox("Tray Icon", this);
        QHBoxLayout* icon_layout = new QHBoxLayout();

        icon_layout->addWidget(new QLabel("Style:"));
        icon_style_combo_ = new QComboBox();
        icon_style_combo_->addItem("Flat Color", "flat");
        icon_style_combo_->addItem("Logo 1", "logo1");
        icon_style_combo_->addItem("Logo 2", "logo2");
        icon_layout->addWidget(icon_style_combo_);
        icon_layout->addStretch();
        connect(icon_style_combo_, &QComboBox::currentIndexChanged, this, [this](int) {
            apply_icon_style(icon_style_combo_->currentData().toString());
        });

        icon_group->setLayout(icon_layout);
        main_layout->addWidget(icon_group);

        // ============ Status Label ============
        main_layout->addStretch();
        status_label_ = new QLabel("● Stopped");
        status_label_->setStyleSheet("font-weight: bold; font-size: 10pt;");
        main_layout->addWidget(status_label_);

        central->setLayout(main_layout);
    }

    // Loads the "flat"/"logo1"/"logo2" icon pairs once. apply_icon_style()
    // then just picks between these, so switching styles never touches disk.
    void load_icon_sets() {
        QPixmap on_pixmap(32, 32);
        on_pixmap.fill(Qt::green);
        QPixmap off_pixmap(32, 32);
        off_pixmap.fill(Qt::red);
        flat_on_ = QIcon(on_pixmap);
        flat_off_ = QIcon(off_pixmap);

        logo1_on_ = QIcon(":/icons/on.png");
        logo1_off_ = QIcon(":/icons/off.png");
        if (logo1_on_.isNull() || logo1_off_.isNull()) {
            logo1_on_ = flat_on_;
            logo1_off_ = flat_off_;
        }

        logo2_on_ = QIcon(":/icons/on2.png");
        logo2_off_ = QIcon(":/icons/off2.png");
        if (logo2_on_.isNull() || logo2_off_.isNull()) {
            logo2_on_ = flat_on_;
            logo2_off_ = flat_off_;
        }
    }

    // Switches which pair icon_on_/icon_off_ point at, then refreshes
    // whatever is currently shown (tray + window icon) to match.
    void apply_icon_style(const QString& style) {
        if (style == "logo2") {
            icon_on_ = logo2_on_;
            icon_off_ = logo2_off_;
        } else if (style == "logo1") {
            icon_on_ = logo1_on_;
            icon_off_ = logo1_off_;
        } else {
            icon_on_ = flat_on_;
            icon_off_ = flat_off_;
        }
        set_icon(ui_thinks_clicking_);
    }

    void setup_tray_icon() {
        tray_icon_ = new QSystemTrayIcon(this);

        load_icon_sets();
        // Set via the combo (not a direct apply_icon_style call) so the
        // dropdown's visible selection matches the icon actually shown,
        // even on a first run where load_config() never touches it.
        icon_style_combo_->setCurrentIndex(icon_style_combo_->findData("logo1"));

        // Tray menu
        QMenu* tray_menu = new QMenu(this);
        QAction* toggle_action = tray_menu->addAction("Toggle Clicker");
        connect(toggle_action, &QAction::triggered, this, &AutoClickerWindow::toggle_clicker);
        tray_menu->addSeparator();
        QAction* quit_action = tray_menu->addAction("Quit");
        connect(quit_action, &QAction::triggered, qApp, &QApplication::quit);

        tray_icon_->setContextMenu(tray_menu);

        connect(tray_icon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::DoubleClick) {
                if (isVisible()) {
                    hide();
                } else {
                    showNormal();
                    activateWindow();
                }
            }
        });

        tray_icon_->show();
    }

    // ------------------------------------------------------------------
    // Config (JSON)
    //   Linux:   ~/.config/UrsfAutoClicker/config.json
    //   macOS:   ~/Library/Preferences/UrsfAutoClicker/config.json
    //   Windows: %LOCALAPPDATA%/UrsfAutoClicker/config.json
    // ------------------------------------------------------------------

    static QString config_path() {
        return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + "/config.json";
    }

    QString current_button_name() const {
        return left_radio_->isChecked() ? "left"
               : right_radio_->isChecked() ? "right"
                                           : "middle";
    }

    void save_config() {
        QJsonObject obj;
        obj["activation_key"] = activation_key_name_;
        obj["hold_mode"] = hold_radio_->isChecked();
        obj["mouse_button"] = current_button_name();
        obj["cps"] = cps_spin_->value();
        obj["randomize"] = randomize_check_->isChecked();
        obj["duty_cycle"] = duty_spin_->value();
        obj["click_limit_enabled"] = limit_check_->isChecked();
        obj["click_limit"] = limit_spin_->value();
        obj["icon_style"] = icon_style_combo_->currentData().toString();
        obj["selected_apps"] = selected_apps_;  // kept as-is for the app filter feature

        const QString path = config_path();
        QDir().mkpath(QFileInfo(path).absolutePath());

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            return;  // like the original: failing to save must never crash the app
        }
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    }

    void load_config() {
        QFile file(config_path());
        if (!file.open(QIODevice::ReadOnly)) {
            update_idle_status();
            return;  // first run: keep the defaults
        }

        QJsonParseError parse_error;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse_error);
        if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) {
            update_idle_status();
            return;  // corrupt file: keep the defaults
        }
        const QJsonObject obj = doc.object();

        const QString key = obj.value("activation_key").toString();
        if (!key.isEmpty()) {
            activation_key_name_ = key;
            activation_label_->setText(key);
            QKeySequence seq = QKeySequence::fromString(key);
            if (!seq.isEmpty()) {
                activation_key_ = qt_key_to_vk(static_cast<int>(seq[0].key()));
            }
        }

        const bool hold = obj.value("hold_mode").toBool(false);
        hold_radio_->setChecked(hold);
        toggle_radio_->setChecked(!hold);

        const QString button = obj.value("mouse_button").toString("left").toLower();
        right_radio_->setChecked(button == "right");
        middle_radio_->setChecked(button == "middle");
        left_radio_->setChecked(button != "right" && button != "middle");

        // The spin boxes clamp out-of-range values to their min/max themselves.
        cps_spin_->setValue(obj.value("cps").toDouble(20.0));
        randomize_check_->setChecked(obj.value("randomize").toBool(false));
        duty_spin_->setValue(obj.value("duty_cycle").toInt(25));
        limit_check_->setChecked(obj.value("click_limit_enabled").toBool(false));
        limit_spin_->setValue(obj.value("click_limit").toInt(100));

        const QString icon_style = obj.value("icon_style").toString("logo1");
        const int style_index = icon_style_combo_->findData(icon_style);
        if (style_index >= 0) {
            icon_style_combo_->setCurrentIndex(style_index);  // emits currentIndexChanged -> apply_icon_style
        }

        selected_apps_ = obj.value("selected_apps").toArray();
        update_app_status_label();

        update_idle_status();
    }

    // ------------------------------------------------------------------
    // Hold / Toggle mode
    // ------------------------------------------------------------------

    void update_app_status_label() {
        if (selected_apps_.isEmpty()) {
            app_status_label_->setText("Applications: All");
        } else {
            app_status_label_->setText(QString("Applications: %1 selected").arg(selected_apps_.size()));
        }
    }

    // Gate for the global hotkey: with no apps chosen, everything is
    // allowed (matches the "Applications: All" default). Otherwise the
    // hotkey only fires while one of the chosen apps is focused. If the
    // platform can't tell what's focused (macOS in this build), we don't
    // block -- better a filter with no effect than one that silently
    // disables the hotkey everywhere.
    bool is_app_allowed() const {
        if (selected_apps_.isEmpty()) return true;

        const std::string active = active_app_identifier();
        if (active.empty()) return true;

        const QString active_id = QString::fromStdString(active);
        for (const QJsonValue& v : selected_apps_) {
            if (v.toString() == active_id) return true;
        }
        return false;
    }

    // "Ready — Hold F6" in Hold mode, "Stopped — Press F6" in Toggle mode.
    void update_idle_status() {
        status_label_->setText(
            hold_radio_->isChecked()
                ? QString("● Ready — Hold %1").arg(activation_key_name_)
                : QString("● Stopped — Press %1").arg(activation_key_name_));
    }

    ClickerConfig build_config() const {
        ClickerConfig cfg;
        cfg.cps = cps_spin_->value();
        cfg.duty_cycle = duty_spin_->value();
        cfg.randomize = randomize_check_->isChecked();
        cfg.click_limit_enabled = limit_check_->isChecked();
        cfg.click_limit = limit_spin_->value();
        cfg.mouse_button = left_radio_->isChecked() ? 0 : right_radio_->isChecked() ? 1 : 2;
        return cfg;
    }

    void set_icon(bool clicking) {
        if (clicking) {
            tray_icon_->setIcon(icon_on_);
            setWindowIcon(icon_on_);
        } else {
            tray_icon_->setIcon(icon_off_);
            setWindowIcon(icon_off_);
        }
    }

    void start_clicker() {
        engine_.start(build_config());
        ui_thinks_clicking_ = true;
        status_label_->setText("● Clicking...");
        set_icon(true);
    }

    void stop_clicker() {
        engine_.stop();
        ui_thinks_clicking_ = false;
        set_icon(false);
        update_idle_status();
    }

    // Sends the current UI settings to a running clicker. The original app
    // re-read every control on each timer tick, so changing CPS / randomize /
    // duty cycle / button / limit while clicking took effect immediately.
    void push_live_settings() {
        if (!engine_.is_enabled()) return;
        engine_.update_config(build_config());
    }

    void connect_live_settings() {
        connect(cps_spin_, &QDoubleSpinBox::valueChanged, this, [this](double) { push_live_settings(); });
        connect(duty_spin_, &QSpinBox::valueChanged, this, [this](int) { push_live_settings(); });
        connect(limit_spin_, &QSpinBox::valueChanged, this, [this](int) { push_live_settings(); });
        connect(randomize_check_, &QCheckBox::toggled, this, [this](bool) { push_live_settings(); });
        connect(limit_check_, &QCheckBox::toggled, this, [this](bool) { push_live_settings(); });

        for (QRadioButton* radio : {left_radio_, right_radio_, middle_radio_}) {
            connect(radio, &QRadioButton::toggled, this, [this](bool checked) {
                if (checked) push_live_settings();
            });
        }

        // Switching modes: refresh the idle text; and, like the original,
        // switching to Hold while clicking stops it (the key isn't held).
        connect(hold_radio_, &QRadioButton::toggled, this, [this](bool) {
            if (hold_radio_->isChecked() && engine_.is_enabled()) {
                stop_clicker();
            } else if (!engine_.is_enabled()) {
                update_idle_status();
            }
        });
    }

    // Qt key codes (Qt::Key_F6 = 0x01000035) are NOT Windows virtual-key
    // codes (VK_F6 = 0x75). GetAsyncKeyState() only understands the latter,
    // so passing a Qt code to it silently never reports a press. Returns 0
    // for keys we can't map (and on non-Windows, where it isn't used).
    static int qt_key_to_vk(int qt_key) {
#ifdef _WIN32
        if (qt_key >= Qt::Key_F1 && qt_key <= Qt::Key_F24) return VK_F1 + (qt_key - Qt::Key_F1);
        if (qt_key >= Qt::Key_A && qt_key <= Qt::Key_Z) return 'A' + (qt_key - Qt::Key_A);
        if (qt_key >= Qt::Key_0 && qt_key <= Qt::Key_9) return '0' + (qt_key - Qt::Key_0);
        switch (qt_key) {
        case Qt::Key_Escape:    return VK_ESCAPE;
        case Qt::Key_Return:
        case Qt::Key_Enter:     return VK_RETURN;
        case Qt::Key_Space:     return VK_SPACE;
        case Qt::Key_Tab:       return VK_TAB;
        case Qt::Key_Backspace: return VK_BACK;
        case Qt::Key_Delete:    return VK_DELETE;
        case Qt::Key_Insert:    return VK_INSERT;
        case Qt::Key_Home:      return VK_HOME;
        case Qt::Key_End:       return VK_END;
        case Qt::Key_PageUp:    return VK_PRIOR;
        case Qt::Key_PageDown:  return VK_NEXT;
        case Qt::Key_Left:      return VK_LEFT;
        case Qt::Key_Right:     return VK_RIGHT;
        case Qt::Key_Up:        return VK_UP;
        case Qt::Key_Down:      return VK_DOWN;
        case Qt::Key_Pause:     return VK_PAUSE;
        case Qt::Key_Print:     return VK_SNAPSHOT;
        case Qt::Key_ScrollLock:return VK_SCROLL;
        case Qt::Key_NumLock:   return VK_NUMLOCK;
        default: break;
        }
#else
        Q_UNUSED(qt_key);
#endif
        return 0;
    }

#ifdef __linux__
    // Qt's QKeySequence text and X11's key names don't always match
    // (e.g. Qt says "Esc", X11 wants "Escape"). Translate the common ones.
    static QString x11_key_name(const QString& qt_name) {
        static const QMap<QString, QString> table = {
                                                      {"Esc", "Escape"},
                                                      {"Return", "Return"},
                                                      {"Enter", "KP_Enter"},
                                                      {"Space", "space"},
                                                      {"Tab", "Tab"},
                                                      {"Backspace", "BackSpace"},
                                                      {"Del", "Delete"},
                                                      {"Ins", "Insert"},
                                                      {"Home", "Home"},
                                                      {"End", "End"},
                                                      {"PgUp", "Prior"},
                                                      {"PgDown", "Next"},
                                                      {"Up", "Up"},
                                                      {"Down", "Down"},
                                                      {"Left", "Left"},
                                                      {"Right", "Right"},
                                                      };
        return table.value(qt_name, qt_name);
    }
#endif

private slots:
    void check_hotkey() {
        if (selecting_key_) return;

#ifdef _WIN32
        // Windows: GetAsyncKeyState
        if (activation_key_ == 0) return;  // unmapped key: nothing to poll
        bool is_pressed = (GetAsyncKeyState(activation_key_) & 0x8000) != 0;

        if (is_pressed && !prev_key_state_) {
            on_hotkey_pressed();
        } else if (!is_pressed && prev_key_state_) {
            on_hotkey_released();
        }
        prev_key_state_ = is_pressed;

#else
        // Linux/macOS: Global hotkey listening requires platform-specific code
        // For now: rely on tray icon toggle (most reliable cross-platform)
        // Full implementation would need:
        // - X11: XGrabKey + XEvent loop
        // - Wayland: D-Bus + systemd-logind
        // - macOS: NSEvent global hotkey
        return;
#endif
    }

    void on_select_key() {
        selecting_key_ = true;
        activation_label_->setText("Press a key...");
        select_button_->setEnabled(false);
    }

    void on_choose_apps() {
        QDialog dialog(this);
        dialog.setWindowTitle("Choose Applications");
        dialog.resize(380, 420);

        QVBoxLayout* layout = new QVBoxLayout(&dialog);

        QLabel* hint = new QLabel(
            "When one or more apps are checked, the activation hotkey only "
            "works while one of them is the focused window. Leave none "
            "checked to allow all applications.", &dialog);
        hint->setWordWrap(true);
        layout->addWidget(hint);

        QListWidget* list = new QListWidget(&dialog);
        list->setSelectionMode(QAbstractItemView::NoSelection);
        layout->addWidget(list);

        QSet<QString> selected_ids;
        for (const QJsonValue& v : selected_apps_) selected_ids.insert(v.toString());

        const std::vector<AppInfo> apps = list_running_apps();
        for (const AppInfo& app : apps) {
            const QString id = QString::fromStdString(app.id);
            QListWidgetItem* item = new QListWidgetItem(
                QString::fromStdString(app.display_name) + "  (" + id + ")");
            item->setData(Qt::UserRole, id);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(selected_ids.contains(id) ? Qt::Checked : Qt::Unchecked);
            list->addItem(item);
        }

        if (apps.empty()) {
            QListWidgetItem* item = new QListWidgetItem(
                "No windows detected (unsupported platform/session, or nothing open).");
            item->setFlags(Qt::NoItemFlags);
            list->addItem(item);
        }

        QHBoxLayout* button_layout = new QHBoxLayout();
        QPushButton* clear_button = new QPushButton("Clear (Allow All)", &dialog);
        QPushButton* cancel_button = new QPushButton("Cancel", &dialog);
        QPushButton* ok_button = new QPushButton("OK", &dialog);
        button_layout->addWidget(clear_button);
        button_layout->addStretch();
        button_layout->addWidget(cancel_button);
        button_layout->addWidget(ok_button);
        layout->addLayout(button_layout);

        connect(clear_button, &QPushButton::clicked, &dialog, [list]() {
            for (int i = 0; i < list->count(); ++i) {
                if (list->item(i)->flags() & Qt::ItemIsUserCheckable) {
                    list->item(i)->setCheckState(Qt::Unchecked);
                }
            }
        });
        connect(ok_button, &QPushButton::clicked, &dialog, &QDialog::accept);
        connect(cancel_button, &QPushButton::clicked, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted) return;

        QJsonArray new_selection;
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem* item = list->item(i);
            if ((item->flags() & Qt::ItemIsUserCheckable) && item->checkState() == Qt::Checked) {
                new_selection.append(item->data(Qt::UserRole).toString());
            }
        }
        selected_apps_ = new_selection;
        update_app_status_label();
    }

    void toggle_clicker() {
        if (engine_.is_enabled()) {
            stop_clicker();
        } else {
            start_clicker();
        }
    }

    // Toggle mode: each press flips the clicker.
    // Hold mode:   clicking only while the key is held down.
    void on_hotkey_pressed() {
        if (!is_app_allowed()) return;  // focused app isn't in the chosen set

        if (hold_radio_->isChecked()) {
            if (!engine_.is_enabled()) start_clicker();
        } else {
            toggle_clicker();
        }
    }

    void on_hotkey_released() {
        if (hold_radio_->isChecked() && engine_.is_enabled()) {
            stop_clicker();
        }
    }

    // Catches the case the old TCP version couldn't: the engine hitting its
    // click limit and stopping itself on the background thread. There's no
    // signal for that anymore (it's all in-process now), so we poll.
    void sync_with_engine() {
        if (ui_thinks_clicking_ && !engine_.is_enabled()) {
            ui_thinks_clicking_ = false;
            set_icon(false);
            status_label_->setText("● Stopped — limit reached");
        }
    }

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (selecting_key_) {
            QString key_name = QKeySequence(event->key()).toString();
            activation_key_name_ = key_name;
            activation_label_->setText(key_name);
            selecting_key_ = false;
            select_button_->setEnabled(true);
            if (!engine_.is_enabled()) {
                update_idle_status();  // "Press F7" / "Hold F7" follows the new key
            }

#ifdef __linux__
            if (hotkey_listener_) {
                hotkey_listener_->stop();
                hotkey_listener_->set_key_name(x11_key_name(key_name));
                hotkey_listener_->start();
            }
#else
            activation_key_ = event->nativeVirtualKey() != 0
                                  ? static_cast<int>(event->nativeVirtualKey())
                                  : qt_key_to_vk(event->key());
#endif
            return;
        }
        QMainWindow::keyPressEvent(event);
    }

    void closeEvent(QCloseEvent* event) override {
#ifdef __linux__
        if (hotkey_listener_) {
            hotkey_listener_->stop();
        }
#endif
        engine_.stop();
        QMainWindow::closeEvent(event);
    }

private:
    ClickerEngine engine_;
    QTimer* sync_timer_;
    bool ui_thinks_clicking_;

    QSystemTrayIcon* tray_icon_;
    QComboBox* icon_style_combo_;
    QIcon icon_on_, icon_off_;              // currently selected pair (what set_icon() shows)
    QIcon flat_on_, flat_off_;
    QIcon logo1_on_, logo1_off_;
    QIcon logo2_on_, logo2_off_;

    QLabel* activation_label_;
    QPushButton* select_button_;
    QRadioButton* hold_radio_;
    QRadioButton* toggle_radio_;
    QPushButton* choose_apps_button_;
    QLabel* app_status_label_;

    QRadioButton* left_radio_;
    QRadioButton* right_radio_;
    QRadioButton* middle_radio_;

    QDoubleSpinBox* cps_spin_;
    QCheckBox* randomize_check_;
    QSpinBox* duty_spin_;

    QCheckBox* limit_check_;
    QSpinBox* limit_spin_;

    QLabel* status_label_;

    bool selecting_key_;
    int activation_key_;
    QString activation_key_name_ = "F6";   // shown in the UI, saved to config, used for the X11 grab
    QJsonArray selected_apps_;             // app filter list (preserved in config)
    bool prev_key_state_;
    QTimer* hotkey_timer_;
#ifdef __linux__
    X11HotkeyListener* hotkey_listener_ = nullptr;
#endif
};

// =========================================================================
// Main Entry Point
// =========================================================================

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("UrsfAutoClicker");  // config dir name

    AutoClickerWindow window;
    window.show();

    return app.exec();
}

#include "frontend.moc"