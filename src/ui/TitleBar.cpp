#include "ui/TitleBar.h"

#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QAbstractButton>
#include <QEnterEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QToolTip>

namespace nyan::ui {
namespace {

enum class ControlKind {
    Minimize,
    Maximize,
    Close,
};

class WindowControlButton : public QAbstractButton {
public:
    WindowControlButton(ControlKind kind, const QString &tooltip, QWidget *parent = nullptr)
        : QAbstractButton(parent)
        , m_kind(kind)
    {
        setToolTip(tooltip);
        setAccessibleName(tooltip);
        setCursor(Qt::ArrowCursor);
        setFocusPolicy(Qt::NoFocus);
        setFixedSize(46, 42);
        connect(&Theme::instance(), &Theme::changed, this, qOverload<>(&QWidget::update));
    }

    void setIconName(const QString &name)
    {
        m_iconName = name;
        update();
    }

    void setToolTipText(const QString &text)
    {
        setToolTip(text);
        setAccessibleName(text);
    }

    QSize sizeHint() const override { return QSize(46, 42); }

protected:
    void enterEvent(QEnterEvent *event) override
    {
        m_hovered = true;
        update();
        QAbstractButton::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        m_hovered = false;
        update();
        QAbstractButton::leaveEvent(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        const Palette &palette = Theme::instance().palette();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const bool dangerous = m_kind == ControlKind::Close;
        QColor background = Qt::transparent;
        if (isDown() && m_hovered)
            background = dangerous ? QColor(0xC42B1C) : palette.surfaceHover.darker(112);
        else if (m_hovered)
            background = dangerous ? QColor(0xE81123) : palette.surfaceHover;

        if (background.alpha() > 0)
            painter.fillRect(rect(), background);

        QColor iconColor = dangerous && m_hovered ? QColor(Qt::white) : palette.text;
        if (m_hovered && !dangerous)
            iconColor = QColor(Qt::white);
        if (!isEnabled())
            iconColor = palette.textFaint;
        const int iconSize = m_kind == ControlKind::Maximize ? 14 : 13;
        const QPixmap icon = Icons::pixmap(m_iconName, iconColor, iconSize, devicePixelRatioF());
        const QRect target((width() - iconSize) / 2, (height() - iconSize) / 2, iconSize, iconSize);
        painter.drawPixmap(target, icon);
    }

private:
    ControlKind m_kind;
    QString m_iconName;
    bool m_hovered = false;
};

} // namespace

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(42);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 0, 0, 0);
    layout->setSpacing(9);

    m_logo = new QLabel(this);
    m_logo->setFixedSize(26, 26);
    m_logo->setPixmap(Icons::logo(24, 2.0));
    m_logo->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(m_logo, 0, Qt::AlignVCenter);

    m_title = new QLabel(QStringLiteral("Nyan SSH"), this);
    m_title->setFont(Theme::instance().displayFont(11, QFont::DemiBold));
    m_title->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(m_title, 0, Qt::AlignVCenter);
    layout->addStretch(1);

    m_controls = new QWidget(this);
    auto *controlsLayout = new QHBoxLayout(m_controls);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(0);

    m_minimize = new WindowControlButton(ControlKind::Minimize, QStringLiteral("最小化"), m_controls);
    static_cast<WindowControlButton *>(m_minimize)->setIconName(QStringLiteral("minimize"));
    controlsLayout->addWidget(m_minimize);

    m_maximize = new WindowControlButton(ControlKind::Maximize, QStringLiteral("最大化"), m_controls);
    static_cast<WindowControlButton *>(m_maximize)->setIconName(QStringLiteral("maximize"));
    controlsLayout->addWidget(m_maximize);

    m_close = new WindowControlButton(ControlKind::Close, QStringLiteral("关闭"), m_controls);
    static_cast<WindowControlButton *>(m_close)->setIconName(QStringLiteral("close"));
    controlsLayout->addWidget(m_close);

    layout->addWidget(m_controls, 0, Qt::AlignTop);

    connect(m_minimize, &QAbstractButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_maximize, &QAbstractButton::clicked, this, &TitleBar::maximizeToggleRequested);
    connect(m_close, &QAbstractButton::clicked, this, &TitleBar::closeRequested);

    connect(&Theme::instance(), &Theme::changed, this, [this] {
        m_title->setFont(Theme::instance().displayFont(11, QFont::DemiBold));
        m_logo->setPixmap(Icons::logo(24, 2.0));
        updateControlIcons();
        update();
    });
}

void TitleBar::setMaximized(bool maximized)
{
    if (m_maximized == maximized)
        return;
    m_maximized = maximized;
    updateControlIcons();
}

void TitleBar::updateControlIcons()
{
    auto *maximize = static_cast<WindowControlButton *>(m_maximize);
    maximize->setIconName(m_maximized ? QStringLiteral("restore") : QStringLiteral("maximize"));
    maximize->setToolTipText(m_maximized ? QStringLiteral("向下还原") : QStringLiteral("最大化"));
}

bool TitleBar::isControlAt(const QPoint &globalPosition) const
{
    for (QAbstractButton *button : {m_minimize, m_maximize, m_close}) {
        if (!button->isVisible())
            continue;
        const QPoint local = button->mapFromGlobal(globalPosition);
        if (button->rect().contains(local))
            return true;
    }
    return false;
}

bool TitleBar::isOnTitleBar(const QPoint &globalPosition) const
{
    return rect().contains(mapFromGlobal(globalPosition));
}

void TitleBar::paintEvent(QPaintEvent *)
{
    const Palette &palette = Theme::instance().palette();
    QPainter painter(this);
    painter.fillRect(rect(), palette.sidebar);

    QLinearGradient gradient(0, 0, width(), 0);
    gradient.setColorAt(0.0, palette.sidebar);
    gradient.setColorAt(0.72, palette.sidebar);
    QColor right = palette.window;
    gradient.setColorAt(1.0, right);
    painter.fillRect(rect(), gradient);

    painter.setPen(QPen(palette.border, 1));
    painter.drawLine(0, height() - 1, width(), height() - 1);
}

} // namespace nyan::ui
