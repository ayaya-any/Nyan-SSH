#include "core/AppPaths.h"

#include <QDir>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace nyan::AppPaths {
namespace {

QString overrideHome()
{
    return QProcessEnvironment::systemEnvironment().value(QStringLiteral("NYAN_SSH_HOME")).trimmed();
}

} // namespace

QString dataDir()
{
    const QString override = overrideHome();
    if (!override.isEmpty())
        return QDir::cleanPath(override);

    const QString roaming = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir(roaming).filePath(QStringLiteral("NyanSSH"));
}

QString sessionsFile()
{
    return QDir(dataDir()).filePath(QStringLiteral("sessions.json"));
}

QString knownHostsFile()
{
    return QDir(dataDir()).filePath(QStringLiteral("known_hosts.json"));
}

QString settingsFile()
{
    return QDir(dataDir()).filePath(QStringLiteral("settings.ini"));
}

QString logsDir()
{
    return QDir(dataDir()).filePath(QStringLiteral("logs"));
}

QString defaultDownloadDir()
{
    QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloads.isEmpty())
        downloads = QDir::homePath();
    return QDir(downloads).filePath(QStringLiteral("NyanSSH"));
}

bool ensureDataDir(QString *error)
{
    QDir dir(dataDir());
    if (dir.exists())
        return true;
    if (dir.mkpath(QStringLiteral(".")))
        return true;
    if (error)
        *error = QStringLiteral("无法创建应用数据目录：%1").arg(dir.absolutePath());
    return false;
}

} // namespace nyan::AppPaths
