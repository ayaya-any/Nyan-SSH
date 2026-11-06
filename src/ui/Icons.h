#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>

namespace nyan::ui::Icons {

QPixmap pixmap(const QString &name, const QColor &color, int size, qreal devicePixelRatio = 0.0);
QIcon icon(const QString &name, const QColor &color, int size = 18);
QPixmap logo(int size, qreal devicePixelRatio = 0.0);

} // namespace nyan::ui::Icons
