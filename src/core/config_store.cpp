#include "core/config_store.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

QString ConfigStore::path() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + "/config.json";
}

AppSettings ConfigStore::load() {
    AppSettings s;

    QFile file(path());
    if (!file.open(QIODevice::ReadOnly)) return s;  // first run

    QJsonParseError parse_error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) return s;  // corrupt
    const QJsonObject obj = doc.object();

    const QString key = obj.value("activation_key").toString();
    if (!key.isEmpty()) s.activationKey = key;

    s.holdMode = obj.value("hold_mode").toBool(false);

    const QString button = obj.value("mouse_button").toString("left").toLower();
    s.mouseButton = button == "right" ? 1 : button == "middle" ? 2 : 0;

    s.cps = std::clamp(obj.value("cps").toDouble(20.0), 1.0, 100.0);
    s.randomize = obj.value("randomize").toBool(false);
    s.dutyCycle = std::clamp(obj.value("duty_cycle").toInt(25), 5, 95);
    s.limitEnabled = obj.value("click_limit_enabled").toBool(false);
    s.limit = std::clamp(obj.value("click_limit").toInt(100), 1, 1000000);
    s.iconStyle = obj.value("icon_style").toString("logo1");

    for (const QJsonValue& v : obj.value("selected_apps").toArray()) {
        s.selectedApps << v.toString();
    }
    return s;
}

bool ConfigStore::save(const AppSettings& s) {
    QJsonObject obj;
    obj["activation_key"] = s.activationKey;
    obj["hold_mode"] = s.holdMode;
    obj["mouse_button"] = s.mouseButton == 1 ? "right" : s.mouseButton == 2 ? "middle" : "left";
    obj["cps"] = s.cps;
    obj["randomize"] = s.randomize;
    obj["duty_cycle"] = s.dutyCycle;
    obj["click_limit_enabled"] = s.limitEnabled;
    obj["click_limit"] = s.limit;
    obj["icon_style"] = s.iconStyle;
    obj["selected_apps"] = QJsonArray::fromStringList(s.selectedApps);

    const QString p = path();
    QDir().mkpath(QFileInfo(p).absolutePath());

    QFile file(p);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}
