/**
 * URSF AutoClicker - Qt6 Frontend
 *
 * Cross-platform UI with system tray icon.
 *
 * Build:
 *   mkdir build && cd build
 *   cmake ..
 *   make
 *
 * Dependencies:
 *   - Qt6 (Core, Gui, Widgets, Network)
 *   - CMake
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include "x11_hotkey.h"
#endif

#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QButtonGroup>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QKeySequence>
#include <QKeyEvent>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QThread>
#include <QProcess>
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

// =========================================================================
// Backend Connection Manager (runs in background thread)
// =========================================================================

class BackendConnection : public QObject {
    Q_OBJECT

public:
    BackendConnection(QObject* parent = nullptr)
        : QObject(parent), socket_(nullptr), backend_process_(nullptr),
        is_clicking_(false), click_count_(0) {
        connect_timer_.setSingleShot(true);
    }

    ~BackendConnection() {
        stop_backend();
    }

    bool start_backend(int port = 9999) {
        // Start backend process if not running
        if (!backend_process_) {
            backend_process_ = new QProcess(this);
            backend_process_->start("./clicker-backend", QStringList() << QString::number(port));

            if (!backend_process_->waitForStarted(3000)) {
                emit error_occurred("Failed to start backend process");
                return false;
            }

            // Small delay for backend to open listening socket
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Try to connect to backend
        port_ = port;
        attempt_connect(0);
        return true;
    }

    void stop_backend() {
        if (backend_process_) {
            backend_process_->terminate();
            backend_process_->waitForFinished(2000);
            backend_process_->kill();
            backend_process_->waitForFinished();
        }
    }

    void ensure_connected() {
        if (!socket_ || socket_->state() != QTcpSocket::ConnectedState) {
            if (socket_) {
                socket_->disconnectFromHost();
                socket_->deleteLater();
                socket_ = nullptr;
            }
            attempt_connect(0);
        }
    }

    // "key=value ..." settings string shared by START and UPDATE.
    // QString::number() is locale-independent, so a Turkish locale can't
    // turn "20.5" into "20,5" and break the backend's parser.
    static QString build_settings(double cps, int duty, const QString& button,
                                  bool randomize, bool limit_enabled, int limit) {
        return QString("cps=%1 duty=%2 button=%3 randomize=%4 limit_enabled=%5 limit=%6")
        .arg(QString::number(cps, 'f', 1))
            .arg(duty)
            .arg(button)
            .arg(randomize ? 1 : 0)
            .arg(limit_enabled ? 1 : 0)
            .arg(limit);
    }

    // Applies changed settings to a running clicker (no restart, no state change).
    void send_update_command(double cps, int duty, const QString& button,
                             bool randomize, bool limit_enabled, int limit) {
        if (!socket_ || socket_->state() != QTcpSocket::ConnectedState) {
            return;  // nothing running on our side that could be updated
        }

        QString cmd = "UPDATE " + build_settings(cps, duty, button, randomize, limit_enabled, limit) + "\n";
        socket_->write(cmd.toUtf8());
        socket_->flush();
    }

    void send_start_command(double cps, int duty, const QString& button,
                            bool randomize, bool limit_enabled, int limit) {
        ensure_connected();

        if (!socket_ || socket_->state() != QTcpSocket::ConnectedState) {
            emit error_occurred("Not connected to backend");
            return;
        }

        QString cmd = "START " + build_settings(cps, duty, button, randomize, limit_enabled, limit) + "\n";

        socket_->write(cmd.toUtf8());
        socket_->flush();

        is_clicking_ = true;
        click_count_ = 0;
        emit status_changed("Clicking...");
        emit icon_changed(true);  // Tray icon: ON
    }

    void send_stop_command() {
        ensure_connected();

        if (!socket_ || socket_->state() != QTcpSocket::ConnectedState) {
            emit error_occurred("Not connected to backend");
            return;
        }

        socket_->write("STOP\n");
        socket_->flush();

        is_clicking_ = false;
        emit status_changed("Stopped");
        emit icon_changed(false);  // Tray icon: OFF
    }

    bool is_clicking() const { return is_clicking_; }
    int get_click_count() const { return click_count_; }

signals:
    void connected();
    void disconnected();
    void error_occurred(const QString& error);
    void status_changed(const QString& status);
    void icon_changed(bool clicking);  // true = ON, false = OFF

private slots:
    void attempt_connect(int attempt) {
        if (!socket_) {
            socket_ = new QTcpSocket(this);
            connect(socket_, &QTcpSocket::connected, this, &BackendConnection::on_connected);
            connect(socket_, &QTcpSocket::disconnected, this, &BackendConnection::on_disconnected);

            // Qt6.7+ compatible error signal handling
            connect(socket_, static_cast<void(QTcpSocket::*)(QAbstractSocket::SocketError)>(&QTcpSocket::errorOccurred),
                    this, &BackendConnection::on_error);

            connect(socket_, &QTcpSocket::readyRead, this, &BackendConnection::on_read);
        }

        socket_->connectToHost(QHostAddress::LocalHost, port_);

        if (attempt < 10) {  // Try for up to 5 seconds
            connect_timer_.singleShot(500, this, [this, attempt]() {
                attempt_connect(attempt + 1);
            });
        }
    }

    void on_connected() {
        emit connected();
        std::cout << "Connected to backend" << std::endl;
    }

    void on_disconnected() {
        emit disconnected();
    }

    void on_error(QAbstractSocket::SocketError err) {
        Q_UNUSED(err);
        // Silently retry
    }

    void on_read() {
        QString response = QString::fromUtf8(socket_->readAll());
        // Process response if needed
    }

private:
    QTcpSocket* socket_;
    QProcess* backend_process_;
    bool is_clicking_;
    int click_count_;
    int port_;
    QTimer connect_timer_;
};

// =========================================================================
// Main Application Window
// =========================================================================

class AutoClickerWindow : public QMainWindow {
    Q_OBJECT

public:
    AutoClickerWindow(QWidget* parent = nullptr)
        : QMainWindow(parent), backend_connection_(nullptr),
        selecting_key_(false), activation_key_(Qt::Key_F6),
        prev_key_state_(false) {
        setWindowTitle("URSF AutoClicker");
        setWindowIcon(QIcon(":/icons/off.png"));

        resize(520, 550);


        // Create backend connection
        backend_connection_ = new BackendConnection(this);
        connect(backend_connection_, &BackendConnection::icon_changed,
                this, &AutoClickerWindow::on_icon_changed);
        connect(backend_connection_, &BackendConnection::status_changed,
                this, &AutoClickerWindow::on_status_changed);
        connect(backend_connection_, &BackendConnection::error_occurred,
                this, &AutoClickerWindow::on_backend_error);

        // Setup UI
        setup_ui();
        setup_tray_icon();
        connect_live_settings();
        load_config();

        // Settings are saved when the app exits, however that happens
        // (window close or tray "Quit", which never triggers closeEvent).
        connect(qApp, &QCoreApplication::aboutToQuit, this, &AutoClickerWindow::save_config);

        // Start backend
        backend_connection_->start_backend(9999);

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
        // Start global hotkey listener (100ms poll) — Windows GetAsyncKeyState
        hotkey_timer_ = new QTimer(this);
        connect(hotkey_timer_, &QTimer::timeout, this, &AutoClickerWindow::check_hotkey);
        hotkey_timer_->start(100);
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

        // ============ Status Label ============
        main_layout->addStretch();
        status_label_ = new QLabel("● Stopped");
        status_label_->setStyleSheet("font-weight: bold; font-size: 10pt;");
        main_layout->addWidget(status_label_);

        central->setLayout(main_layout);
    }

    void setup_tray_icon() {
        tray_icon_ = new QSystemTrayIcon(this);

        // Load icons — fallback to colored squares if files don't exist
        QPixmap on_pixmap(32, 32);
        on_pixmap.fill(Qt::green);
        QPixmap off_pixmap(32, 32);
        off_pixmap.fill(Qt::red);

        icon_on_ = QIcon(on_pixmap);
        icon_off_ = QIcon(off_pixmap);

        tray_icon_->setIcon(icon_off_);

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
                activation_key_ = static_cast<int>(seq[0].key());
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

        selected_apps_ = obj.value("selected_apps").toArray();

        update_idle_status();
    }

    // ------------------------------------------------------------------
    // Hold / Toggle mode
    // ------------------------------------------------------------------

    // "Ready — Hold F6" in Hold mode, "Stopped — Press F6" in Toggle mode.
    void update_idle_status() {
        status_label_->setText(
            hold_radio_->isChecked()
                ? QString("● Ready — Hold %1").arg(activation_key_name_)
                : QString("● Stopped — Press %1").arg(activation_key_name_));
    }

    void start_clicker() {
        backend_connection_->send_start_command(
            cps_spin_->value(),
            duty_spin_->value(),
            current_button_name(),
            randomize_check_->isChecked(),
            limit_check_->isChecked(),
            limit_spin_->value());
    }

    void stop_clicker() {
        backend_connection_->send_stop_command();
        // Only fall back to the idle text if the stop really went through.
        if (!backend_connection_->is_clicking()) {
            update_idle_status();
        }
    }

    // Sends the current UI settings to a running clicker. The original app
    // re-read every control on each timer tick, so changing CPS / randomize /
    // duty cycle / button / limit while clicking took effect immediately.
    void push_live_settings() {
        if (!backend_connection_->is_clicking()) return;

        backend_connection_->send_update_command(
            cps_spin_->value(),
            duty_spin_->value(),
            current_button_name(),
            randomize_check_->isChecked(),
            limit_check_->isChecked(),
            limit_spin_->value());
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
            if (hold_radio_->isChecked() && backend_connection_->is_clicking()) {
                stop_clicker();
            } else if (!backend_connection_->is_clicking()) {
                update_idle_status();
            }
        });
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
        // TODO: Show dialog to choose applications
        QMessageBox::information(this, "Apps Filter", "App filtering not yet implemented in this demo");
    }

    void toggle_clicker() {
        if (backend_connection_->is_clicking()) {
            stop_clicker();
        } else {
            start_clicker();
        }
    }

    // Toggle mode: each press flips the clicker.
    // Hold mode:   clicking only while the key is held down.
    void on_hotkey_pressed() {
        if (hold_radio_->isChecked()) {
            if (!backend_connection_->is_clicking()) start_clicker();
        } else {
            toggle_clicker();
        }
    }

    void on_hotkey_released() {
        if (hold_radio_->isChecked() && backend_connection_->is_clicking()) {
            stop_clicker();
        }
    }

    void on_icon_changed(bool clicking) {
        if (clicking) {
            tray_icon_->setIcon(icon_on_);
            setWindowIcon(icon_on_);
        } else {
            tray_icon_->setIcon(icon_off_);
            setWindowIcon(icon_off_);
        }
    }

    void on_status_changed(const QString& status) {
        status_label_->setText("● " + status);
    }

    void on_backend_error(const QString& error) {
        QMessageBox::warning(this, "Backend Error", error);
    }

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (selecting_key_) {
            QString key_name = QKeySequence(event->key()).toString();
            activation_key_name_ = key_name;
            activation_label_->setText(key_name);
            selecting_key_ = false;
            select_button_->setEnabled(true);
            if (!backend_connection_->is_clicking()) {
                update_idle_status();  // "Press F7" / "Hold F7" follows the new key
            }

#ifdef __linux__
            if (hotkey_listener_) {
                hotkey_listener_->stop();
                hotkey_listener_->set_key_name(x11_key_name(key_name));
                hotkey_listener_->start();
            }
#else
            activation_key_ = event->key();
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
        backend_connection_->stop_backend();
        QMainWindow::closeEvent(event);
    }

private:
    BackendConnection* backend_connection_;
    QSystemTrayIcon* tray_icon_;
    QIcon icon_on_, icon_off_;

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