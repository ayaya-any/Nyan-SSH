#include "ui/MaterialWidgets.h"

#include "ui/Icons.h"

#include <QApplication>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QScreen>
#include <QStyleOption>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWindow>

#include <cmath>

namespace nyan::ui {
namespace {

constexpr int kButtonRadius = 9;

QColor blend(const QColor &from, const QColor &to, qreal t)
{
    const qreal clamped = qBound(0.0, t, 1.0);
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * clamped,
                            from.greenF() + (to.greenF() - from.greenF()) * clamped,
                            from.blueF() + (to.blueF() - from.blueF()) * clamped,
                            from.alphaF() + (to.alphaF() - from.alphaF()) * clamped);
}

QColor withAlpha(QColor color, int alpha)
{
    color.setAlpha(alpha);
    return color;
}

} // namespace

// ---------------------------------------------------------------------------
// MButton
// ---------------------------------------------------------------------------

MButton::MButton(const QString &text, Variant variant, QWidget *parent)
    : QAbstractButton(parent)
    , m_variant(variant)
{
    setText(text);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover, false);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    m_rippleAnimation = new QVariantAnimation(this);
    m_rippleAnimation->setDuration(420);
    m_rippleAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_rippleAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_rippleProgress = value.toReal();
        update();
    });
    connect(m_rippleAnimation, &QVariantAnimation::finished, this, [this] {
        m_rippleProgress = 1.0;
        update();
    });
    connect(&Theme::instance(), &Theme::changed, this, qOverload<>(&QWidget::update));
}

void MButton::setVariant(Variant variant)
{
    m_variant = variant;
    update();
}

void MButton::setIconName(const QString &name)
{
    m_iconName = name;
    updateGeometry();
    update();
}

void MButton::setIconSize(int size)
{
    m_iconSize = size;
    updateGeometry();
    update();
}

void MButton::setCompact(bool compact)
{
    m_compact = compact;
    updateGeometry();
    update();
}

void MButton::setText(const QString &text)
{
    QAbstractButton::setText(text);
    setAccessibleName(text);
    updateGeometry();
    update();
}

QSize MButton::sizeHint() const
{
    const QFontMetrics metrics(Theme::instance().uiFont(10, QFont::DemiBold));
    int width = metrics.horizontalAdvance(text()) + (m_compact ? 24 : 36);
    if (!m_iconName.isEmpty())
        width += m_iconSize + (text().isEmpty() ? 0 : 8);
    return QSize(qMax(width, m_iconName.isEmpty() && text().isEmpty() ? 40 : 0),
                 m_compact ? 32 : 38);
}

QSize MButton::minimumSizeHint() const
{
    return sizeHint();
}

QColor MButton::contentColor() const
{
    const Palette &p = Theme::instance().palette();
    if (!isEnabled())
        return p.textFaint;
    switch (m_variant) {
    case Variant::Filled:
    case Variant::Danger:
        return p.onAccent;
    case Variant::Tonal:
    case Variant::Text:
        return m_variant == Variant::Danger && m_variant != Variant::Tonal ? p.danger : p.accent;
    case Variant::Outlined:
        return p.text;
    }
    return p.text;
}

QColor MButton::backgroundColor() const
{
    const Palette &p = Theme::instance().palette();
    if (!isEnabled())
        return m_variant == Variant::Filled || m_variant == Variant::Danger
                   ? withAlpha(p.surfaceHover, 130)
                   : Qt::transparent;
    switch (m_variant) {
    case Variant::Filled:
        return isDown() ? p.accentPressed : p.accent;
    case Variant::Danger:
        return isDown() ? p.danger.darker(115) : p.danger;
    case Variant::Tonal:
        return isDown() ? withAlpha(p.accent, 58) : withAlpha(p.accent, 34);
    case Variant::Outlined:
        return isDown() ? withAlpha(p.accent, 26) : Qt::transparent;
    case Variant::Text:
        return isDown() ? withAlpha(p.accent, 26) : Qt::transparent;
    }
    return Qt::transparent;
}

void MButton::mousePressEvent(QMouseEvent *event)
{
    if (isEnabled() && event->button() == Qt::LeftButton)
        startRipple(event->position());
    QAbstractButton::mousePressEvent(event);
}

void MButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void MButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void MButton::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (isEnabled())
            click();
        event->accept();
        return;
    }
    QAbstractButton::keyPressEvent(event);
}

