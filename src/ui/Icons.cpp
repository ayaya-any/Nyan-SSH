#include "ui/Icons.h"

#include "ui/MaterialIcons.h"

#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QString>
#include <QSvgRenderer>

namespace nyan::ui::Icons {
namespace {

QHash<QString, QPixmap> &cache()
{
    static QHash<QString, QPixmap> instance;
    return instance;
}

qreal effectiveDpr(qreal requested)
{
    if (requested > 0.0)
        return requested;
    if (qApp)
        return qApp->devicePixelRatio();
    return 1.0;
}

} // namespace

QPixmap pixmap(const QString &name, const QColor &color, int size, qreal devicePixelRatio)
{
    const qreal dpr = effectiveDpr(devicePixelRatio);
    const int pixelSize = qMax(1, qRound(size * dpr));
    const QString key = QStringLiteral("%1|%2|%3|%4")
                            .arg(name, color.name(QColor::HexArgb))
                            .arg(size)
                            .arg(dpr);

    auto &store = cache();
    const auto it = store.constFind(key);
    if (it != store.cend())
        return it.value();

    // Material Icons is the primary icon library.  Custom SVGs (logo, window
    // decorations and any unmapped names) remain available as a fallback.
    if (const QPixmap material = MaterialIcons::pixmap(name, color, size, dpr);
        !material.isNull()) {
        return material;
    }

    const QString path = QStringLiteral(":/icons/%1.svg").arg(name);
    QSvgRenderer renderer(path);
    if (!renderer.isValid())
        return {};

    QImage image(pixelSize, pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&painter, QRectF(0, 0, pixelSize, pixelSize));
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(image.rect(), color);
    painter.end();

    image.setDevicePixelRatio(dpr);
    QPixmap pixmap = QPixmap::fromImage(image);
    if (store.size() > 512)
        store.clear();
    store.insert(key, pixmap);
    return pixmap;
}

QIcon icon(const QString &name, const QColor &color, int size)
{
    QIcon result;
    result.addPixmap(pixmap(name, color, size, 1.0));
    if (qApp && qApp->devicePixelRatio() > 1.0)
        result.addPixmap(pixmap(name, color, size, qApp->devicePixelRatio()));
    return result;
}

QPixmap logo(int size, qreal devicePixelRatio)
{
    const qreal dpr = effectiveDpr(devicePixelRatio);
    const int pixelSize = qMax(1, qRound(size * dpr));
    const QString key = QStringLiteral("logo|%1|%2").arg(size).arg(dpr);
    auto &store = cache();
    const auto it = store.constFind(key);
    if (it != store.cend())
        return it.value();

    QSvgRenderer renderer(QStringLiteral(":/icons/logo.svg"));
    if (!renderer.isValid())
        return {};

    QImage image(pixelSize, pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&painter, QRectF(0, 0, pixelSize, pixelSize));
    painter.end();
    image.setDevicePixelRatio(dpr);

    const QPixmap pixmap = QPixmap::fromImage(image);
    store.insert(key, pixmap);
    return pixmap;
}

} // namespace nyan::ui::Icons
