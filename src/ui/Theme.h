#pragma once

#include "core/AppSettings.h"

#include <QColor>
#include <QFont>
#include <QObject>
#include <QVector>

namespace nyan::ui {

struct Palette {
    QColor window;
    QColor sidebar;
    QColor surface;
    QColor surfaceAlt;
    QColor surfaceHover;
    QColor elevated;
    QColor border;
    QColor borderStrong;
    QColor text;
    QColor textMuted;
    QColor textFaint;
    QColor accent;
    QColor accentHover;
    QColor accentPressed;
    QColor onAccent;
    QColor success;
    QColor warning;
    QColor danger;
    QColor shadow;
    QColor terminalBackground;
    QColor terminalForeground;
    QColor terminalCursor;
    QColor terminalSelection;
    QVector<QColor> ansi;

    QColor accentSoft(int alpha = 36) const;
    QColor alpha(const QColor &base, int alpha) const;
};

class Theme : public QObject {
    Q_OBJECT
public:
    static Theme &instance();

    void setMode(ThemeMode mode);
    ThemeMode mode() const { return m_mode; }
    const Palette &palette() const { return m_palette; }

    QFont uiFont(int pointSize = 10, QFont::Weight weight = QFont::Normal) const;
    QFont displayFont(int pointSize = 18, QFont::Weight weight = QFont::DemiBold) const;
    QFont monoFont(int pointSize = 11, const QString &family = QString()) const;

    QString styleSheet() const;
    void apply();

    static QString rgba(const QColor &color, int alpha);

signals:
    void changed();

private:
    Theme();
    void rebuild();

    ThemeMode m_mode = ThemeMode::Dark;
    Palette m_palette;
};

} // namespace nyan::ui