void MButton::startRipple(const QPointF &position)
{
    m_rippleCenter = position;
    m_rippleRadius = 0.0;
    const QPointF corners[] = {
        QPointF(0, 0), QPointF(width(), 0), QPointF(0, height()), QPointF(width(), height())};
    for (const QPointF &corner : corners)
        m_rippleRadius = qMax(m_rippleRadius, std::hypot(corner.x() - position.x(), corner.y() - position.y()));
    m_rippleProgress = 0.0;
    m_rippleAnimation->stop();
    m_rippleAnimation->setStartValue(0.0);
    m_rippleAnimation->setEndValue(1.0);
    m_rippleAnimation->start();
}

void MButton::paintEvent(QPaintEvent *)
{
    const Palette &p = Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(bounds, kButtonRadius, kButtonRadius);

    if ((m_variant == Variant::Filled || m_variant == Variant::Danger) && isEnabled()) {
        QPainterPath shadowPath;
        shadowPath.addRoundedRect(bounds.translated(0, 2.5), kButtonRadius, kButtonRadius);
        painter.fillPath(shadowPath, withAlpha(m_variant == Variant::Danger ? p.danger : p.accent, 45));
    }

    QColor background = backgroundColor();
    if (m_hovered && isEnabled() && !isDown()) {
        if (m_variant == Variant::Filled || m_variant == Variant::Danger)
            background = background.lighter(112);
        else if (m_variant == Variant::Tonal)
            background = withAlpha(p.accent, 48);
        else if (m_variant == Variant::Outlined)
            background = withAlpha(p.accent, 16);
        else
            background = withAlpha(p.accent, 16);
    }
    if (background.alpha() > 0)
        painter.fillPath(path, background);

    if (m_variant == Variant::Outlined) {
        painter.setPen(QPen(isEnabled() ? p.borderStrong : p.border, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

    if (m_rippleProgress < 1.0 && m_rippleRadius > 0.0) {
        painter.save();
        painter.setClipPath(path);
        const QColor rippleColor = (m_variant == Variant::Filled || m_variant == Variant::Danger)
                                       ? QColor(255, 255, 255, 38)
                                       : withAlpha(p.accent, 34);
        painter.setPen(Qt::NoPen);
        painter.setBrush(rippleColor);
        const qreal radius = m_rippleRadius * m_rippleProgress;
        painter.drawEllipse(m_rippleCenter, radius, radius);
        painter.restore();
    }

    if (hasFocus()) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(withAlpha(m_variant == Variant::Filled || m_variant == Variant::Danger
                                         ? p.onAccent
                                         : p.accent,
                                     150),
                            1.4));
        painter.drawRoundedRect(bounds.adjusted(2, 2, -2, -2), kButtonRadius - 2, kButtonRadius - 2);
    }

    const QColor content = contentColor();
    const QFont font = Theme::instance().uiFont(10, QFont::DemiBold);
    painter.setFont(font);
    const QFontMetrics metrics(font);

    const QString label = metrics.elidedText(text(), Qt::ElideRight, width() - 24);
    const int textWidth = label.isEmpty() ? 0 : metrics.horizontalAdvance(label);
    const int iconWidth = m_iconName.isEmpty() ? 0 : m_iconSize;
    const int gap = (!label.isEmpty() && iconWidth > 0) ? 8 : 0;
    const int total = iconWidth + gap + textWidth;
    int x = (width() - total) / 2;
    const int centerY = height() / 2;

    if (iconWidth > 0) {
        const QPixmap iconPixmap = Icons::pixmap(m_iconName, content, m_iconSize, devicePixelRatioF());
        if (!iconPixmap.isNull()) {
            const int iconY = centerY - m_iconSize / 2;
            painter.drawPixmap(QRect(x, iconY, m_iconSize, m_iconSize), iconPixmap);
        }
        x += iconWidth + gap;
    }

    if (!label.isEmpty()) {
        painter.setPen(content);
        painter.drawText(QRect(x, 0, width() - x - 4, height()), Qt::AlignVCenter | Qt::AlignLeft, label);
    }
}

// ---------------------------------------------------------------------------
// MIconButton
// ---------------------------------------------------------------------------

MIconButton::MIconButton(const QString &iconName, const QString &tooltip, QWidget *parent)
    : QAbstractButton(parent)
    , m_iconName(iconName)
{
    setToolTip(tooltip);
    setAccessibleName(tooltip);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setFixedSize(m_size, m_size);
    connect(&Theme::instance(), &Theme::changed, this, qOverload<>(&QWidget::update));
}

void MIconButton::setIconName(const QString &name)
{
    m_iconName = name;
    update();
}

void MIconButton::setButtonSize(int size)
{
    m_size = qMax(20, size);
    m_iconSize = qBound(12, m_size - 14, 22);
    setFixedSize(m_size, m_size);
    updateGeometry();
    update();
}

void MIconButton::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    update();
}

void MIconButton::setIconColor(const QColor &color)
{
    m_iconColorOverride = color;
    m_hasColorOverride = true;
    update();
}

