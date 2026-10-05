#include "ui/choose_apps_dialog.h"

#include <vector>

#include <QAbstractItemView>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

#include "platform/app_filter.h"

std::optional<QStringList> ChooseAppsDialog::pick(const QStringList& current, QWidget* parent) {
    QDialog dialog(parent);
    dialog.setWindowTitle("Choose Applications");
    dialog.resize(380, 420);

    auto* layout = new QVBoxLayout(&dialog);

    auto* hint = new QLabel(
        "When one or more apps are checked, the activation hotkey only "
        "works while one of them is the focused window. Leave none "
        "checked to allow all applications.", &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* list = new QListWidget(&dialog);
    list->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(list);

    const QSet<QString> selected_ids(current.begin(), current.end());

    const std::vector<AppInfo> apps = list_running_apps();
    for (const AppInfo& app : apps) {
        const QString id = QString::fromStdString(app.id);
        auto* item = new QListWidgetItem(
            QString::fromStdString(app.display_name) + "  (" + id + ")");
        item->setData(Qt::UserRole, id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(selected_ids.contains(id) ? Qt::Checked : Qt::Unchecked);
        list->addItem(item);
    }

    if (apps.empty()) {
        auto* item = new QListWidgetItem(
            "No windows detected (unsupported platform/session, or nothing open).");
        item->setFlags(Qt::NoItemFlags);
        list->addItem(item);
    }

    auto* button_layout = new QHBoxLayout();
    auto* clear_button = new QPushButton("Clear (Allow All)", &dialog);
    auto* cancel_button = new QPushButton("Cancel", &dialog);
    auto* ok_button = new QPushButton("OK", &dialog);
    button_layout->addWidget(clear_button);
    button_layout->addStretch();
    button_layout->addWidget(cancel_button);
    button_layout->addWidget(ok_button);
    layout->addLayout(button_layout);

    QObject::connect(clear_button, &QPushButton::clicked, &dialog, [list]() {
        for (int i = 0; i < list->count(); ++i) {
            if (list->item(i)->flags() & Qt::ItemIsUserCheckable) {
                list->item(i)->setCheckState(Qt::Unchecked);
            }
        }
    });
    QObject::connect(ok_button, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(cancel_button, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) return std::nullopt;

    QStringList result;
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem* item = list->item(i);
        if ((item->flags() & Qt::ItemIsUserCheckable) && item->checkState() == Qt::Checked) {
            result << item->data(Qt::UserRole).toString();
        }
    }
    return result;
}
