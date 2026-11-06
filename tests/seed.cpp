// Development helper: seed an isolated NYAN_SSH_HOME with a saved session so
// the GUI can be exercised without manual typing.  Not part of the app.

#include "core/AppPaths.h"
#include "core/AppSettings.h"
#include "core/HostKeyStore.h"
#include "core/SessionProfile.h"
#include "core/SessionStore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QTextStream>

using namespace nyan;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();

    QString home;
    QString host = QStringLiteral("127.0.0.1");
    quint16 port = 2222;
    QString user = QStringLiteral("nyan");
    QString password = QStringLiteral("nyan123");
    QString passphrase;
    QString keyPath;
    QString name = QStringLiteral("本地实验台");
    QString fingerprint;
    bool autoLogin = true;
    bool sftp = true;
    bool light = false;

    for (int i = 1; i < args.size(); ++i) {
        const QString &arg = args.at(i);
        auto next = [&]() { return i + 1 < args.size() ? args.at(++i) : QString(); };
        if (arg == QStringLiteral("--home"))
            home = next();
        else if (arg == QStringLiteral("--host"))
            host = next();
        else if (arg == QStringLiteral("--port"))
            port = static_cast<quint16>(next().toUShort());
        else if (arg == QStringLiteral("--user"))
            user = next();
        else if (arg == QStringLiteral("--password"))
            password = next();
        else if (arg == QStringLiteral("--key"))
            keyPath = next();
        else if (arg == QStringLiteral("--passphrase"))
            passphrase = next();
        else if (arg == QStringLiteral("--name"))
            name = next();
        else if (arg == QStringLiteral("--fingerprint"))
            fingerprint = next();
        else if (arg == QStringLiteral("--no-auto"))
            autoLogin = false;
        else if (arg == QStringLiteral("--no-sftp"))
            sftp = false;
        else if (arg == QStringLiteral("--light"))
            light = true;
    }

    if (home.isEmpty()) {
        QTextStream(stderr) << "--home is required\n";
        return 2;
    }
    qputenv("NYAN_SSH_HOME", home.toUtf8());
    AppPaths::ensureDataDir();

    AppSettings settings;
    settings.load();
    settings.theme = light ? ThemeMode::Light : ThemeMode::Dark;
    settings.autoOpenFilePanel = sftp;
    settings.confirmOnExit = false;
    settings.save();

    SessionStore store;
    store.load();
    SessionProfile profile;
    profile.name = name;
    profile.host = host;
    profile.port = port;
    profile.username = user;
    if (!keyPath.isEmpty()) {
        profile.auth = AuthType::PublicKey;
        profile.privateKeyPath = keyPath;
        profile.setPassphrase(passphrase, false);
        profile.rememberPassword = false;
    } else {
        profile.auth = AuthType::Password;
        profile.setPassword(password, true);
        profile.rememberPassword = true;
    }
    profile.autoLogin = autoLogin;
    profile.color = SessionProfile::defaultColorFor(host + user);
    profile.lastUsedAt = QDateTime::currentDateTimeUtc();
    store.upsert(profile);
    QString error;
    if (!store.save(&error)) {
        QTextStream(stderr) << error << "\n";
        return 1;
    }

    if (!fingerprint.isEmpty()) {
        HostKeyStore knownHosts;
        knownHosts.load();
        knownHosts.trust(host, port, QStringLiteral("ecdsa-sha2-nistp256"), fingerprint);
        if (!knownHosts.save(&error)) {
            QTextStream(stderr) << error << "\n";
            return 1;
        }
    }

    QTextStream(stdout) << "seeded " << profile.name << " " << profile.subtitle() << " at " << home
                        << "\n";
    return 0;
}
