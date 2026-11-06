#include "ui/Dialogs.h"

#include "ui/Icons.h"
#include "ui/MaterialWidgets.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace nyan::ui::Dialogs {
namespace {

QLabel *makeMessageLabel(const QString &message)
{
    auto *label = new QLabel(message);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setFont(Theme::instance().uiFont(10));
    label->setMinimumWidth(360);
    return label;
}

QWidget *makeIconRow(const QString &icon, const QColor &color, const QString &message)
{
    auto *container = new QWidget;
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    auto *iconLabel = new QLabel;
    iconLabel->setPixmap(Icons::pixmap(icon, color, 24, 2.0));
    iconLabel->setFixedSize(26, 26);
    layout->addWidget(iconLabel, 0, Qt::AlignTop);
    layout->addWidget(makeMessageLabel(message), 1);
    return container;
}

} // namespace

bool confirm(QWidget *parent, const QString &title, const QString &message, const QString &acceptText,
             bool danger)
{
    MDialog dialog(title, parent);
    dialog.setPreferredWidth(480);
    dialog.contentLayout()->addWidget(makeIconRow(danger ? QStringLiteral("alert") : QStringLiteral("info"),
                                                  danger ? Theme::instance().palette().danger
                                                         : Theme::instance().palette().accent,
                                                  message));

    auto *cancel = new MButton(QStringLiteral("取消"), MButton::Variant::Outlined);
    auto *accept = new MButton(acceptText, danger ? MButton::Variant::Danger : MButton::Variant::Filled);
    QObject::connect(cancel, &QAbstractButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(accept, &QAbstractButton::clicked, &dialog, &QDialog::accept);
    dialog.addFooterButton(cancel);
    dialog.addFooterButton(accept);
    accept->setFocus();
    return dialog.exec() == QDialog::Accepted;
}

void message(QWidget *parent, const QString &title, const QString &message, const QString &acceptText)
{
    MDialog dialog(title, parent);
    dialog.setPreferredWidth(480);
    dialog.contentLayout()->addWidget(makeIconRow(QStringLiteral("info"),
                                                  Theme::instance().palette().accent, message));
    auto *accept = new MButton(acceptText, MButton::Variant::Filled);
    QObject::connect(accept, &QAbstractButton::clicked, &dialog, &QDialog::accept);
    dialog.addFooterButton(accept);
    accept->setFocus();
    dialog.exec();
}

QString promptText(QWidget *parent, const QString &title, const QString &label, const QString &initial,
                   const QString &placeholder, const QString &iconName)
{
    MDialog dialog(title, parent);
    dialog.setPreferredWidth(460);

    auto *field = new MTextField(label, iconName);
    field->setText(initial);
    field->setPlaceholder(placeholder);
    field->lineEdit()->selectAll();
    dialog.contentLayout()->addWidget(field);

    auto *cancel = new MButton(QStringLiteral("取消"), MButton::Variant::Outlined);
    auto *accept = new MButton(QStringLiteral("确定"), MButton::Variant::Filled);
    QObject::connect(cancel, &QAbstractButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(field, &MTextField::returnPressed, &dialog, &QDialog::accept);
    QObject::connect(accept, &QAbstractButton::clicked, &dialog, [&] {
        if (field->text().trimmed().isEmpty()) {
            field->setError(true, QStringLiteral("内容不能为空"));
            field->lineEdit()->setFocus();
            return;
        }
        dialog.accept();
    });
    dialog.addFooterButton(cancel);
    dialog.addFooterButton(accept);
    field->lineEdit()->setFocus();

    if (dialog.exec() != QDialog::Accepted)
        return {};
    return field->text().trimmed();
}

} // namespace nyan::ui::Dialogs
