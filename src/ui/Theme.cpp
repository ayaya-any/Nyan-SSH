#include "ui/Theme.h"

#include <QApplication>
#include <QPalette>

namespace nyan::ui {

QColor Palette::alpha(const QColor &base, int alpha) const
{
    QColor copy = base;
    copy.setAlpha(alpha);
    return copy;
}

QColor Palette::accentSoft(int alpha) const
{
    return this->alpha(accent, alpha);
}

Theme &Theme::instance()
{
    static Theme theme;
    return theme;
}

Theme::Theme()
{
    rebuild();
}

void Theme::rebuild()
{
    Palette p;
    if (m_mode == ThemeMode::Dark) {
        p.window = QColor(QStringLiteral("#17171B"));
        p.sidebar = QColor(QStringLiteral("#1C1C22"));
        p.surface = QColor(QStringLiteral("#202027"));
        p.surfaceAlt = QColor(QStringLiteral("#26262E"));
        p.surfaceHover = QColor(QStringLiteral("#2E2E38"));
        p.elevated = QColor(QStringLiteral("#2A2A33"));
        p.border = QColor(QStringLiteral("#32323C"));
        p.borderStrong = QColor(QStringLiteral("#474754"));
        p.text = QColor(QStringLiteral("#E8E9EF"));
        p.textMuted = QColor(QStringLiteral("#A6A8B7"));
        p.textFaint = QColor(QStringLiteral("#717281"));
        p.accent = QColor(QStringLiteral("#4C8DFF"));
        p.accentHover = QColor(QStringLiteral("#67A0FF"));
        p.accentPressed = QColor(QStringLiteral("#397BE6"));
        p.onAccent = QColor(QStringLiteral("#FFFFFF"));
        p.success = QColor(QStringLiteral("#4EC9B0"));
        p.warning = QColor(QStringLiteral("#E3C08D"));
        p.danger = QColor(QStringLiteral("#F1707A"));
        p.shadow = QColor(0, 0, 0, 170);
        p.terminalBackground = QColor(QStringLiteral("#151519"));
        p.terminalForeground = QColor(QStringLiteral("#D4D4D4"));
        p.terminalCursor = QColor(QStringLiteral("#7DAFFF"));
        p.terminalSelection = QColor(QStringLiteral("#264F78"));
        p.ansi = {
            QColor(QStringLiteral("#1B1B1F")), QColor(QStringLiteral("#F14C4C")),
            QColor(QStringLiteral("#23D18B")), QColor(QStringLiteral("#DCDCAA")),
            QColor(QStringLiteral("#3B8EEA")), QColor(QStringLiteral("#D670D6")),
            QColor(QStringLiteral("#29B8DB")), QColor(QStringLiteral("#CCCCCC")),
            QColor(QStringLiteral("#6A6A72")), QColor(QStringLiteral("#FF6B6B")),
            QColor(QStringLiteral("#5AF0B3")), QColor(QStringLiteral("#F5F57A")),
            QColor(QStringLiteral("#6CB8FF")), QColor(QStringLiteral("#F08AF0")),
            QColor(QStringLiteral("#5AD4F0")), QColor(QStringLiteral("#FFFFFF")),
        };
    } else {
        p.window = QColor(QStringLiteral("#F2F3F7"));
        p.sidebar = QColor(QStringLiteral("#EAECF2"));
        p.surface = QColor(QStringLiteral("#FFFFFF"));
        p.surfaceAlt = QColor(QStringLiteral("#F6F7FB"));
        p.surfaceHover = QColor(QStringLiteral("#EDF0F6"));
        p.elevated = QColor(QStringLiteral("#FFFFFF"));
        p.border = QColor(QStringLiteral("#DCE0E8"));
        p.borderStrong = QColor(QStringLiteral("#C3C9D6"));
        p.text = QColor(QStringLiteral("#1D222B"));
        p.textMuted = QColor(QStringLiteral("#5B6474"));
        p.textFaint = QColor(QStringLiteral("#8B93A3"));
        p.accent = QColor(QStringLiteral("#0B6BCB"));
        p.accentHover = QColor(QStringLiteral("#2A83DF"));
        p.accentPressed = QColor(QStringLiteral("#0757A6"));
        p.onAccent = QColor(QStringLiteral("#FFFFFF"));
        p.success = QColor(QStringLiteral("#0F7B6C"));
        p.warning = QColor(QStringLiteral("#946A00"));
        p.danger = QColor(QStringLiteral("#C42B1C"));
        p.shadow = QColor(17, 24, 39, 55);
        p.terminalBackground = QColor(QStringLiteral("#FFFFFF"));
        p.terminalForeground = QColor(QStringLiteral("#1F2328"));
        p.terminalCursor = QColor(QStringLiteral("#0B6BCB"));
        p.terminalSelection = QColor(QStringLiteral("#BBD7FB"));
        p.ansi = {
            QColor(QStringLiteral("#000000")), QColor(QStringLiteral("#CD3131")),
            QColor(QStringLiteral("#00BC00")), QColor(QStringLiteral("#949800")),
            QColor(QStringLiteral("#0451A5")), QColor(QStringLiteral("#BC05BC")),
            QColor(QStringLiteral("#0598BC")), QColor(QStringLiteral("#555555")),
            QColor(QStringLiteral("#666666")), QColor(QStringLiteral("#CD3131")),
            QColor(QStringLiteral("#14CE14")), QColor(QStringLiteral("#B5BA00")),
            QColor(QStringLiteral("#0451A5")), QColor(QStringLiteral("#BC05BC")),
            QColor(QStringLiteral("#0598BC")), QColor(QStringLiteral("#A5A5A5")),
        };
    }
    m_palette = p;
}

void Theme::setMode(ThemeMode mode)
{
    if (m_mode == mode) {
        apply();
        return;
    }
    m_mode = mode;
    rebuild();
    apply();
    emit changed();
}

QFont Theme::uiFont(int pointSize, QFont::Weight weight) const
{
    QFont font;
    font.setFamilies({QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI"),
                      QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Noto Sans CJK SC")});
    font.setPointSize(pointSize);
    font.setWeight(weight);
    font.setHintingPreference(QFont::PreferFullHinting);
    return font;
}

QFont Theme::displayFont(int pointSize, QFont::Weight weight) const
{
    QFont font;
    font.setFamilies({QStringLiteral("Segoe UI Variable Display"), QStringLiteral("Segoe UI Semibold"),
                      QStringLiteral("Segoe UI"), QStringLiteral("Microsoft YaHei UI")});
    font.setPointSize(pointSize);
    font.setWeight(weight);
    font.setHintingPreference(QFont::PreferFullHinting);
    return font;
}

QFont Theme::monoFont(int pointSize, const QString &family) const
{
    QFont font;
    QStringList families;
    if (!family.isEmpty())
        families.append(family);
    families << QStringLiteral("Cascadia Mono") << QStringLiteral("Cascadia Code")
             << QStringLiteral("Consolas") << QStringLiteral("Microsoft YaHei UI");
    font.setFamilies(families);
    font.setPointSize(pointSize);
    font.setFixedPitch(true);
    font.setStyleHint(QFont::Monospace);
    font.setHintingPreference(QFont::PreferFullHinting);
    return font;
}

QString Theme::rgba(const QColor &color, int alpha)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(alpha);
}

QString Theme::styleSheet() const
{
    const Palette &p = m_palette;
    QString css = QStringLiteral(R"QSS(
QWidget {
    color: %TEXT%;
}
QMainWindow, QDialog {
    background: %WINDOW%;
}
QWidget#materialDialog {
    background: transparent;
}
QWidget#dialogSurface {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 16px;
}
QWidget#sidebar {
    background: %SIDEBAR%;
}
QWidget#appStatusBar {
    background: %SIDEBAR%;
    border-top: 1px solid %BORDER%;
}
QFrame#hairline {
    background: %BORDER%;
}
QLabel {
    background: transparent;
}
QLabel[role="muted"] {
    color: %TEXT_MUTED%;
}
QLabel[role="faint"] {
    color: %TEXT_FAINT%;
}
QLabel[role="title"] {
    font-size: 19px;
    font-weight: 600;
}
QLabel[role="subtitle"] {
    color: %TEXT_MUTED%;
    font-size: 12px;
}

