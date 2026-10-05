#include "ui/icon_theme.h"

#include <QPixmap>

IconTheme::IconTheme() {
    QPixmap on_pixmap(32, 32);
    on_pixmap.fill(Qt::green);
    QPixmap off_pixmap(32, 32);
    off_pixmap.fill(Qt::red);
    const Pair flat{QIcon(on_pixmap), QIcon(off_pixmap)};
    pairs_.insert("flat", flat);

    // Falls back to the flat pair when the resource files are missing.
    auto load = [&flat](const char* on_path, const char* off_path) {
        Pair p{QIcon(on_path), QIcon(off_path)};
        return (p.on.isNull() || p.off.isNull()) ? flat : p;
    };
    pairs_.insert("logo1", load(":/icons/on.png", ":/icons/off.png"));
    pairs_.insert("logo2", load(":/icons/on2.png", ":/icons/off2.png"));
}

QList<IconStyleInfo> IconTheme::styles() {
    return {
        {"flat", "Flat Color"},
        {"logo1", "Logo 1"},
        {"logo2", "Logo 2"},
    };
}

QString IconTheme::normalize(const QString& style_id) {
    for (const IconStyleInfo& s : styles()) {
        if (s.id == style_id) return style_id;
    }
    return "logo1";
}

QIcon IconTheme::icon(const QString& style_id, bool on) const {
    const Pair p = pairs_.value(normalize(style_id));
    return on ? p.on : p.off;
}
