#pragma once

#include <QString>

namespace nyan::AppPaths {

/// Root directory for all persisted application data.  The NYAN_SSH_HOME
/// environment variable overrides the default so tests and portable builds can
/// keep their state isolated.
QString dataDir();

QString sessionsFile();
QString knownHostsFile();
QString settingsFile();
QString logsDir();
QString defaultDownloadDir();

bool ensureDataDir(QString *error = nullptr);

} // namespace nyan::AppPaths
