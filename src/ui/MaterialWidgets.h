#pragma once

#include "ui/Theme.h"

#include <QAbstractButton>
#include <QDialog>
#include <QFrame>
#include <QStringList>
#include <QWidget>

#include <functional>

class QLabel;
class QLineEdit;
class QHBoxLayout;
class QVBoxLayout;
class QVariantAnimation;
class QGraphicsDropShadowEffect;
class QGraphicsOpacityEffect;
class QTimer;
namespace nyan::ui {

class MButton : public QAbstractButton {
    Q_OBJECT
public:
    enum class Variant {
        Filled,
        Tonal,
        Outlined,
        Text,
        Danger,
    };

    explicit MButton(const QString &text = QString(), Variant variant = Variant::Filled,
                     QWidget *parent = nullptr);

    void setVariant(Variant variant);
    Variant variant() const { return m_variant; }
    void setIconName(const QString &name);
    QString iconName() const { return m_iconName; }
    void setIconSize(int size);
    void setCompact(bool compact);
    bool isCompact() const { return m_compact; }
    void setText(const QString &text);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void startRipple(const QPointF &position);
    QColor contentColor() const;
    QColor backgroundColor() const;

    Variant m_variant = Variant::Filled;
    QString m_iconName;
    int m_iconSize = 18;
    bool m_compact = false;
    bool m_hovered = false;
    qreal m_rippleProgress = 1.0;
    QPointF m_rippleCenter;
    qreal m_rippleRadius = 0.0;
    QVariantAnimation *m_rippleAnimation = nullptr;
};

class MIconButton : public QAbstractButton {
    Q_OBJECT
public:
    explicit MIconButton(const QString &iconName, const QString &tooltip = QString(),
                         QWidget *parent = nullptr);

    void setIconName(const QString &name);
    QString iconName() const { return m_iconName; }
    void setButtonSize(int size);
    int buttonSize() const { return m_size; }
    void setActive(bool active);
    bool isActive() const { return m_active; }
    void setIconColor(const QColor &color);
    void clearIconColorOverride();
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_iconName;
    int m_size = 34;
    int m_iconSize = 18;
    bool m_hovered = false;
    bool m_active = false;
    QColor m_iconColorOverride;
    bool m_hasColorOverride = false;
};

class MTextField : public QWidget {
    Q_OBJECT
public:
    explicit MTextField(const QString &label, const QString &iconName = QString(),
                        QWidget *parent = nullptr);

    QLineEdit *lineEdit() const { return m_edit; }
    QString text() const;
    void setText(const QString &text);
    void setLabel(const QString &label);
    void setPlaceholder(const QString &placeholder);
    void setPasswordMode(bool password);
    void setIconName(const QString &iconName);
    void setSupportingText(const QString &text);
    void setError(bool error, const QString &message = QString());
    bool hasError() const { return m_error; }
    void setEnabled(bool enabled);

signals:
    void textChanged(const QString &text);
    void returnPressed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateLabelAnimation();
    void updateLabelAppearance();
    void updateToggleButton();
    void updateHeights();
    int fieldHeight() const { return 60; }

    QLineEdit *m_edit = nullptr;
    QLabel *m_labelWidget = nullptr;
    MIconButton *m_toggle = nullptr;
    QString m_label;
    QString m_iconName;
    QString m_placeholderText;
    QString m_supportingText;
    bool m_password = false;
    bool m_error = false;
    bool m_focused = false;
    qreal m_floatProgress = 0.0;
    QVariantAnimation *m_floatAnimation = nullptr;
};

class MSwitch : public QAbstractButton {
    Q_OBJECT
public:
    explicit MSwitch(QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void animateTo(qreal target);
    qreal m_progress = 0.0;
    QVariantAnimation *m_animation = nullptr;
};

class MCard : public QFrame {
    Q_OBJECT
public:
    explicit MCard(QWidget *parent = nullptr);

    void setCardRadius(int radius);
    void setSurfaceColor(const QColor &color);
    void setShadowEnabled(bool enabled);
    void setHoverable(bool hoverable);
    bool isHoverable() const { return m_hoverable; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    int m_radius = 12;
    bool m_hasSurfaceOverride = false;
    QColor m_surfaceOverride;
    bool m_hoverable = false;
    bool m_hovered = false;
    QGraphicsDropShadowEffect *m_shadowEffect = nullptr;
};

class MDialog : public QDialog {
    Q_OBJECT
public:
    explicit MDialog(const QString &title, QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setSubtitle(const QString &subtitle);
    void setPreferredWidth(int width);
    void setClosable(bool closable);
    QVBoxLayout *contentLayout() const { return m_contentLayout; }
    QHBoxLayout *footerLayout() const { return m_footerLayout; }
    void addFooterButton(MButton *button, bool stretchBefore = false);
    void addFooterStretch();

protected:
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *m_surface = nullptr;
    QWidget *m_header = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    MIconButton *m_closeButton = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QHBoxLayout *m_footerLayout = nullptr;
    QGraphicsDropShadowEffect *m_shadow = nullptr;
    QVariantAnimation *m_fade = nullptr;
    int m_preferredWidth = 520;
    bool m_centered = false;
    bool m_dragging = false;
    QPoint m_dragOffset;
};

class MSnackbar : public QWidget {
    Q_OBJECT
public:
    explicit MSnackbar(QWidget *parent);

    void showMessage(const QString &text, const QString &actionText = QString(),
                     std::function<void()> action = {});

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void hideAnimated();

    QLabel *m_label = nullptr;
    MButton *m_action = nullptr;
    QTimer *m_timer = nullptr;
    std::function<void()> m_callback;
    QGraphicsOpacityEffect *m_opacity = nullptr;
    QVariantAnimation *m_fade = nullptr;
};

class MSpinner : public QWidget {
    Q_OBJECT
public:
    explicit MSpinner(QWidget *parent = nullptr);
    QSize sizeHint() const override;
    void setColor(const QColor &color);
    void start();
    void stop();
    bool isRunning() const { return m_running; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    QTimer *m_timer = nullptr;
    QColor m_color;
    bool m_hasColor = false;
    qreal m_angle = 0.0;
    bool m_running = false;
};

class MTag : public QWidget {
    Q_OBJECT
public:
    explicit MTag(const QString &text, const QColor &color, QWidget *parent = nullptr);
    void setText(const QString &text);
    void setColor(const QColor &color);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_text;
    QColor m_color;
};

class MSegmentedControl : public QWidget {
    Q_OBJECT
public:
    explicit MSegmentedControl(const QStringList &options, QWidget *parent = nullptr);

    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index, bool animated = true);
    QSize sizeHint() const override;

signals:
    void currentChanged(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QStringList m_options;
    int m_currentIndex = 0;
    qreal m_indicator = 0.0;
    QVariantAnimation *m_animation = nullptr;
    int m_hovered = -1;
};

} // namespace nyan::ui
