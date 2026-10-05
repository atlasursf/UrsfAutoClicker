#pragma once

/**
 * IconTheme - the "flat" / "logo1" / "logo2" icon pairs (on/off).
 * Everything is loaded once in the constructor, so switching styles
 * never touches the disk. Needs a QGuiApplication to exist.
 */

#include <QHash>
#include <QIcon>
#include <QList>
#include <QString>

struct IconStyleInfo {
    QString id;     // stored in config
    QString label;  // shown in the UI
};

class IconTheme {
public:
    IconTheme();

    static QList<IconStyleInfo> styles();
    static QString normalize(const QString& styleId);  // unknown -> "logo1"

    QIcon icon(const QString& styleId, bool on) const;

private:
    struct Pair {
        QIcon on, off;
    };
    QHash<QString, Pair> pairs_;
};