void MIconButton::clearIconColorOverride()
{
    m_hasColorOverride = false;
    update();
}

QSize MIconButton::sizeHint() const
{
    return QSize(m_size, m_size);
}

void MIconButton::paintEvent(QPaintEvent *)
{
    const Palette &p = Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = QRectF(rect()).adjusted(1, 1, -1, -1);
    QColor background = Qt::transparent;
    if (m_active)
        background = withAlpha(p.accent, 40);
    if (m_hovered)
        background = m_active ? withAlpha(p.accent, 58) : p.surfaceHover;
    if (isDown() && isEnabled())
        background = withAlpha(p.accent, 65);

    if (background.alpha() > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(background);
        painter.drawRoundedRect(bounds, 8, 8);
    }

    if (hasFocus()) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(withAlpha(p.accent, 150), 1.2));
        painter.drawRoundedRect(bounds.adjusted(1, 1, -1, -1), 7, 7);
    }

    QColor color = m_hasColorOverride ? m_iconColorOverride : (m_active ? p.accent : p.textMuted);
    if (!isEnabled())
        color = p.textFaint;
    else if (m_hovered)
        color = m_hasColorOverride ? m_iconColorOverride : p.text;

    const QPixmap pixmap = Icons::pixmap(m_iconName, color, m_iconSize, devicePixelRatioF());
    if (pixmap.isNull())
        return;
    const int x = (width() - m_iconSize) / 2;
    const int y = (height() - m_iconSize) / 2;
    painter.drawPixmap(QRect(x, y, m_iconSize, m_iconSize), pixmap);
}

void MIconButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void MIconButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

// ---------------------------------------------------------------------------
// MTextField
// ---------------------------------------------------------------------------

MTextField::MTextField(const QString &label, const QString &iconName, QWidget *parent)
    : QWidget(parent)
    , m_label(label)
    , m_iconName(iconName)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(fieldHeight());
    setCursor(Qt::IBeamCursor);
    setFocusPolicy(Qt::NoFocus);

    m_edit = new QLineEdit(this);
    m_edit->setObjectName(QStringLiteral("mTextFieldEdit"));
    m_edit->setFrame(false);
    m_edit->setFont(Theme::instance().uiFont(10));
    m_edit->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_edit->setAttribute(Qt::WA_TranslucentBackground, true);
    m_edit->setAutoFillBackground(false);
    m_edit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: transparent; border: none; padding: 0; }"));
    m_edit->installEventFilter(this);
    connect(m_edit, &QLineEdit::textChanged, this, [this](const QString &text) {
        updateLabelAnimation();
        updateLabelAppearance();
        emit textChanged(text);
    });
    connect(m_edit, &QLineEdit::returnPressed, this, &MTextField::returnPressed);

    m_labelWidget = new QLabel(this);
    m_labelWidget->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_labelWidget->setAttribute(Qt::WA_NoSystemBackground, true);
    m_labelWidget->setTextFormat(Qt::PlainText);
    m_labelWidget->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_labelWidget->setStyleSheet(QStringLiteral("background: transparent;"));

    m_toggle = new MIconButton(QStringLiteral("eye"), QStringLiteral("显示密码"), this);
    m_toggle->setButtonSize(28);
    m_toggle->setFocusPolicy(Qt::NoFocus);
    m_toggle->hide();
    connect(m_toggle, &QAbstractButton::clicked, this, [this] {
        if (!m_edit)
            return;
        const bool hidden = m_edit->echoMode() == QLineEdit::Password;
        m_edit->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
        updateToggleButton();
    });

    m_floatAnimation = new QVariantAnimation(this);
    m_floatAnimation->setDuration(150);
    m_floatAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_floatAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_floatProgress = value.toReal();
        updateLabelAppearance();
        update();
    });
    connect(&Theme::instance(), &Theme::changed, this, [this] {
        m_edit->setFont(Theme::instance().uiFont(10));
        updateLabelAppearance();
        update();
    });
    updateLabelAppearance();
}

QString MTextField::text() const
{
    return m_edit ? m_edit->text() : QString();
}

void MTextField::setText(const QString &text)
{
    if (m_edit)
        m_edit->setText(text);
}

void MTextField::setLabel(const QString &label)
{
    m_label = label;
    updateLabelAppearance();
    update();
}

void MTextField::setPlaceholder(const QString &placeholder)
{
    m_placeholderText = placeholder;
    if (m_edit)
        m_edit->setPlaceholderText(m_focused ? placeholder : QString());
}

void MTextField::setPasswordMode(bool password)
{
    m_password = password;
    m_toggle->setVisible(password);
    if (m_edit) {
        m_edit->setEchoMode(password ? QLineEdit::Password : QLineEdit::Normal);
        resizeEvent(nullptr);
    }
    updateToggleButton();
}

