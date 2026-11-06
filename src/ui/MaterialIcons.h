#pragma once

#include <QColor>
#include <QPixmap>
#include <QString>

namespace nyan::ui::MaterialIcons {

/// True when the bundled Material Icons font could be loaded from the qrc.
bool available();
QString familyName();

/// Render a Material Icons glyph.  \a name may be a canonical Material name
/// (`cloud_upload`, `folder_open`, ...), an underscore name, or one of the
/// project's historical aliases (`eye-off`, `chevron-right`, ...).  Returns a
/// null pixmap when the name is unknown, letting Icons::pixmap fall back to
/// the original SVG artwork.
QPixmap pixmap(const QString &name, const QColor &color, int size, qreal devicePixelRatio = 0.0);

} // namespace nyan::ui::MaterialIcons