QTabWidget::pane {
    border: none;
    background: transparent;
}
QTabWidget::tab-bar {
    left: 6px;
}
QTabBar {
    background: transparent;
    qproperty-drawBase: 0;
}
QTabBar::tab {
    background: transparent;
    color: %TEXT_MUTED%;
    padding: 9px 16px 9px 6px;
    margin-right: 4px;
    border: 1px solid transparent;
    border-bottom: 2px solid transparent;
    border-top-left-radius: 8px;
    border-top-right-radius: 8px;
    min-width: 80px;
}
QTabBar::tab:hover {
    background: %SURFACE_ALT%;
    color: %TEXT%;
}
QTabBar::tab:selected {
    background: %SURFACE_ALT%;
    color: %TEXT%;
    border-bottom: 2px solid %ACCENT%;
}

#latencyChip {
    background: %SURFACE_ALT%;
    border: 1px solid %BORDER%;
    border-radius: 10px;
}

QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px 2px 2px 0;
}
QScrollBar::handle:vertical {
    background: %SCROLL_HANDLE%;
    min-height: 32px;
    border-radius: 5px;
}
QScrollBar::handle:vertical:hover {
    background: %SCROLL_HANDLE_HOVER%;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: transparent;
}
QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 0 2px 2px 2px;
}
QScrollBar::handle:horizontal {
    background: %SCROLL_HANDLE%;
    min-width: 32px;
    border-radius: 5px;
}
QScrollBar::handle:horizontal:hover {
    background: %SCROLL_HANDLE_HOVER%;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
    background: transparent;
}