void MTextField::setIconName(const QString &iconName)
{
    m_iconName = iconName;
    if (m_edit)
        resizeEvent(nullptr);
    updateLabelAppearance();
    update();
}

void MTextField::setSupportingText(const QString &text)
{
    m_supportingText = text;
    updateHeights();
    update();
}

void MTextField::setError(bool error, const QString &message)
{
    m_error = error;
    if (m_error && !message.isEmpty())
        m_supportingText = message;
    else if (!error)
        m_supportingText.clear();
    updateHeights();
    updateLabelAppearance();
    update();
}

void MTextField::setEnabled(bool enabled)
{
    QWidget::setEnabled(enabled);
    if (m_edit)
        m_edit->setEnabled(enabled);
    if (m_toggle)
        m_toggle->setEnabled(enabled);
    updateLabelAppearance();
    update();
}

void MTextField::updateHeights()
{
    setFixedHeight(m_supportingText.isEmpty() ? fieldHeight() : fieldHeight() + 18);
    updateGeometry();
}

void MTextField::updateToggleButton()
{
    if (!m_toggle || !m_edit)
        return;
    const bool hidden = m_edit->echoMode() == QLineEdit::Password;
    m_toggle->setIconName(hidden ? QStringLiteral("eye") : QStringLiteral("eye-off"));
    m_toggle->setToolTip(hidden ? QStringLiteral("显示密码") : QStringLiteral("隐藏密码"));
}

void MTextField::updateLabelAnimation()
{
    const qreal target = (m_focused || (!m_edit->text().isEmpty())) ? 1.0 : 0.0;
    if (qFuzzyCompare(m_floatProgress, target))
        return;
    m_floatAnimation->stop();
    m_floatAnimation->setStartValue(m_floatProgress);
    m_floatAnimation->setEndValue(target);
    m_floatAnimation->start();
}

void MTextField::resizeEvent(QResizeEvent *)
{
    const int startX = m_iconName.isEmpty() ? 14 : 42;
    const int editRight = m_password ? 46 : 12;
    if (m_edit)
        m_edit->setGeometry(startX, 24, qMax(10, width() - startX - editRight), 28);
    if (m_toggle) {
        m_toggle->move(width() - 38, 16);
        m_toggle->raise();
    }
    updateLabelAppearance();
}

void MTextField::updateLabelAppearance()
{
    if (!m_labelWidget)
        return;
    const Palette &p = Theme::instance().palette();
    QFont font = Theme::instance().uiFont(10.5, QFont::Medium);
    font.setPointSizeF(10.5 + (8.5 - 10.5) * m_floatProgress);
    m_labelWidget->setFont(font);

    const int textX = m_iconName.isEmpty() ? 14 : 42;
    const int available = qMax(10, width() - textX - (m_password ? 46 : 14));
    const QFontMetrics metrics(font);
    m_labelWidget->setText(metrics.elidedText(m_label, Qt::ElideRight, available));

    const int height = metrics.height() + 2;
    const int restY = (fieldHeight() - height) / 2 + 1;
    const int floatY = 4;
    m_labelWidget->setGeometry(textX, qRound(restY + (floatY - restY) * m_floatProgress), available,
                              height);

    QColor color = m_error ? p.danger : (m_focused ? p.accent : p.textMuted);
    if (!isEnabled())
        color = p.textFaint;
    m_labelWidget->setStyleSheet(
        QStringLiteral("background:transparent;color:%1;").arg(color.name()));
    m_labelWidget->raise();
}

void MTextField::mousePressEvent(QMouseEvent *event)
{
    if (m_edit) {
        m_edit->setFocus(Qt::MouseFocusReason);
        m_edit->setCursorPosition(m_edit->cursorPositionAt(event->position().toPoint() - m_edit->pos()));
    }
    QWidget::mousePressEvent(event);
}

