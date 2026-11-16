#include "ui/SettingsDialog.h"

#include "ui/Theme.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace nyan::ui {
namespace {

/// QSpinBox's stylesheet padding does not move the internal line edit text on
/// all styles.  Set the editor's text margins directly so the value keeps a
/// stable inset from the frame at every DPI.
class SettingsSpinBox : public QSpinBox {
public:
    explicit SettingsSpinBox(QWidget *parent = nullptr)
        : QSpinBox(parent)
    {
        lineEdit()->setTextMargins(12, 0, 4, 0);
        lineEdit()->setStyleSheet(
            QStringLiteral("QLineEdit { background: transparent; border: none; padding: 0; }"));
    }
};

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
    contentLayout()->setSpacing(10);

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

    auto *opacityRow = new QWidget;
    auto *opacityLayout = new QHBoxLayout(opacityRow);
    opacityLayout->setContentsMargins(0, 0, 0, 0);
    opacityLayout->setSpacing(12);
    auto *opacityLabel = new QLabel(QStringLiteral("窗口透明度"));
    opacityLabel->setFont(Theme::instance().uiFont(9.5));
    opacityLayout->addWidget(opacityLabel);
    opacityLayout->addStretch();
    m_opacity = new QSlider(Qt::Horizontal);
    m_opacity->setRange(55, 100);
    m_opacity->setSingleStep(1);
    m_opacity->setPageStep(5);
    m_opacity->setFixedWidth(220);
    m_opacity->setValue(m_settings.windowOpacity);
    m_opacity->setToolTip(QStringLiteral("调整整个主窗口的不透明度（55% - 100%）"));
    opacityLayout->addWidget(m_opacity);
    m_opacityValue = new QLabel(QStringLiteral("%1%").arg(m_opacity->value()));
    m_opacityValue->setFixedWidth(42);
    m_opacityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_opacityValue->setFont(Theme::instance().uiFont(8.5));
    opacityLayout->addWidget(m_opacityValue);
    contentLayout()->addWidget(opacityRow);

    connect(m_opacity, &QSlider::valueChanged, this, [this](int value) {
        m_opacityValue->setText(QStringLiteral("%1%").arg(value));
    });

    auto *backgroundRow = new QWidget;
    auto *backgroundLayout = new QHBoxLayout(backgroundRow);
    backgroundLayout->setContentsMargins(0, 0, 0, 0);
    backgroundLayout->setSpacing(8);
    auto *backgroundLabel = new QLabel(QStringLiteral("背景图"));
    backgroundLabel->setFont(Theme::instance().uiFont(9.5));
    backgroundLayout->addWidget(backgroundLabel);
    m_backgroundImage = m_settings.backgroundImage;
    m_backgroundPath = new QLabel;
    m_backgroundPath->setProperty("role", QStringLiteral("muted"));
    m_backgroundPath->setFont(Theme::instance().uiFont(8.5));
    m_backgroundPath->setMinimumWidth(120);
    m_backgroundPath->setMaximumWidth(220);
    backgroundLayout->addWidget(m_backgroundPath, 1);
    auto *chooseBackground = new MButton(QStringLiteral("选择图片"), MButton::Variant::Outlined);
    chooseBackground->setCompact(true);
    backgroundLayout->addWidget(chooseBackground);
    auto *clearBackground = new MButton(QStringLiteral("清除"), MButton::Variant::Text);
    clearBackground->setCompact(true);
    backgroundLayout->addWidget(clearBackground);
    contentLayout()->addWidget(backgroundRow);

    auto updateBackgroundLabel = [this, clearBackground] {
        const QString name = m_backgroundImage.isEmpty()
                                 ? QStringLiteral("未设置")
                                 : QFileInfo(m_backgroundImage).fileName();
        m_backgroundPath->setText(name);
        m_backgroundPath->setToolTip(m_backgroundImage);
        clearBackground->setEnabled(!m_backgroundImage.isEmpty());
    };
    updateBackgroundLabel();
    connect(chooseBackground, &QAbstractButton::clicked, this, [this, updateBackgroundLabel] {
        const QString start = m_backgroundImage.isEmpty()
                                  ? QString()
                                  : QFileInfo(m_backgroundImage).absolutePath();
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择背景图"), start,
            QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (path.isEmpty())
            return;
        m_backgroundImage = path;
        updateBackgroundLabel();
    });
    connect(clearBackground, &QAbstractButton::clicked, this, [this, updateBackgroundLabel] {
        m_backgroundImage.clear();
        updateBackgroundLabel();
    });

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
    QFontDatabase database;
    const QStringList allFamilies = QFontDatabase::families();
    for (const QString &family : allFamilies) {
        if (database.isFixedPitch(family))
            families.append(family);
    }
    std::sort(families.begin(), families.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    if (families.isEmpty())
        families << QStringLiteral("Cascadia Mono") << QStringLiteral("Consolas");
    if (!m_settings.terminalFontFamily.isEmpty()
        && !families.contains(m_settings.terminalFontFamily)) {
        families.prepend(m_settings.terminalFontFamily);
    }
    families.removeDuplicates();
    m_fontFamily->addItems(families);
    m_fontFamily->setCurrentText(m_settings.terminalFontFamily);
    m_fontFamily->setToolTip(QStringLiteral("仅列出系统检测为等宽（fixed pitch）的字体"));
    fontColumn->addWidget(m_fontFamily);
    fontLayout->addLayout(fontColumn, 1);

    auto *sizeColumn = new QVBoxLayout;
    sizeColumn->setSpacing(5);
    sizeColumn->addWidget(new QLabel(QStringLiteral("字号")));
    m_fontSize = new SettingsSpinBox;
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
    m_scrollback = new SettingsSpinBox;
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
        m_settings.windowOpacity = m_opacity->value();
        m_settings.backgroundImage = m_backgroundImage;
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
}

} // namespace nyan::ui


