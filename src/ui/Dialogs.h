#pragma once

#include <QString>

class QWidget;

namespace nyan::ui::Dialogs {

bool confirm(QWidget *parent, const QString &title, const QString &message,
             const QString &acceptText = QStringLiteral("确定"), bool danger = false);
void message(QWidget *parent, const QString &title, const QString &message,
             const QString &acceptText = QStringLiteral("知道了"));
QString promptText(QWidget *parent, const QString &title, const QString &label,
                   const QString &initial = QString(), const QString &placeholder = QString(),
                   const QString &iconName = QStringLiteral("edit"));

} // namespace nyan::ui::Dialogs