bool MTextField::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_edit) {
        if (event->type() == QEvent::FocusIn) {
            m_focused = true;
            if (!m_placeholderText.isEmpty())
                m_edit->setPlaceholderText(m_placeholderText);
            updateLabelAnimation();
            updateLabelAppearance();
            update();
        } else if (event->type() == QEvent::FocusOut) {
            m_focused = false;
            m_edit->setPlaceholderText(QString());
            updateLabelAnimation();
            updateLabelAppearance();
            update();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MTextField::paintEvent(QPaintEvent *)
{
    const Palette &p = Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal fh = fieldHeight();
    const QRectF bounds(0.5, 0.5, width() - 1.0, fh - 1.0);
    QPainterPath path;
    path.addRoundedRect(bounds, 9, 9);

    QColor background = isEnabled() ? p.surfaceAlt : withAlpha(p.surfaceAlt, 150);
    painter.fillPath(path, background);

    QColor borderColor = p.border;
    qreal borderWidth = 1.0;
    if (m_error)
        borderColor = p.danger;
    else if (m_focused)
        borderColor = p.accent;
    if (m_focused || m_error) {
        borderColor.setAlpha(230);
        borderWidth = 1.5;
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(borderColor, borderWidth));
    painter.drawPath(path);

    if (!m_iconName.isEmpty()) {
        const QColor iconColor = m_error ? p.danger : (m_focused ? p.accent : p.textMuted);
        const QPixmap iconPixmap = Icons::pixmap(m_iconName, iconColor, 20, devicePixelRatioF());
        painter.drawPixmap(QRect(14, 20, 20, 20), iconPixmap);
    }

    if (!m_supportingText.isEmpty()) {
        QFont helperFont = Theme::instance().uiFont(9);
        painter.setFont(helperFont);
        painter.setPen(m_error ? p.danger : p.textMuted);
        const QFontMetrics helperMetrics(helperFont);
        const QString helper = helperMetrics.elidedText(m_supportingText, Qt::ElideRight, width() - 8);
        painter.drawText(QPointF(4, fh + 13), helper);
    }
}

// ---------------------------------------------------------------------------
// MSwitch
// ---------------------------------------------------------------------------

MSwitch::MSwitch(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(44, 24);
    setFocusPolicy(Qt::TabFocus);

    m_animation = new QVariantAnimation(this);
    m_animation->setDuration(160);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_progress = value.toReal();
        update();
    });
    connect(this, &QAbstractButton::toggled, this, [this](bool checked) {
        animateTo(checked ? 1.0 : 0.0);
    });
    connect(&Theme::instance(), &Theme::changed, this, qOverload<>(&QWidget::update));
}

QSize MSwitch::sizeHint() const
{
    return QSize(44, 24);
}

void MSwitch::animateTo(qreal target)
{
    m_animation->stop();
    m_animation->setStartValue(m_progress);
    m_animation->setEndValue(target);
    m_animation->start();
}

void MSwitch::paintEvent(QPaintEvent *)
{
    const Palette &p = Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF track(1, 3, width() - 2, height() - 6);
    QColor trackColor = isChecked() ? p.accent : p.surfaceAlt;
    if (!isEnabled())
        trackColor = withAlpha(trackColor, 120);
    painter.setPen(QPen(isChecked() ? withAlpha(p.accent, 180) : p.borderStrong, 1));
    painter.setBrush(trackColor);
    painter.drawRoundedRect(track, track.height() / 2.0, track.height() / 2.0);

    const qreal thumbSize = 16.0;
    const qreal travel = track.width() - thumbSize - 6.0;
    const qreal x = track.left() + 3.0 + travel * m_progress;
    const QRectF thumb(x, (height() - thumbSize) / 2.0, thumbSize, thumbSize);
    painter.setPen(Qt::NoPen);
    painter.setBrush(withAlpha(p.shadow, 55));
    painter.drawEllipse(thumb.translated(0, 1.2));
    painter.setBrush(isChecked() ? QColor(QStringLiteral("#FFFFFF")) : p.textMuted);
    painter.drawEllipse(thumb);

    if (hasFocus()) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(withAlpha(p.accent, 150), 1.2));
        painter.drawRoundedRect(track.adjusted(-1.5, -1.5, 1.5, 1.5), track.height() / 2.0 + 1.5,
                                track.height() / 2.0 + 1.5);
    }
}

// ---------------------------------------------------------------------------
// MCard
// ---------------------------------------------------------------------------

MCard::MCard(QWidget *parent)
    : QFrame(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    connect(&Theme::instance(), &Theme::changed, this, qOverload<>(&QWidget::update));
}

void MCard::setCardRadius(int radius)
{
    m_radius = qMax(0, radius);
    update();
}

void MCard::setSurfaceColor(const QColor &color)
{
    m_surfaceOverride = color;
    m_hasSurfaceOverride = true;
    update();
}

void MCard::setShadowEnabled(bool enabled)
{
    if (enabled && !m_shadowEffect) {
        m_shadowEffect = new QGraphicsDropShadowEffect(this);
        m_shadowEffect->setBlurRadius(32);
        m_shadowEffect->setOffset(0, 10);
        m_shadowEffect->setColor(Theme::instance().palette().shadow);
        setGraphicsEffect(m_shadowEffect);
    } else if (!enabled && m_shadowEffect) {
        setGraphicsEffect(nullptr);
        m_shadowEffect = nullptr;
    }
}

void MCard::setHoverable(bool hoverable)
{
    m_hoverable = hoverable;
    setAttribute(Qt::WA_Hover, hoverable);
}

