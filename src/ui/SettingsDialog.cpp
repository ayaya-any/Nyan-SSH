#include "ui/SettingsDialog.h"

#include "ui/Theme.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace nyan::ui {
namespace {

QLabel *sectionTitle(const QString &text)
{
    auto *label = new QLabel(text);
    label->setFont(Theme::instance().uiFont(9, QFont::DemiBold));
    label->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::instance().palette().accent.name()));
    return label;
}

QWidget *switchRow(const QString &title, const QString &hint, MSwitch *toggle)
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(12);
    auto *textLayout = new QVBoxLayout;
    textLayout->setSpacing(2);
    auto *titleLabel = new QLabel(title);
    titleLabel->setFont(Theme::instance().uiFont(9.5, QFont::Medium));
    auto *hintLabel = new QLabel(hint);
    hintLabel->setProperty("role", QStringLiteral("muted"));
    hintLabel->setFont(Theme::instance().uiFont(8.5));
    hintLabel->setWordWrap(true);
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(hintLabel);
    layout->addLayout(textLayout, 1);
    layout->addWidget(toggle, 0, Qt::AlignVCenter);
    return row;
}

} // namespace

SettingsDialog::SettingsDialog(QWidget *parent, const AppSettings &current)
    : MDialog(QStringLiteral("设置"), parent)
    , m_settings(current)
{
    setPreferredWidth(560);
    setSubtitle(QStringLiteral("外观、终端与文件管理偏好"));

    contentLayout()->addWidget(sectionTitle(QStringLiteral("外观")));
    auto *themeRow = new QWidget;
    auto *themeLayout = new QHBoxLayout(themeRow);
    themeLayout->setContentsMargins(0, 0, 0, 0);
    auto *themeLabel = new QLabel(QStringLiteral("主题"));
    themeLabel->setFont(Theme::instance().uiFont(9.5));
    themeLayout->addWidget(themeLabel);
    themeLayout->addStretch();
    m_theme = new MSegmentedControl({QStringLiteral("VS 深色"), QStringLiteral("VS 浅色")});
    m_theme->setCurrentIndex(m_settings.theme == ThemeMode::Light ? 1 : 0, false);
    m_theme->setFixedWidth(240);
    themeLayout->addWidget(m_theme);
    contentLayout()->addWidget(themeRow);

    contentLayout()->addWidget(sectionTitle(QStringLiteral("终端")));
    auto *fontRow = new QWidget;
    auto *fontLayout = new QHBoxLayout(fontRow);
    fontLayout->setContentsMargins(0, 0, 0, 0);
    fontLayout->setSpacing(12);
    auto *fontColumn = new QVBoxLayout;
    fontColumn->setSpacing(5);
    fontColumn->addWidget(new QLabel(QStringLiteral("等宽字体")));
    m_fontFamily = new QComboBox;
    QStringList families;
    const QStringList allFamilies = QFontDatabase::families();
    for (const QString &family : allFamilies) {
        if (family.contains(QStringLiteral("Mono"), Qt::CaseInsensitive)
            || family.contains(QStringLiteral("Consolas"), Qt::CaseInsensitive)
            || family.contains(QStringLiteral("Courier"), Qt::CaseInsensitive)
            || family.contains(QStringLiteral("Cascadia"), Qt::CaseInsensitive)) {
            families.append(family);
        }
    }
    if (!m_settings.terminalFontFamily.isEmpty() && !families.contains(m_settings.terminalFontFamily))
        families.prepend(m_settings.terminalFontFamily);
    families.removeDuplicates();
    m_fontFamily->addItems(families);
    m_fontFamily->setCurrentText(m_settings.terminalFontFamily);
    fontColumn->addWidget(m_fontFamily);
    fontLayout->addLayout(fontColumn, 1);

    auto *sizeColumn = new QVBoxLayout;
    sizeColumn->setSpacing(5);
    sizeColumn->addWidget(new QLabel(QStringLiteral("字号")));
    m_fontSize = new QSpinBox;
    m_fontSize->setRange(7, 32);
    m_fontSize->setValue(m_settings.terminalFontSize);
    m_fontSize->setSuffix(QStringLiteral(" pt"));
    m_fontSize->setFixedWidth(110);
    sizeColumn->addWidget(m_fontSize);
    fontLayout->addLayout(sizeColumn);
    contentLayout()->addWidget(fontRow);

    auto *scrollbackRow = new QWidget;
    auto *scrollbackLayout = new QHBoxLayout(scrollbackRow);
    scrollbackLayout->setContentsMargins(0, 0, 0, 0);
    auto *scrollbackText = new QVBoxLayout;
    scrollbackText->setSpacing(2);
    auto *scrollbackTitle = new QLabel(QStringLiteral("回滚缓冲"));
    scrollbackTitle->setFont(Theme::instance().uiFont(9.5, QFont::Medium));
    auto *scrollbackHint = new QLabel(QStringLiteral("保留的历史行数（500 - 100000）"));
    scrollbackHint->setProperty("role", QStringLiteral("muted"));
    scrollbackHint->setFont(Theme::instance().uiFont(8.5));
    scrollbackText->addWidget(scrollbackTitle);
    scrollbackText->addWidget(scrollbackHint);
    scrollbackLayout->addLayout(scrollbackText, 1);
    m_scrollback = new QSpinBox;
    m_scrollback->setRange(500, 100000);
    m_scrollback->setSingleStep(500);
    m_scrollback->setValue(m_settings.scrollbackLines);
    m_scrollback->setFixedWidth(130);
    scrollbackLayout->addWidget(m_scrollback);
    contentLayout()->addWidget(scrollbackRow);

    m_copyOnSelect = new MSwitch;
    m_copyOnSelect->setChecked(m_settings.copyOnSelect);
    contentLayout()->addWidget(switchRow(QStringLiteral("选择即复制"),
                                         QStringLiteral("用鼠标选中终端文本后自动写入剪贴板"),
                                         m_copyOnSelect));

    contentLayout()->addWidget(sectionTitle(QStringLiteral("文件管理")));
    m_confirmDelete = new MSwitch;
    m_confirmDelete->setChecked(m_settings.confirmBeforeDelete);
    contentLayout()->addWidget(switchRow(QStringLiteral("删除前确认"),
                                         QStringLiteral("删除远程文件或文件夹时显示确认对话框"),
                                         m_confirmDelete));
    m_showHidden = new MSwitch;
    m_showHidden->setChecked(m_settings.showHiddenFiles);
    contentLayout()->addWidget(switchRow(QStringLiteral("显示隐藏文件"),
                                         QStringLiteral("在远程文件列表中显示以 . 开头的文件"),
                                         m_showHidden));
    m_autoOpenPanel = new MSwitch;
    m_autoOpenPanel->setChecked(m_settings.autoOpenFilePanel);
    contentLayout()->addWidget(switchRow(QStringLiteral("连接后自动打开文件面板"),
                                         QStringLiteral("登录成功后立即建立 SFTP 会话"),
                                         m_autoOpenPanel));

    contentLayout()->addWidget(sectionTitle(QStringLiteral("应用")));
    m_confirmExit = new MSwitch;
    m_confirmExit->setChecked(m_settings.confirmOnExit);
    contentLayout()->addWidget(switchRow(QStringLiteral("退出前确认"),
                                         QStringLiteral("仍有活动会话时询问是否退出"),
                                         m_confirmExit));

    auto *cancel = new MButton(QStringLiteral("取消"), MButton::Variant::Outlined);
    auto *save = new MButton(QStringLiteral("保存"), MButton::Variant::Filled);
    connect(cancel, &QAbstractButton::clicked, this, &QDialog::reject);
    connect(save, &QAbstractButton::clicked, this, [this] {
        m_settings.theme = m_theme->currentIndex() == 1 ? ThemeMode::Light : ThemeMode::Dark;
        m_settings.terminalFontFamily = m_fontFamily->currentText();
        m_settings.terminalFontSize = m_fontSize->value();
        m_settings.scrollbackLines = m_scrollback->value();
        m_settings.copyOnSelect = m_copyOnSelect->isChecked();
        m_settings.confirmBeforeDelete = m_confirmDelete->isChecked();
        m_settings.showHiddenFiles = m_showHidden->isChecked();
        m_settings.autoOpenFilePanel = m_autoOpenPanel->isChecked();
        m_settings.confirmOnExit = m_confirmExit->isChecked();
        QDialog::accept();
    });
    addFooterButton(cancel);
    addFooterButton(save);
    save->setFocus();
}

} // namespace nyan::ui