QMenu {
    background: %ELEVATED%;
    border: 1px solid %BORDER%;
    border-radius: 9px;
    padding: 6px;
}
QMenu::item {
    background: transparent;
    color: %TEXT%;
    padding: 8px 28px 8px 12px;
    border-radius: 6px;
}
QMenu::item:selected {
    background: %ACCENT%;
    color: %ON_ACCENT%;
}
QMenu::item:disabled {
    color: %TEXT_FAINT%;
}
QMenu::separator {
    height: 1px;
    background: %BORDER%;
    margin: 5px 8px;
}
QMenu::indicator {
    width: 14px;
    height: 14px;
}

QToolTip {
    background: %ELEVATED%;
    color: %TEXT%;
    border: 1px solid %BORDER_STRONG%;
    border-radius: 6px;
    padding: 5px 8px;
}

QLineEdit, QPlainTextEdit, QTextEdit {
    background: %SURFACE_ALT%;
    color: %TEXT%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 7px 10px;
    selection-background-color: %ACCENT%;
    selection-color: %ON_ACCENT%;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus {
    border: 1px solid %ACCENT%;
}
QLineEdit#mTextFieldEdit {
    background: transparent;
    border: none;
    padding: 0;
    border-radius: 0;
}
QLineEdit#mTextFieldEdit:focus {
    border: none;
}

