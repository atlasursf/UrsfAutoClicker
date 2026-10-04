#pragma once

/**
 * Dock - sol taraftaki dikey buton çubuğu (Java Dock.java'nın Qt6 uyarlaması)
 *
 * Header-only, Q_OBJECT yok -> moc gerekmez.
 * Kullanım:
 *   Dock* dock = new Dock(this);
 *   dock->addButton("🖱", "Clicker", "Clicker", []{ ... });
 *   dock->addStretch();
 *   dock->addButton("⏻", "Exit", "Exit", []{ ... }, false);
 *   dock->setActive(0);
 */

#include <QWidget>
#include <QVBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QColor>
#include <QFont>
#include <QCursor>
#include <QString>
#include <functional>
#include <vector>

namespace dock_style {
constexpr int kWidth = 90;
constexpr int kButtonHeight = 56;

// Sistem paletini kullan, fallback değerler ile
inline QColor get_bg_color() {
    const QPalette p;
    return p.color(QPalette::Window);  // Sistem arka planı
}

inline QColor get_hover_color() {
    const QPalette p;
    return p.color(QPalette::Midlight);  // Sistem hover rengi
}

inline QColor get_active_color() {
    const QPalette p;
    return p.color(QPalette::Highlight);  // Sistem seçili rengi
}

inline QColor get_border_color() {
    const QPalette p;
    return p.color(QPalette::Dark);  // Sistem koyu kenarlık
}
}  // namespace dock_style

class DockButton : public QWidget {
public:
    DockButton(const QString& emoji, const QString& label, const QString& tooltip,
               QWidget* parent = nullptr)
        : QWidget(parent), emoji_(emoji), label_(label) {
        setToolTip(tooltip);
        setFixedSize(dock_style::kWidth, dock_style::kButtonHeight);
        setCursor(Qt::PointingHandCursor);
    }

    void setActive(bool active) {
        active_ = active;
        update();
    }

    // Tema değiştiğinde (ör. Light -> Dark) renkleri güncelle
    void update_colors() {
        update();
    }

    std::function<void()> on_click;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const QColor bg = active_ ? dock_style::get_active_color()
                          : hovered_ ? dock_style::get_hover_color()
                                     : dock_style::get_bg_color();
        p.fillRect(rect(), bg);

        // Emoji (üstte)
        QFont emoji_font("Segoe UI Emoji", 16);
        p.setFont(emoji_font);

        // Tema'ya göre yazı rengi ayarla (koyu tema -> beyaz, açık tema -> siyah)
        const QPalette p_palette;
        const QColor text_color = p_palette.color(QPalette::WindowText);
        p.setPen(text_color);
        p.drawText(QRect(0, 4, width(), 30), Qt::AlignCenter, emoji_);

        // Etiket (altta)
        QFont label_font = font();
        label_font.setPointSize(8);
        p.setFont(label_font);
        p.setPen(text_color);
        p.drawText(QRect(0, 34, width(), 18), Qt::AlignCenter, label_);

        // Aktif butonda sol çizgi
        if (active_) {
            p.fillRect(0, 0, 3, height(), dock_style::get_active_color().lighter(130));
        }
    }

    void enterEvent(QEnterEvent*) override { hovered_ = true; update(); }
    void leaveEvent(QEvent*) override { hovered_ = false; update(); }

    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint())) {
            if (on_click) on_click();
        }
    }

private:
    QString emoji_;
    QString label_;
    bool hovered_ = false;
    bool active_ = false;
};

class Dock : public QWidget {
public:
    explicit Dock(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedWidth(dock_style::kWidth);
        layout_ = new QVBoxLayout(this);
        layout_->setContentsMargins(0, 20, 0, 20);
        layout_->setSpacing(10);
    }

    // sticky=false: tıklanınca aktif (mavi) olarak kalmaz (ör. Exit butonu).
    // Dönen değer: setActive() için kullanılabilecek buton indeksi.
    int addButton(const QString& emoji, const QString& label, const QString& tooltip,
                  std::function<void()> action, bool sticky = true) {
        auto* btn = new DockButton(emoji, label, tooltip, this);
        const int index = static_cast<int>(buttons_.size());
        btn->on_click = [this, index, sticky, action = std::move(action)]() {
            if (sticky) setActive(index);
            if (action) action();
        };
        buttons_.push_back(btn);
        layout_->addWidget(btn);
        return index;
    }

    void addStretch() { layout_->addStretch(); }

    void setActive(int index) {
        for (int i = 0; i < static_cast<int>(buttons_.size()); ++i) {
            buttons_[i]->setActive(i == index);
        }
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), dock_style::get_bg_color());
        p.fillRect(width() - 1, 0, 1, height(), dock_style::get_border_color());  // sağ kenarlık
    }

private:
    QVBoxLayout* layout_;
    std::vector<DockButton*> buttons_;
};