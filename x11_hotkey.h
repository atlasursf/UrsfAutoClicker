/**
 * X11 Global Hotkey Listener - Header
 *
 * Kept free of any X11/Xlib includes so that Qt's moc (which parses this
 * header) never sees X11's macros (Bool, True, False, None, Status, ...),
 * which otherwise collide with Qt's own identifiers and break moc parsing.
 * All X11 usage lives in x11_hotkey.cpp instead.
 */
#pragma once

#include <QThread>
#include <QString>
#include <atomic>

class X11HotkeyListener : public QThread {
    Q_OBJECT

public:
    explicit X11HotkeyListener(QObject* parent = nullptr);
    ~X11HotkeyListener() override;

    // Must be called before start(). Takes an X11 key name, e.g. "F6", "A", "space".
    void set_key_name(const QString& name);

    void stop();

protected:
    void run() override;

signals:
    void hotkeyPressed();
    void errorOccurred(const QString& message);

private:
    std::atomic<bool> running_;
    QString key_name_;
};
