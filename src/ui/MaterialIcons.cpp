#include "ui/MaterialIcons.h"

#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QPainter>

namespace nyan::ui::MaterialIcons {
namespace {

const QString &family()
{
    static const QString value = [] {
        const int fontId =
            QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/MaterialIcons-Regular.ttf"));
        if (fontId < 0)
            return QString();
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        return families.isEmpty() ? QString() : families.first();
    }();
    return value;
}

const QHash<QString, char32_t> &glyphs()
{
    static const QHash<QString, char32_t> map = [] {
        QHash<QString, char32_t> aliases = {
            {QStringLiteral("alert"), 0xE002},          // warning
            {QStringLiteral("arrow-left"), 0xE5C4},     // arrow_back
            {QStringLiteral("arrow-up"), 0xE5D8},       // arrow_upward
            {QStringLiteral("check-circle"), 0xE86C},   // check_circle
            {QStringLiteral("check"), 0xE5CA},
            {QStringLiteral("chevron_down_dark"), 0xE313},  // keyboard_arrow_down
            {QStringLiteral("chevron_down_light"), 0xE313},
            {QStringLiteral("chevron_up_dark"), 0xE316},    // keyboard_arrow_up
            {QStringLiteral("chevron_up_light"), 0xE316},
            {QStringLiteral("chevron-down"), 0xE313},
            {QStringLiteral("chevron-left"), 0xE5CB},
            {QStringLiteral("chevron-right"), 0xE5CC},
            {QStringLiteral("chevron-up"), 0xE316},
            {QStringLiteral("clock"), 0xE8B5},           // schedule
            {QStringLiteral("close_dark"), 0xE5CD},
            {QStringLiteral("close_light"), 0xE5CD},
            {QStringLiteral("close"), 0xE5CD},
            {QStringLiteral("copy"), 0xE14D},            // content_copy
            {QStringLiteral("database"), 0xE1DB},        // storage
            {QStringLiteral("download"), 0xE2C4},        // file_download
            {QStringLiteral("edit"), 0xE3C9},
            {QStringLiteral("external"), 0xE89E},        // open_in_new
            {QStringLiteral("eye-off"), 0xE8F5},         // visibility_off
            {QStringLiteral("eye"), 0xE8F4},             // visibility
            {QStringLiteral("file-text"), 0xE873},       // description
            {QStringLiteral("file"), 0xE24D},            // insert_drive_file
            {QStringLiteral("folder-open"), 0xE2C8},     // folder_open
            {QStringLiteral("folder-plus"), 0xE2CC},     // create_new_folder
            {QStringLiteral("folder"), 0xE2C7},
            {QStringLiteral("globe"), 0xE80B},           // public
            {QStringLiteral("home"), 0xE88A},
            {QStringLiteral("image"), 0xE3F4},
            {QStringLiteral("info"), 0xE88E},
            {QStringLiteral("key"), 0xE0DA},             // vpn_key
            {QStringLiteral("link"), 0xE157},
            {QStringLiteral("lock"), 0xE897},
            {QStringLiteral("log-in"), 0xEA77},          // login
            {QStringLiteral("log-out"), 0xE9BA},         // logout
            {QStringLiteral("maximize"), 0xE3C6},        // crop_square
            {QStringLiteral("minimize"), 0xE15B},        // remove
            {QStringLiteral("more"), 0xE5D3},            // more_horiz
            {QStringLiteral("panel-right"), 0xF114},     // view_sidebar
            {QStringLiteral("paste"), 0xE14F},           // content_paste
            {QStringLiteral("plus"), 0xE145},            // add
            {QStringLiteral("power"), 0xE8AC},           // power_settings_new
            {QStringLiteral("refresh"), 0xE5D5},
            {QStringLiteral("restore"), 0xE3E0},         // filter_none
            {QStringLiteral("search"), 0xE8B6},
            {QStringLiteral("server"), 0xE875},          // dns
            {QStringLiteral("settings"), 0xE8B8},
            {QStringLiteral("shield"), 0xE32A},          // security
            {QStringLiteral("sparkles"), 0xE65F},        // auto_awesome
            {QStringLiteral("terminal"), 0xEB8E},
            {QStringLiteral("trash"), 0xE872},           // delete
            {QStringLiteral("upload"), 0xE2C6},          // file_upload
            {QStringLiteral("wifi-off"), 0xE648},        // wifi_off
            {QStringLiteral("wifi"), 0xE63E},
            {QStringLiteral("x-circle"), 0xE5C9},        // cancel
        };

        // The official codepoints file makes every Material icon name
        // available, not just the aliases used by the current UI.
        QFile file(QStringLiteral(":/fonts/MaterialIcons-Regular.codepoints"));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!file.atEnd()) {
                const QByteArray line = file.readLine().trimmed();
                const int separator = line.indexOf(' ');
                if (separator <= 0)
                    continue;
                bool ok = false;
                const uint codepoint = line.mid(separator + 1).trimmed().toUInt(&ok, 16);
                if (ok)
                    aliases.insert(QString::fromLatin1(line.left(separator)),
                                   static_cast<char32_t>(codepoint));
            }
        }
        return aliases;
    }();
    return map;
}

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

bool available()
{
    return !family().isEmpty();
}

QString familyName()
{
    return family();
}

QPixmap pixmap(const QString &name, const QColor &color, int size, qreal devicePixelRatio)
{
    if (size <= 0 || family().isEmpty())
        return {};

    const auto glyph = glyphs().constFind(name);
    if (glyph == glyphs().cend())
        return {};

    const qreal dpr = effectiveDpr(devicePixelRatio);
    const QString key = QStringLiteral("material|%1|%2|%3|%4")
                            .arg(name, color.name(QColor::HexArgb))
                            .arg(size)
                            .arg(dpr);
    auto &store = cache();
    if (const auto it = store.constFind(key); it != store.cend())
        return it.value();

    const int pixelSize = qMax(1, qRound(size * dpr));
    QImage image(pixelSize, pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    image.setDevicePixelRatio(dpr);

    QFont font(family());
    // Icon fonts carry a little internal padding; compensate so the visual
    // weight is close to the previous SVG icon set at the same logical size.
    font.setPixelSize(qMax(1, qRound(size * 1.30)));
    font.setStyleStrategy(QFont::PreferAntialias);

    const QString text(1, QChar(static_cast<char16_t>(glyph.value())));
    const QFontMetricsF metrics(font);
    const QRectF ink = metrics.tightBoundingRect(text);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setFont(font);
    painter.setPen(color);
    if (!ink.isEmpty()) {
        const qreal x = (size - ink.width()) / 2.0 - ink.left();
        const qreal y = (size - ink.height()) / 2.0 - ink.top();
        painter.drawText(QPointF(x, y), text);
    }
    painter.end();

    QPixmap result = QPixmap::fromImage(image);
    if (store.size() > 512)
        store.clear();
    store.insert(key, result);
    return result;
}

} // namespace nyan::ui::MaterialIcons
