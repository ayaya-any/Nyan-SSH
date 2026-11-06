#pragma once

#include <QString>

namespace nyan {

enum class ThemeMode {
    Dark,
    Light,
};

class AppSettings {
public:
    explicit AppSettings(QString path = {});

    void load();
    void save();

    ThemeMode theme = ThemeMode::Dark;
    QString terminalFontFamily = QStringLiteral("Cascadia Mono");
    int terminalFontSize = 11;
    int scrollbackLines = 5000;
    bool copyOnSelect = false;
    bool confirmBeforeDelete = true;
    bool showHiddenFiles = false;
    bool confirmOnExit = true;
    bool autoOpenFilePanel = false;
    QString lastLocalDirectory;
    QString lastTerminalFontFamily;

    QString path() const { return m_path; }

    static QString themeName(ThemeMode mode);
    static ThemeMode themeFromName(const QString &name);

private:
    QString m_path;
};

} // namespace nyan