void MCard::enterEvent(QEnterEvent *event)
{
    if (m_hoverable) {
        m_hovered = true;
        update();
    }
    QFrame::enterEvent(event);
}

void MCard::leaveEvent(QEvent *event)
{
    if (m_hoverable) {
        m_hovered = false;
        update();
    }
    QFrame::leaveEvent(event);
}

void MCard::paintEvent(QPaintEvent *)
{
    const Palette &p = Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(bounds, m_radius, m_radius);
    painter.fillPath(path, m_hasSurfaceOverride ? m_surfaceOverride : p.surface);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(m_hovered && m_hoverable ? withAlpha(p.accent, 150) : p.border, 1.0));
    painter.drawPath(path);
}

// ---------------------------------------------------------------------------
// MDialog
// ---------------------------------------------------------------------------

MDialog::MDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName(QStringLiteral("materialDialog"));
    setModal(true);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(28, 28, 28, 28);
    outer->setSpacing(0);

    m_surface = new QWidget(this);
    m_surface->setObjectName(QStringLiteral("dialogSurface"));
    m_surface->setAttribute(Qt::WA_StyledBackground, true);
    outer->addWidget(m_surface);

    m_shadow = new QGraphicsDropShadowEffect(m_surface);
    m_shadow->setBlurRadius(46);
    m_shadow->setOffset(0, 14);
    m_shadow->setColor(Theme::instance().palette().shadow);
    m_surface->setGraphicsEffect(m_shadow);

    auto *surfaceLayout = new QVBoxLayout(m_surface);
    surfaceLayout->setContentsMargins(24, 22, 24, 22);
    surfaceLayout->setSpacing(18);

    m_header = new QWidget(m_surface);
    auto *headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(12);
    auto *titleLayout = new QVBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(5);
    m_titleLabel = new QLabel(title, m_header);
    m_titleLabel->setFont(Theme::instance().displayFont(15, QFont::DemiBold));
    m_titleLabel->setWordWrap(true);
    m_subtitleLabel = new QLabel(m_header);
    m_subtitleLabel->setProperty("role", QStringLiteral("muted"));
    m_subtitleLabel->setFont(Theme::instance().uiFont(9.5));
    m_subtitleLabel->setWordWrap(true);
    m_subtitleLabel->hide();
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_subtitleLabel);
    headerLayout->addLayout(titleLayout, 1);

    m_closeButton = new MIconButton(QStringLiteral("close"), QStringLiteral("关闭"), m_header);
    m_closeButton->setFocusPolicy(Qt::NoFocus);
    connect(m_closeButton, &QAbstractButton::clicked, this, &QDialog::reject);
    headerLayout->addWidget(m_closeButton, 0, Qt::AlignTop);
    surfaceLayout->addWidget(m_header);

    m_contentLayout = new QVBoxLayout();
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(14);
    surfaceLayout->addLayout(m_contentLayout);

    m_footerLayout = new QHBoxLayout();
    m_footerLayout->setContentsMargins(0, 4, 0, 0);
    m_footerLayout->setSpacing(10);
    m_footerLayout->addStretch();
    surfaceLayout->addLayout(m_footerLayout);

    m_header->installEventFilter(this);
    setPreferredWidth(520);
    m_fade = new QVariantAnimation(this);
    m_fade->setDuration(140);
    m_fade->setStartValue(0.0);
    m_fade->setEndValue(1.0);
    connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setWindowOpacity(value.toReal());
    });

    connect(&Theme::instance(), &Theme::changed, this, [this] {
        if (m_shadow)
            m_shadow->setColor(Theme::instance().palette().shadow);
        update();
    });
}

void MDialog::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
}

void MDialog::setSubtitle(const QString &subtitle)
{
    m_subtitleLabel->setText(subtitle);
    m_subtitleLabel->setVisible(!subtitle.isEmpty());
}

void MDialog::setPreferredWidth(int width)
{
    m_preferredWidth = qMax(360, width);
    setFixedWidth(m_preferredWidth + 56);
}

void MDialog::setClosable(bool closable)
{
    m_closeButton->setVisible(closable);
}

void MDialog::addFooterButton(MButton *button, bool stretchBefore)
{
    if (stretchBefore)
        m_footerLayout->addStretch();
    m_footerLayout->addWidget(button);
}

void MDialog::addFooterStretch()
{
    m_footerLayout->addStretch();
}

void MDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (!m_centered) {
        m_centered = true;
        adjustSize();
        QWidget *anchor = parentWidget() ? parentWidget()->window() : nullptr;
        QRect target;
        if (anchor && anchor->isVisible())
            target = anchor->frameGeometry();
        else if (screen())
            target = screen()->availableGeometry();
        if (target.isValid())
            move(target.center() - rect().center());
    }
    setWindowOpacity(0.0);
    m_fade->stop();
    m_fade->start();
}

void MDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    QDialog::keyPressEvent(event);
}

bool MDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_header) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragOffset = mouse->globalPosition().toPoint() - frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            move(mouse->globalPosition().toPoint() - m_dragOffset);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_dragging = false;
        }
    }
    return QDialog::eventFilter(watched, event);
}

// ---------------------------------------------------------------------------
// MSnackbar
// ---------------------------------------------------------------------------

MSnackbar::MSnackbar(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);
    hide();

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 9, 9, 9);
    layout->setSpacing(10);

    m_label = new QLabel(this);
    m_label->setFont(Theme::instance().uiFont(9.5));
    m_label->setTextInteractionFlags(Qt::NoTextInteraction);
    layout->addWidget(m_label, 1);

    m_action = new MButton(QString(), MButton::Variant::Text, this);
    m_action->setCompact(true);
    m_action->hide();
    layout->addWidget(m_action, 0, Qt::AlignVCenter);
    connect(m_action, &QAbstractButton::clicked, this, [this] {
        auto callback = m_callback;
        hideAnimated();
        if (callback)
            callback();
    });

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &MSnackbar::hideAnimated);

    m_fade = new QVariantAnimation(this);
    m_fade->setDuration(170);
    m_fade->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        if (m_opacity)
            m_opacity->setOpacity(value.toReal());
    });
    connect(m_fade, &QVariantAnimation::finished, this, [this] {
        if (m_opacity && m_opacity->opacity() <= 0.01)
            hide();
    });

    m_opacity = new QGraphicsOpacityEffect(this);
    m_opacity->setOpacity(1.0);
    setGraphicsEffect(m_opacity);
    if (parent)
        parent->installEventFilter(this);
    connect(&Theme::instance(), &Theme::changed, this, [this] {
        m_label->setFont(Theme::instance().uiFont(9.5));
        update();
    });
}

void MSnackbar::showMessage(const QString &text, const QString &actionText, std::function<void()> action)
{
    m_label->setText(text);
    m_callback = std::move(action);
    m_action->setVisible(!actionText.isEmpty());
    if (!actionText.isEmpty())
        m_action->setText(actionText);

    adjustSize();
    const int maxWidth = qMax(280, parentWidget() ? parentWidget()->width() - 100 : 520);
    if (width() > maxWidth)
        resize(maxWidth, height());
    adjustSize();

    if (parentWidget()) {
        const int x = (parentWidget()->width() - width()) / 2;
        const int y = parentWidget()->height() - height() - 42;
        move(qMax(12, x), qMax(12, y));
    }

    show();
    raise();
    if (m_opacity)
        m_opacity->setOpacity(1.0);
    m_fade->stop();
    m_fade->setStartValue(0.0);
    m_fade->setEndValue(1.0);
    m_fade->start();
    m_timer->start(3600);
}

void MSnackbar::hideAnimated()
{
    m_timer->stop();
    m_fade->stop();
    m_fade->setStartValue(m_opacity ? m_opacity->opacity() : 1.0);
    m_fade->setEndValue(0.0);
    m_fade->start();
}

void MSnackbar::paintEvent(QPaintEvent *)
{
    const Palette &p = Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setBrush(p.elevated);
    painter.setPen(QPen(p.borderStrong, 1));
    painter.drawRoundedRect(bounds, 10, 10);
}

bool MSnackbar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize && isVisible()) {
        const int x = (parentWidget()->width() - width()) / 2;
        const int y = parentWidget()->height() - height() - 42;
        move(qMax(12, x), qMax(12, y));
    }
    return QWidget::eventFilter(watched, event);
}

// ---------------------------------------------------------------------------
// MSpinner
// ---------------------------------------------------------------------------

MSpinner::MSpinner(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(36, 36);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_timer = new QTimer(this);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, [this] {
        m_angle = std::fmod(m_angle + 7.0, 360.0);
        update();
    });
    connect(&Theme::instance(), &Theme::changed, this, qOverload<>(&QWidget::update));
}

QSize MSpinner::sizeHint() const
{
    return QSize(36, 36);
}

void MSpinner::setColor(const QColor &color)
{
    m_color = color;
    m_hasColor = true;
    update();
}

void MSpinner::start()
{
    m_running = true;
    if (isVisible())
        m_timer->start();
}

void MSpinner::stop()
{
    m_running = false;
    m_timer->stop();
}

void MSpinner::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_running)
        m_timer->start();
}

void MSpinner::hideEvent(QHideEvent *event)
{
    m_timer->stop();
    QWidget::hideEvent(event);
}

