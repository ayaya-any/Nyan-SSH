#include "core/SftpTypes.h"

#include <QDir>
#include <QLocale>
#include <QStringList>

#include <libssh2_sftp.h>

namespace nyan {

QString SftpEntry::permissionText() const
{
    if (mode == 0)
        return {};
    QString text;
    if (isDirectory)
        text += QLatin1Char('d');
    else if (isSymlink)
        text += QLatin1Char('l');
    else
        text += QLatin1Char('-');

    const auto bit = [this](quint32 flag, QChar value) {
        return (mode & flag) ? value : QLatin1Char('-');
    };
    text += bit(LIBSSH2_SFTP_S_IRUSR, QLatin1Char('r'));
    text += bit(LIBSSH2_SFTP_S_IWUSR, QLatin1Char('w'));
    text += bit(LIBSSH2_SFTP_S_IXUSR, QLatin1Char('x'));
    text += bit(LIBSSH2_SFTP_S_IRGRP, QLatin1Char('r'));
    text += bit(LIBSSH2_SFTP_S_IWGRP, QLatin1Char('w'));
    text += bit(LIBSSH2_SFTP_S_IXGRP, QLatin1Char('x'));
    text += bit(LIBSSH2_SFTP_S_IROTH, QLatin1Char('r'));
    text += bit(LIBSSH2_SFTP_S_IWOTH, QLatin1Char('w'));
    text += bit(LIBSSH2_SFTP_S_IXOTH, QLatin1Char('x'));
    return text;
}

QString SftpEntry::sizeText() const
{
    if (isDirectory)
        return QStringLiteral("—");
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(size);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    if (unit == 0)
        return QStringLiteral("%1 B").arg(size);
    return QStringLiteral("%1 %2").arg(QLocale().toString(value, 'f', value < 10 ? 1 : 0), units[unit]);
}

QString SftpEntry::iconName() const
{
    if (isDirectory)
        return QStringLiteral("folder");
    if (isSymlink)
        return QStringLiteral("link");
    return QStringLiteral("file");
}

QString joinRemotePath(const QString &base, const QString &name)
{
    if (name.startsWith(QLatin1Char('/')))
        return normalizeRemotePath(name);
    QString result = base;
    if (result.isEmpty())
        result = QStringLiteral("/");
    if (!result.endsWith(QLatin1Char('/')))
        result += QLatin1Char('/');
    result += name;
    return normalizeRemotePath(result);
}

QString parentRemotePath(const QString &path)
{
    if (path.isEmpty() || path == QStringLiteral("/") || path == QStringLiteral("."))
        return QStringLiteral("/");
    QString normalized = normalizeRemotePath(path);
    if (normalized == QStringLiteral("/"))
        return normalized;
    const int slash = normalized.lastIndexOf(QLatin1Char('/'));
    if (slash <= 0)
        return QStringLiteral("/");
    return normalized.left(slash);
}

QString normalizeRemotePath(const QString &path)
{
    if (path.isEmpty())
        return QStringLiteral("/");
    const bool absolute = path.startsWith(QLatin1Char('/'));
    QStringList parts;
    const QStringList raw = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : raw) {
        if (part == QStringLiteral("."))
            continue;
        if (part == QStringLiteral("..")) {
            if (!parts.isEmpty())
                parts.removeLast();
            continue;
        }
        parts.append(part);
    }
    QString result = (absolute ? QStringLiteral("/") : QString()) + parts.join(QLatin1Char('/'));
    if (result.isEmpty())
        result = absolute ? QStringLiteral("/") : QStringLiteral(".");
    return result;
}

} // namespace nyan