QComboBox, QSpinBox, QDoubleSpinBox, QDateEdit {
    background: %SURFACE_ALT%;
    color: %TEXT%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 6px 10px;
    min-height: 20px;
    selection-background-color: %ACCENT%;
}
QSpinBox, QDoubleSpinBox {
    /* Keep the value clear of the frame and reserve room for the stepper. */
    padding: 7px 28px 7px 4px;
}
QSpinBox:hover, QDoubleSpinBox:hover {
    border-color: %BORDER_STRONG%;
}
QSpinBox:focus, QDoubleSpinBox:focus {
    border-color: %ACCENT%;
}
QComboBox:hover, QSpinBox:hover {
    border-color: %BORDER_STRONG%;
}
QComboBox:focus, QSpinBox:focus {
    border-color: %ACCENT%;
}
QComboBox::drop-down {
    border: none;
    width: 26px;
}
QComboBox::down-arrow {
    image: url(%CHEVRON_DOWN_ICON%);
    width: 12px;
    height: 12px;
}
QSpinBox::up-arrow {
    image: url(%CHEVRON_UP_ICON%);
    width: 10px;
    height: 10px;
}
QSpinBox::down-arrow {
    image: url(%CHEVRON_DOWN_ICON%);
    width: 10px;
    height: 10px;
}
QComboBox QAbstractItemView {
    background: %ELEVATED%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 4px;
    selection-background-color: %ACCENT%;
    selection-color: %ON_ACCENT%;
    outline: none;
}
QSpinBox::up-button, QSpinBox::down-button {
    width: 18px;
    border: none;
    background: transparent;
}

QCheckBox {
    spacing: 8px;
    color: %TEXT%;
}
QCheckBox::indicator {
    width: 18px;
    height: 18px;
    border: 1px solid %BORDER_STRONG%;
    border-radius: 5px;
    background: %SURFACE_ALT%;
}
QCheckBox::indicator:checked {
    background: %ACCENT%;
    border-color: %ACCENT%;
    image: url(:/icons/check.svg);
}

QPushButton {
    background: %SURFACE_ALT%;
    color: %TEXT%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 7px 16px;
}
QPushButton:hover {
    background: %SURFACE_HOVER%;
}
QPushButton:default {
    background: %ACCENT%;
    color: %ON_ACCENT%;
    border-color: %ACCENT%;
}

QTableView {
    background: %SURFACE%;
    alternate-background-color: %SURFACE_ALT%;
    color: %TEXT%;
    border: none;
    border-radius: 10px;
    gridline-color: transparent;
    selection-background-color: %ACCENT_SOFT%;
    selection-color: %TEXT%;
    outline: none;
}
QTableView::item {
    padding: 5px 8px;
    border: none;
}
QTableView::item:selected {
    background: %ACCENT_SOFT%;
    color: %TEXT%;
}
QHeaderView::section {
    background: %SURFACE_ALT%;
    color: %TEXT_MUTED%;
    border: none;
    border-bottom: 1px solid %BORDER%;
    padding: 7px 8px;
    font-size: 11px;
    font-weight: 600;
}
QTableCornerButton::section {
    background: %SURFACE_ALT%;
    border: none;
}