void MSpinner::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor color = m_hasColor ? m_color : Theme::instance().palette().accent;
    QPen pen(color, 3.0);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const QRectF arcRect = QRectF(rect()).adjusted(5, 5, -5, -5);
    painter.drawArc(arcRect, static_cast<int>(-m_angle * 16), 280 * 16);
}

// ---------------------------------------------------------------------------
// MTag
// ---------------------------------------------------------------------------

MTag::MTag(const QString &text, const QColor &color, QWidget *parent)
    : QWidget(parent)
    , m_text(text)
    , m_color(color)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(&Theme::instance(), &Theme::changed, this, qOverload<>(&QWidget::update));
}

void MTag::setText(const QString &text)
{
    m_text = text;
    updateGeometry();
    update();
}

void MTag::setColor(const QColor &color)
{
    m_color = color;
    update();
}

QSize MTag::sizeHint() const
{
    const QFontMetrics metrics(Theme::instance().uiFont(8.5, QFont::Medium));
    return QSize(metrics.horizontalAdvance(m_text) + 16, 22);
}

void MTag::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QColor background = m_color;
    background.setAlpha(34);
    painter.setBrush(background);
    painter.setPen(QPen(withAlpha(m_color, 90), 1));
    painter.drawRoundedRect(bounds, 6, 6);
    painter.setPen(m_color.lighter(120));
    painter.setFont(Theme::instance().uiFont(8.5, QFont::Medium));
    painter.drawText(rect(), Qt::AlignCenter, m_text);
}

// ---------------------------------------------------------------------------
// MSegmentedControl
// ---------------------------------------------------------------------------

MSegmentedControl::MSegmentedControl(const QStringList &options, QWidget *parent)
    : QWidget(parent)
    , m_options(options)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(38);
    setMinimumWidth(200);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_animation = new QVariantAnimation(this);
    m_animation->setDuration(180);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_indicator = value.toReal();
        update();
    });
    connect(&Theme::instance(), &Theme::changed, this, qOverload<>(&QWidget::update));
}

QSize MSegmentedControl::sizeHint() const
{
    const QFontMetrics metrics(Theme::instance().uiFont(9.5, QFont::DemiBold));
    int widest = 0;
    for (const QString &option : m_options)
        widest = qMax(widest, metrics.horizontalAdvance(option));
    return QSize(qMax(220, widest * m_options.size() + 56), 38);
}

void MSegmentedControl::setCurrentIndex(int index, bool animated)
{
    if (m_options.isEmpty())
        return;
    index = qBound(0, index, m_options.size() - 1);
    if (index == m_currentIndex)
        return;
    m_currentIndex = index;
    if (animated) {
        m_animation->stop();
        m_animation->setStartValue(m_indicator);
        m_animation->setEndValue(static_cast<qreal>(index));
        m_animation->start();
    } else {
        m_indicator = index;
    }
    update();
    emit currentChanged(index);
}

void MSegmentedControl::paintEvent(QPaintEvent *)
{
    const Palette &p = Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setBrush(p.surfaceAlt);
    painter.setPen(QPen(p.border, 1));
    painter.drawRoundedRect(bounds, 9, 9);

    if (m_options.isEmpty())
        return;
    const qreal segmentWidth = width() / static_cast<qreal>(m_options.size());
    const QRectF indicator(m_indicator * segmentWidth + 3, 3, segmentWidth - 6, height() - 6);
    painter.setBrush(p.accent);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(indicator, 7, 7);

    painter.setFont(Theme::instance().uiFont(9.5, QFont::DemiBold));
    for (int i = 0; i < m_options.size(); ++i) {
        const QRectF segment(i * segmentWidth, 0, segmentWidth, height());
        painter.setPen(i == m_currentIndex ? p.onAccent : p.textMuted);
        painter.drawText(segment, Qt::AlignCenter, m_options.at(i));
    }

    if (hasFocus()) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(withAlpha(p.accent, 150), 1.2));
        painter.drawRoundedRect(bounds.adjusted(1, 1, -1, -1), 8, 8);
    }
}

void MSegmentedControl::mousePressEvent(QMouseEvent *event)
{
    if (m_options.isEmpty())
        return;
    const int index = qBound(0, static_cast<int>(event->position().x() / (width() / m_options.size())),
                             m_options.size() - 1);
    setCurrentIndex(index);
}

void MSegmentedControl::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Left)
        setCurrentIndex(qMax(0, m_currentIndex - 1));
    else if (event->key() == Qt::Key_Right)
        setCurrentIndex(qMin(m_options.size() - 1, m_currentIndex + 1));
    else
        QWidget::keyPressEvent(event);
}

} // namespace nyan::ui
