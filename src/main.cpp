#include "core/AppSettings.h"
#include "core/SessionProfile.h"
#include "ui/Icons.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QStyleFactory>
#include <QTimer>

using namespace nyan;

int main(int argc, char **argv)
{
    Q_INIT_RESOURCE(resources);

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QApplication::setOrganizationName(QStringLiteral("Nyan"));
    QApplication::setOrganizationDomain(QStringLiteral("nyan.dev"));
    QApplication::setApplicationName(QStringLiteral("Nyan SSH"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setApplicationDisplayName(QStringLiteral("Nyan SSH"));
    QApplication::setWindowIcon(QIcon(ui::Icons::logo(64, 2.0)));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Material 风格 SSH 客户端"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption connectOption(
        {QStringLiteral("c"), QStringLiteral("connect")},
        QStringLiteral("连接 user@host[:port]"), QStringLiteral("target"));
    const QCommandLineOption passwordOption(QStringLiteral("password"),
                                            QStringLiteral("命令行连接使用的密码"), QStringLiteral("password"));
    const QCommandLineOption keyOption(QStringLiteral("key"), QStringLiteral("命令行连接使用的私钥"),
                                       QStringLiteral("path"));
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                              QStringLiteral("渲染后保存窗口截图（开发用途）"),
                                              QStringLiteral("path"));
    const QCommandLineOption exitAfterOption(QStringLiteral("exit-after"),
                                             QStringLiteral("指定毫秒后退出（开发用途）"),
                                             QStringLiteral("ms"));
    parser.addOption(connectOption);
    parser.addOption(passwordOption);
    parser.addOption(keyOption);
    parser.addOption(screenshotOption);
    parser.addOption(exitAfterOption);
    parser.addPositionalArgument(QStringLiteral("target"), QStringLiteral("可选的 user@host[:port]"), QStringLiteral("[target]"));
    parser.process(app);

    ui::MainWindow window;
    window.show();

    QString target = parser.value(connectOption);
    if (target.isEmpty() && !parser.positionalArguments().isEmpty())
        target = parser.positionalArguments().first();
    if (!target.isEmpty()) {
        QString user = QStringLiteral("root");
        QString host = target;
        const int at = target.indexOf(QLatin1Char('@'));
        if (at >= 0) {
            user = target.left(at);
            host = target.mid(at + 1);
        }
        quint16 port = 22;
        const int colon = host.lastIndexOf(QLatin1Char(':'));
        if (colon > 0 && !host.contains(QLatin1Char(']'))) {
            const QString portText = host.mid(colon + 1);
            bool ok = false;
            const int parsed = portText.toInt(&ok);
            if (ok && parsed > 0 && parsed < 65536) {
                port = static_cast<quint16>(parsed);
                host = host.left(colon);
            }
        } else if (host.startsWith(QLatin1Char('[')) && host.contains(QStringLiteral("]:"))) {
            const int bracket = host.indexOf(QStringLiteral("]:"));
            const QString portText = host.mid(bracket + 2);
            bool ok = false;
            const int parsed = portText.toInt(&ok);
            if (ok) {
                port = static_cast<quint16>(parsed);
                host = host.mid(1, bracket - 1);
            }
        }

        SessionProfile profile;
        profile.name = QStringLiteral("%1@%2").arg(user, host);
        profile.host = host;
        profile.port = port;
        profile.username = user;
        profile.color = SessionProfile::defaultColorFor(host + user);
        profile.rememberPassword = false;
        const QString keyPath = parser.value(keyOption);
        if (!keyPath.isEmpty()) {
            profile.auth = AuthType::PublicKey;
            profile.privateKeyPath = QFileInfo(keyPath).absoluteFilePath();
        } else {
            profile.auth = AuthType::Password;
            profile.setPassword(parser.value(passwordOption), false);
        }
        const QString finalTarget = profile.name;
        QTimer::singleShot(120, &window, [&window, profile] { window.connectQuick(profile); });
        Q_UNUSED(finalTarget);    }

    const QString screenshot = parser.value(screenshotOption);
    const QString exitAfterText = parser.value(exitAfterOption);
    if (!screenshot.isEmpty()) {
        const int delay = exitAfterText.isEmpty() ? 2600 : qBound(500, exitAfterText.toInt(), 20000);
        QTimer::singleShot(delay, &app, [&window, screenshot, &app] {
            window.grab().save(screenshot);
            app.quit();
        });
    } else if (!exitAfterText.isEmpty()) {
        QTimer::singleShot(qBound(100, exitAfterText.toInt(), 60000), &app, &QCoreApplication::quit);
    }

    return app.exec();
}
