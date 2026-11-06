// Exercises the production SftpWorker: mkdir, upload, download, rename,
// recursive delete.  Requires the local test server.

#include "core/SftpWorker.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QMetaObject>

using namespace nyan;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    QString fingerprint = QStringLiteral("SHA256:TH85xwODSs/tZ69umL1etFtGg29Z/kUCmBFpimbB43k");
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i) == QStringLiteral("--fingerprint") && i + 1 < args.size())
            fingerprint = args.at(++i);
    }

    qRegisterMetaType<nyan::SftpEntry>("nyan::SftpEntry");
    qRegisterMetaType<QVector<nyan::SftpEntry>>("QVector<nyan::SftpEntry>");

    QTemporaryDir temp;
    if (!temp.isValid())
        return 1;
    const QString localUpload = temp.filePath(QStringLiteral("upload.txt"));
    {
        QFile file(localUpload);
        if (!file.open(QIODevice::WriteOnly))
            return 1;
        file.write("nyan sftp worker integration\nline two\n");
    }
    const QString localDownload = temp.filePath(QStringLiteral("download.txt"));
    const QString remoteDir = QStringLiteral("/nyan-sftp-worker-%1")
                                  .arg(QDateTime::currentSecsSinceEpoch());
    const QString remoteFile = remoteDir + QStringLiteral("/upload.txt");
    const QString remoteRenamed = remoteDir + QStringLiteral("/renamed.txt");

    QThread thread;
    auto *worker = new SftpWorker;
    worker->moveToThread(&thread);

    QString stage = QStringLiteral("root-list");
    QString failure;
    bool succeeded = false;

    SshConfig config;
    config.host = QStringLiteral("127.0.0.1");
    config.port = 2222;
    config.username = QStringLiteral("nyan");
    config.auth = AuthType::Password;
    config.password = QStringLiteral("nyan123");

    auto fail = [&](const QString &message) {
        failure = message;
        QMetaObject::invokeMethod(worker, &SftpWorker::close, Qt::QueuedConnection);
        QTimer::singleShot(200, &app, &QCoreApplication::quit);
    };

    QObject::connect(worker, &SftpWorker::openFailed, &app, [&](const QString &message) {
        fail(QStringLiteral("open failed: %1").arg(message));
    });
    QObject::connect(worker, &SftpWorker::opened, &app, [&](const QString &home, const QString &) {
        out << "opened, home=" << home << "\n";
        QMetaObject::invokeMethod(worker, [=] { worker->listDirectory(QStringLiteral("/")); },
                                  Qt::QueuedConnection);
    });
    QObject::connect(worker, &SftpWorker::directoryListFailed, &app,
                     [&](const QString &path, const QString &message) {
                         fail(QStringLiteral("list %1 failed: %2").arg(path, message));
                     });
    QObject::connect(worker, &SftpWorker::directoryListed, &app,
                     [&](const QString &path, const QVector<SftpEntry> &entries) {
                         QStringList names;
                         for (const SftpEntry &entry : entries)
                             names.append(entry.name);
                         if (stage == QStringLiteral("root-list")) {
                             if (!names.contains(QStringLiteral("README.md"))) {
                                 fail(QStringLiteral("README.md missing from /"));
                                 return;
                             }
                             stage = QStringLiteral("mkdir");
                             out << "root listing ok\n";
                             QMetaObject::invokeMethod(
                                 worker, [=] { worker->makeDirectory(remoteDir); },
                                 Qt::QueuedConnection);
                         } else if (stage == QStringLiteral("verify-rename")) {
                             if (!names.contains(QStringLiteral("renamed.txt"))
                                 || names.contains(QStringLiteral("upload.txt"))) {
                                 fail(QStringLiteral("rename did not take effect: %1")
                                          .arg(names.join(QStringLiteral(","))));
                                 return;
                             }
                             stage = QStringLiteral("delete");
                             out << "rename listing ok\n";
                             QMetaObject::invokeMethod(
                                 worker, [=] { worker->removeEntry(remoteDir, true); },
                                 Qt::QueuedConnection);
                         }
                     });
    QObject::connect(worker, &SftpWorker::operationFinished, &app,
                     [&](const QString &operation, const QString &path, bool ok,
                         const QString &message) {
                         if (!ok) {
                             fail(QStringLiteral("%1 %2 failed: %3").arg(operation, path, message));
                             return;
                         }
                         out << "op " << operation << " " << path << " ok\n";
                         if (operation == QStringLiteral("mkdir")
                             && stage == QStringLiteral("mkdir")) {
                             stage = QStringLiteral("upload");
                             QMetaObject::invokeMethod(
                                 worker,
                                 [=] { worker->uploadFile(1, localUpload, remoteFile); },
                                 Qt::QueuedConnection);
                         } else if (operation == QStringLiteral("rename")
                                    && stage == QStringLiteral("rename")) {
                             stage = QStringLiteral("verify-rename");
                             QMetaObject::invokeMethod(
                                 worker, [=] { worker->listDirectory(remoteDir); },
                                 Qt::QueuedConnection);
                         } else if (operation == QStringLiteral("remove")
                                    && stage == QStringLiteral("delete")) {
                             succeeded = true;
                             out << "recursive delete ok\n";
                             QMetaObject::invokeMethod(worker, &SftpWorker::close,
                                                       Qt::QueuedConnection);
                             QTimer::singleShot(200, &app, &QCoreApplication::quit);
                         }
                     });
    QObject::connect(worker, &SftpWorker::transferFinished, &app,
                     [&](int id, bool ok, const QString &message) {
                         if (!ok) {
                             fail(QStringLiteral("transfer %1 failed: %2").arg(id).arg(message));
                             return;
                         }
                         if (id == 1 && stage == QStringLiteral("upload")) {
                             stage = QStringLiteral("download");
                             out << "upload ok\n";
                             QMetaObject::invokeMethod(
                                 worker,
                                 [=] { worker->downloadFile(2, remoteFile, localDownload); },
                                 Qt::QueuedConnection);
                         } else if (id == 2 && stage == QStringLiteral("download")) {
                             QFile file(localDownload);
                             if (!file.open(QIODevice::ReadOnly)
                                 || file.readAll() != QByteArrayLiteral("nyan sftp worker integration\nline two\n")) {
                                 fail(QStringLiteral("downloaded content mismatch"));
                                 return;
                             }
                             stage = QStringLiteral("rename");
                             out << "download ok\n";
                             QMetaObject::invokeMethod(
                                 worker,
                                 [=] { worker->renameEntry(remoteFile, remoteRenamed); },
                                 Qt::QueuedConnection);
                         }
                     });
    QObject::connect(worker, &SftpWorker::errorOccurred, &app, [&](const QString &message) {
        out << "worker error: " << message << "\n";
    });
    QObject::connect(worker, &SftpWorker::closed, &thread, &QThread::quit);

    QTimer::singleShot(40000, &app, [&] {
        failure = QStringLiteral("timeout in stage %1").arg(stage);
        QMetaObject::invokeMethod(worker, &SftpWorker::close, Qt::QueuedConnection);
        QMetaObject::invokeMethod(&app, &QCoreApplication::quit, Qt::QueuedConnection);
    });

    thread.start();
    QMetaObject::invokeMethod(
        worker, [=] { worker->open(config, fingerprint); }, Qt::QueuedConnection);

    app.exec();
    thread.quit();
    thread.wait(5000);
    delete worker;

    if (!failure.isEmpty()) {
        out << "FAIL: " << failure << "\n";
        return 1;
    }
    if (!succeeded) {
        out << "FAIL: test did not complete\n";
        return 1;
    }
    out << "ALL OK\n";
    return 0;
}
