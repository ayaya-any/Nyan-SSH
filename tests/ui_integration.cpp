// Integration test for the production FilePanel + SftpWorker stack.
// Requires tests/server/test_ssh_server.py on 127.0.0.1:2222.
//
// Usage: nyan_ui_integration --fingerprint SHA256:...

#include "core/AppSettings.h"
#include "core/SshConnection.h"
#include "ui/FilePanel.h"
#include "ui/MaterialIcons.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTableView>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QtTest>

using namespace nyan;

namespace {

bool waitUntil(const std::function<bool()> &predicate, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (predicate())
            return true;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
        QThread::msleep(10);
    }
    return predicate();
}

QStringList modelNames(QTableView *table)
{
    QStringList names;
    if (!table || !table->model())
        return names;
    for (int row = 0; row < table->model()->rowCount(); ++row)
        names.append(table->model()->index(row, 0).data(Qt::DisplayRole).toString());
    return names;
}

int removeRemoteFile(const QString &fingerprint, const QString &remotePath)
{
    SshConfig config;
    config.host = QStringLiteral("127.0.0.1");
    config.port = 2222;
    config.username = QStringLiteral("nyan");
    config.auth = AuthType::Password;
    config.password = QStringLiteral("nyan123");

    SshConnection connection;
    QString error;
    const auto accept = [](const QString &, const QString &, const QString &) { return true; };
    if (!connection.connect(config, fingerprint, accept, &error))
        return 1;
    LIBSSH2_SFTP *sftp = connection.openSftp(&error);
    if (!sftp)
        return 1;
    libssh2_sftp_unlink(sftp, remotePath.toUtf8().constData());
    libssh2_sftp_shutdown(sftp);
    connection.disconnect();
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    Q_INIT_RESOURCE(resources);
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    ui::Theme::instance().setMode(ThemeMode::Dark);

    if (!ui::MaterialIcons::available()
        || ui::MaterialIcons::pixmap(QStringLiteral("settings"), QColor(Qt::white), 18).isNull()
        || ui::MaterialIcons::pixmap(QStringLiteral("cloud_upload"), QColor(Qt::white), 24)
               .isNull()) {
        qCritical() << "FAIL: Material Icons font could not be loaded";
        return 1;
    }

    QString fingerprint = QStringLiteral("SHA256:TH85xwODSs/tZ69umL1etFtGg29Z/kUCmBFpimbB43k");
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i) == QStringLiteral("--fingerprint") && i + 1 < args.size())
            fingerprint = args.at(++i);
    }

    QTextStream out(stdout);
    QTemporaryDir localDir;
    if (!localDir.isValid()) {
        out << "FAIL: temp dir\n";
        return 1;
    }
    const QString localFile = localDir.filePath(QStringLiteral("nyan-ui-upload.txt"));
    {
        QFile file(localFile);
        file.open(QIODevice::WriteOnly);
        file.write("uploaded by nyan ui integration\n");
    }

    AppSettings settings;
    settings.autoOpenFilePanel = false;
    settings.showHiddenFiles = false;

    ui::FilePanel panel;
    panel.setSettings(&settings);
    panel.resize(560, 720);
    panel.show();

    SshConfig config;
    config.host = QStringLiteral("127.0.0.1");
    config.port = 2222;
    config.username = QStringLiteral("nyan");
    config.auth = AuthType::Password;
    config.password = QStringLiteral("nyan123");
    config.terminalType = QStringLiteral("xterm-256color");

    panel.openSession(config, fingerprint, QStringLiteral("/"));
    if (!waitUntil([&] { return panel.isReady(); }, 12000)) {
        out << "FAIL: sftp panel never became ready\n";
        return 1;
    }
    auto *table = panel.findChild<QTableView *>();
    if (!table) {
        out << "FAIL: file table not found\n";
        return 1;
    }
    if (!waitUntil([&] { return table->model() && table->model()->rowCount() > 0; }, 12000)) {
        out << "FAIL: directory listing empty\n";
        return 1;
    }
    QStringList names = modelNames(table);
    out << "root entries: " << names.join(QStringLiteral(", ")) << "\n";
    if (!names.contains(QStringLiteral("projects"))) {
        out << "FAIL: projects missing\n";
        return 1;
    }

    // Navigate into projects by double clicking the row.
    int projectsRow = -1;
    for (int row = 0; row < table->model()->rowCount(); ++row) {
        if (table->model()->index(row, 0).data().toString() == QStringLiteral("projects")) {
            projectsRow = row;
            break;
        }
    }
    if (projectsRow < 0) {
        out << "FAIL: projects row missing\n";
        return 1;
    }
    table->scrollTo(table->model()->index(projectsRow, 0));
    // Exercise the view's doubleClicked contract directly; the mouse mapping
    // itself is standard Qt behaviour and is verified in the GUI manually.
    QMetaObject::invokeMethod(table, "doubleClicked", Qt::DirectConnection,
                              Q_ARG(QModelIndex, table->model()->index(projectsRow, 0)));
    if (!waitUntil([&] { return panel.currentPath() == QStringLiteral("/projects"); }, 8000)) {
        out << "FAIL: navigation to /projects failed, current=" << panel.currentPath() << "\n";
        return 1;
    }
    if (!waitUntil([&] {
            const QStringList current = modelNames(table);
            return current.contains(QStringLiteral("nyan-web")) && current.contains(QStringLiteral("nyan-core"));
        },
                   8000)) {
        out << "FAIL: projects listing unexpected: " << modelNames(table).join(QStringLiteral(", ")) << "\n";
        return 1;
    }
    out << "navigation ok\n";

    panel.uploadLocalFiles({localFile});
    if (!waitUntil([&] { return modelNames(table).contains(QStringLiteral("nyan-ui-upload.txt")); }, 15000)) {
        out << "FAIL: uploaded file did not appear: " << modelNames(table).join(QStringLiteral(", ")) << "\n";
        return 1;
    }
    out << "upload ok\n";

    removeRemoteFile(fingerprint, QStringLiteral("/projects/nyan-ui-upload.txt"));
    panel.shutdown();
    out << "ALL OK\n";
    return 0;
}
