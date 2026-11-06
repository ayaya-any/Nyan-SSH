// Render real dialogs for visual verification.  Usage:
//   nyan_dialog_probe settings|connection|hostkey|password|hostkey-mismatch [light]

#include "core/AppSettings.h"
#include "ui/AuthDialogs.h"
#include "ui/ConnectionDialog.h"
#include "ui/MaterialWidgets.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QLabel>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

using namespace nyan;
using namespace nyan::ui;

int main(int argc, char **argv)
{
    Q_INIT_RESOURCE(resources);
    QApplication app(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    QApplication::setApplicationName(QStringLiteral("Nyan SSH"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    const QStringList args = app.arguments();
    const QString mode = args.size() > 1 ? args.at(1) : QStringLiteral("settings");
    const bool light = args.contains(QStringLiteral("light"));
    Theme::instance().setMode(light ? ThemeMode::Light : ThemeMode::Dark);

    auto *background = new QWidget;
    background->setStyleSheet(
        QStringLiteral("background:%1;").arg(Theme::instance().palette().window.name()));
    background->resize(900, 760);
    background->move(80, 80);
    background->show();

    MDialog *dialog = nullptr;
    if (mode == QStringLiteral("settings")) {
        AppSettings settings;
        settings.theme = light ? ThemeMode::Light : ThemeMode::Dark;
        dialog = new SettingsDialog(background, settings);
    } else if (mode == QStringLiteral("connection")) {
        dialog = new ConnectionDialog(background, ConnectionDialog::Mode::Create);
    } else if (mode == QStringLiteral("connection-first-color")) {
        SessionProfile profile;
        profile.color = SessionProfile::accentPalette().first();
        dialog = new ConnectionDialog(background, ConnectionDialog::Mode::Create, &profile);
    } else if (mode == QStringLiteral("connection-switch")) {
        dialog = new ConnectionDialog(background, ConnectionDialog::Mode::Create);
    } else if (mode == QStringLiteral("hostkey") || mode == QStringLiteral("hostkey-mismatch")) {
        const bool mismatch = mode.endsWith(QStringLiteral("mismatch"));
        dialog = new HostKeyDialog(
            background, QStringLiteral("127.0.0.1"), 2222, QStringLiteral("ecdsa-sha2-nistp256"),
            QStringLiteral("SHA256:TH85xwODSs/tZ69umL1etFtGg29Z/kUCmBFpimbB43k"),
            mismatch ? QStringLiteral("SHA256:OLDOLDOLDOLDOLDOLDOLDOLDOLDOLDOLDOLDOLD")
                    : QString());
    } else if (mode == QStringLiteral("password")) {
        dialog = new PasswordDialog(background, PasswordDialog::Mode::Password,
                                    QStringLiteral("nyan@127.0.0.1:2222"),
                                    QStringLiteral("认证失败，请检查用户名与凭据。服务器允许：password"),
                                    true);
    }

    if (!dialog)
        return 2;
    dialog->show();
    dialog->raise();
    dialog->activateWindow();

    const QString output = args.size() > 2 && !args.at(2).startsWith(QStringLiteral("light"))
                               ? args.at(2)
                               : QStringLiteral("dialog.png");

    if (mode == QStringLiteral("connection-switch")) {
        auto *segmented = dialog->findChild<MSegmentedControl *>();
        if (!segmented)
            return 3;
        QTimer::singleShot(300, &app, [dialog, segmented] {
            qInfo().noquote() << "password height:" << dialog->height();
            segmented->setCurrentIndex(1);
        });
        QTimer::singleShot(650, &app, [dialog, segmented] {
            qInfo().noquote() << "key height:" << dialog->height();
            segmented->setCurrentIndex(0);
        });
        QTimer::singleShot(1000, &app, [dialog, &app, &output] {
            qInfo().noquote() << "back-to-password height:" << dialog->height();
            dialog->grab().save(output);
            app.quit();
        });
        return app.exec();
    }

    QTimer::singleShot(900, &app, [&] {
        QPixmap pixmap;
        if (mode == QStringLiteral("connection-first-color")) {
            pixmap = dialog->grab();
        } else {
            const QRect geometry = dialog->frameGeometry();
            if (QScreen *screen = dialog->screen())
                pixmap = screen->grabWindow(0, geometry.x(), geometry.y(), geometry.width(), geometry.height());
            if (pixmap.isNull())
                pixmap = dialog->grab();
        }
        pixmap.save(output);
        app.quit();
    });

    return app.exec();
}
