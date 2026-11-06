#include "core/AppSettings.h"

#include "core/AppPaths.h"

#include <QSettings>

namespace nyan {

AppSettings::AppSettings(QString path)
    : m_path(path.isEmpty() ? AppPaths::settingsFile() : std::move(path))
{
}

void AppSettings::load()
{
    QSettings settings(m_path, QSettings::IniFormat);

    theme = themeFromName(settings.value(QStringLiteral("appearance/theme"), QStringLiteral("dark")).toString());
    terminalFontFamily = settings
                             .value(QStringLiteral("terminal/fontFamily"), QStringLiteral("Cascadia Mono"))
                             .toString();
    terminalFontSize = settings.value(QStringLiteral("terminal/fontSize"), 11).toInt();
    scrollbackLines = settings.value(QStringLiteral("terminal/scrollback"), 5000).toInt();
    copyOnSelect = settings.value(QStringLiteral("terminal/copyOnSelect"), false).toBool();
    confirmBeforeDelete = settings.value(QStringLiteral("files/confirmDelete"), true).toBool();
    showHiddenFiles = settings.value(QStringLiteral("files/showHidden"), false).toBool();
    confirmOnExit = settings.value(QStringLiteral("app/confirmExit"), true).toBool();
    autoOpenFilePanel = settings.value(QStringLiteral("app/autoOpenFilePanel"), false).toBool();
    lastLocalDirectory = settings.value(QStringLiteral("files/lastLocalDirectory")).toString();
    lastTerminalFontFamily = settings.value(QStringLiteral("terminal/lastFontFamily")).toString();

    terminalFontSize = qBound(7, terminalFontSize, 32);
    scrollbackLines = qBound(500, scrollbackLines, 100000);
}

void AppSettings::save()
{
    AppPaths::ensureDataDir();
    QSettings settings(m_path, QSettings::IniFormat);

    settings.setValue(QStringLiteral("appearance/theme"), themeName(theme));
    settings.setValue(QStringLiteral("terminal/fontFamily"), terminalFontFamily);
    settings.setValue(QStringLiteral("terminal/fontSize"), terminalFontSize);
    settings.setValue(QStringLiteral("terminal/scrollback"), scrollbackLines);
    settings.setValue(QStringLiteral("terminal/copyOnSelect"), copyOnSelect);
    settings.setValue(QStringLiteral("files/confirmDelete"), confirmBeforeDelete);
    settings.setValue(QStringLiteral("files/showHidden"), showHiddenFiles);
    settings.setValue(QStringLiteral("app/confirmExit"), confirmOnExit);
    settings.setValue(QStringLiteral("app/autoOpenFilePanel"), autoOpenFilePanel);
    settings.setValue(QStringLiteral("files/lastLocalDirectory"), lastLocalDirectory);
    settings.setValue(QStringLiteral("terminal/lastFontFamily"), lastTerminalFontFamily);
    settings.sync();
}

QString AppSettings::themeName(ThemeMode mode)
{
    return mode == ThemeMode::Light ? QStringLiteral("light") : QStringLiteral("dark");
}

ThemeMode AppSettings::themeFromName(const QString &name)
{
    return name.compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0 ? ThemeMode::Light : ThemeMode::Dark;
}

} // namespace nyan