QSplitter::handle {
    background: transparent;
}
QSplitter::handle:hover {
    background: %ACCENT_SOFT%;
}
QScrollArea {
    background: transparent;
    border: none;
}
QAbstractScrollArea {
    background: transparent;
}
QStatusBar {
    background: %SIDEBAR%;
    color: %TEXT_MUTED%;
    border-top: 1px solid %BORDER%;
}
QStatusBar::item {
    border: none;
}
QProgressBar {
    background: %SURFACE_ALT%;
    border: none;
    border-radius: 4px;
    text-align: center;
    color: %TEXT_MUTED%;
    max-height: 6px;
}
QProgressBar::chunk {
    background: %ACCENT%;
    border-radius: 4px;
}
QToolButton {
    background: transparent;
    border: none;
    border-radius: 7px;
    padding: 5px;
    color: %TEXT_MUTED%;
}
QToolButton:hover {
    background: %SURFACE_HOVER%;
    color: %TEXT%;
}
QDialogButtonBox QPushButton {
    min-width: 76px;
}
QGraphicsView {
    border: none;
    background: transparent;
}
)QSS");

    const auto replace = [&css](const char *token, const QString &value) {
        css.replace(QString::fromLatin1(token), value);
    };
    replace("%WINDOW%", p.window.name());
    replace("%SIDEBAR%", p.sidebar.name());
    replace("%SURFACE%", p.surface.name());
    replace("%SURFACE_ALT%", p.surfaceAlt.name());
    replace("%SURFACE_HOVER%", p.surfaceHover.name());
    replace("%ELEVATED%", p.elevated.name());
    replace("%BORDER_STRONG%", p.borderStrong.name());
    replace("%BORDER%", p.border.name());
    replace("%TEXT_MUTED%", p.textMuted.name());
    replace("%TEXT_FAINT%", p.textFaint.name());
    replace("%TEXT%", p.text.name());
    replace("%ACCENT_SOFT%", rgba(p.accent, 42));
    replace("%ACCENT%", p.accent.name());
    replace("%ON_ACCENT%", p.onAccent.name());
    replace("%DANGER_SOFT%", rgba(p.danger, 46));
    replace("%SCROLL_HANDLE%", rgba(p.textMuted, 90));
    replace("%SCROLL_HANDLE_HOVER%", rgba(p.textMuted, 140));
    replace("%CLOSE_ICON%",
            m_mode == ThemeMode::Dark ? QStringLiteral(":/icons/close_dark.svg")
                                      : QStringLiteral(":/icons/close_light.svg"));
    replace("%CHEVRON_DOWN_ICON%",
            m_mode == ThemeMode::Dark ? QStringLiteral(":/icons/chevron_down_dark.svg")
                                      : QStringLiteral(":/icons/chevron_down_light.svg"));
    replace("%CHEVRON_UP_ICON%",
            m_mode == ThemeMode::Dark ? QStringLiteral(":/icons/chevron_up_dark.svg")
                                      : QStringLiteral(":/icons/chevron_up_light.svg"));
    return css;
}

void Theme::apply()
{
    if (!qApp)
        return;

    QPalette palette;
    palette.setColor(QPalette::Window, m_palette.window);
    palette.setColor(QPalette::WindowText, m_palette.text);
    palette.setColor(QPalette::Base, m_palette.surface);
    palette.setColor(QPalette::AlternateBase, m_palette.surfaceAlt);
    palette.setColor(QPalette::Text, m_palette.text);
    palette.setColor(QPalette::Button, m_palette.surfaceAlt);
    palette.setColor(QPalette::ButtonText, m_palette.text);
    palette.setColor(QPalette::BrightText, m_palette.danger);
    palette.setColor(QPalette::Highlight, m_palette.accent);
    palette.setColor(QPalette::HighlightedText, m_palette.onAccent);
    palette.setColor(QPalette::ToolTipBase, m_palette.elevated);
    palette.setColor(QPalette::ToolTipText, m_palette.text);
    palette.setColor(QPalette::PlaceholderText, m_palette.textFaint);
    palette.setColor(QPalette::Link, m_palette.accent);
    palette.setColor(QPalette::LinkVisited, m_palette.accentHover);
    palette.setColor(QPalette::Light, m_palette.borderStrong);
    palette.setColor(QPalette::Midlight, m_palette.border);
    palette.setColor(QPalette::Dark, m_palette.window);
    palette.setColor(QPalette::Mid, m_palette.border);
    palette.setColor(QPalette::Shadow, m_palette.shadow);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, m_palette.textFaint);
    palette.setColor(QPalette::Disabled, QPalette::Text, m_palette.textFaint);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, m_palette.textFaint);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, m_palette.surfaceHover);
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, m_palette.textFaint);

    qApp->setPalette(palette);
    qApp->setFont(uiFont(10));
    qApp->setStyleSheet(styleSheet());
}

} // namespace nyan::ui
