// End-to-end backend smoke test against tests/server/test_ssh_server.py.
//
// Usage:
//   nyan_smoke --host 127.0.0.1 --port 2222 --user nyan --password nyan123
//              [--fingerprint SHA256:...] [--key path]
//
// The test authenticates, opens a PTY shell, runs a command, and then exercises
// SFTP listing, stat, upload, download and cleanup.  Exit code 0 means success.

#include "core/SshConnection.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QTextStream>
#include <QThread>

#include <libssh2_sftp.h>

using namespace nyan;

namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

int fail(const QString &message)
{
    out() << "FAIL: " << message << "\n";
    out().flush();
    return 1;
}

bool readUntil(LIBSSH2_CHANNEL *channel, const QByteArray &needle, QByteArray *collected, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    char buffer[8192];
    while (timer.elapsed() < timeoutMs) {
        const auto read = libssh2_channel_read_ex(channel, 0, buffer, sizeof(buffer));
        if (read > 0) {
            collected->append(buffer, static_cast<int>(read));
            if (collected->contains(needle))
                return true;
        } else if (read < 0 && read != LIBSSH2_ERROR_EAGAIN) {
            return false;
        } else {
            QThread::msleep(20);
        }
    }
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("nyan_smoke"));

    QString host = QStringLiteral("127.0.0.1");
    quint16 port = 2222;
    QString user = QStringLiteral("nyan");
    QString password = QStringLiteral("nyan123");
    QString keyPath;
    QString expectedFingerprint;

    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString &arg = args.at(i);
        auto next = [&]() -> QString { return i + 1 < args.size() ? args.at(++i) : QString(); };
        if (arg == QStringLiteral("--host"))
            host = next();
        else if (arg == QStringLiteral("--port"))
            port = static_cast<quint16>(next().toUShort());
        else if (arg == QStringLiteral("--user"))
            user = next();
        else if (arg == QStringLiteral("--password"))
            password = next();
        else if (arg == QStringLiteral("--key"))
            keyPath = next();
        else if (arg == QStringLiteral("--fingerprint"))
            expectedFingerprint = next();
    }

    SshConfig config;
    config.host = host;
    config.port = port;
    config.username = user;
    config.auth = keyPath.isEmpty() ? AuthType::Password : AuthType::PublicKey;
    config.password = password;
    config.privateKeyPath = keyPath;
    config.cols = 100;
    config.rows = 30;

    SshConnection connection;
    QString error;
    bool promptSeen = false;
    const auto callback = [&](const QString &type, const QString &fingerprint, const QString &expected) {
        out() << "host key: " << type << " " << fingerprint << "\n";
        out().flush();
        if (!expected.isEmpty() && expected != fingerprint) {
            out() << "host key mismatch\n";
            return false;
        }
        promptSeen = true;
        return true;
    };

    out() << "connecting to " << host << ":" << port << " as " << user << "...\n";
    out().flush();
    if (!connection.connect(config, expectedFingerprint, callback, &error))
        return fail(error);
    if (expectedFingerprint.isEmpty() && !promptSeen)
        return fail(QStringLiteral("unknown host key callback was not called"));
    out() << "connected, host key " << connection.hostKeyInfo().fingerprint << "\n";

    if (!connection.openShell(&error))
        return fail(error);

    if (libssh2_channel_write_ex(connection.channel(), 0, "echo smoke-ok\r", 14) < 0)
        return fail(QStringLiteral("failed to write to shell"));

    QByteArray shellOutput;
    if (!readUntil(connection.channel(), "smoke-ok", &shellOutput, 8000))
        return fail(QStringLiteral("shell did not echo smoke-ok; got: %1").arg(QString::fromUtf8(shellOutput)));
    out() << "shell echo ok (" << shellOutput.size() << " bytes)\n";

    // The application opens a dedicated SSH connection for SFTP, so exercise
    // the same topology here instead of sharing the interactive session.
    SshConnection sftpConnection;
    const auto acceptKnownHost = [](const QString &, const QString &, const QString &) { return true; };
    if (!sftpConnection.connect(config, connection.hostKeyInfo().fingerprint, acceptKnownHost, &error))
        return fail(QStringLiteral("sftp connection: %1").arg(error));

    LIBSSH2_SFTP *sftp = sftpConnection.openSftp(&error);
    if (!sftp)
        return fail(error);
    out() << "sftp subsystem ok\n";

    LIBSSH2_SFTP_HANDLE *dir = libssh2_sftp_opendir(sftp, ".");
    if (!dir)
        return fail(QStringLiteral("cannot open remote home directory"));
    QStringList remoteNames;
    char nameBuffer[4096];
    char longEntry[4096];
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    for (;;) {
        const int length = libssh2_sftp_readdir_ex(dir, nameBuffer, sizeof(nameBuffer), longEntry,
                                                   sizeof(longEntry), &attrs);
        if (length <= 0)
            break;
        remoteNames.append(QString::fromUtf8(nameBuffer, length));
    }
    libssh2_sftp_closedir(dir);
    if (!remoteNames.contains(QStringLiteral("README.md")))
        return fail(QStringLiteral("README.md missing from SFTP root: %1").arg(remoteNames.join(QStringLiteral(", "))));
    out() << "sftp list ok (" << remoteNames.size() << " entries)\n";

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return fail(QStringLiteral("cannot create temp dir"));
    const QString localDownload = tempDir.filePath(QStringLiteral("README.md"));
    LIBSSH2_SFTP_HANDLE *download = libssh2_sftp_open_ex(sftp, "README.md", 9, LIBSSH2_FXF_READ, 0,
                                                         LIBSSH2_SFTP_OPENFILE);
    if (!download)
        return fail(QStringLiteral("cannot open README.md for download"));
    QFile file(localDownload);
    if (!file.open(QIODevice::WriteOnly))
        return fail(QStringLiteral("cannot write local download"));
    char buffer[4096];
    for (;;) {
        const auto read = libssh2_sftp_read(download, buffer, sizeof(buffer));
        if (read > 0) {
            file.write(buffer, read);
        } else if (read == LIBSSH2_ERROR_EAGAIN) {
            QThread::msleep(2);
        } else {
            break;
        }
    }
    file.close();
    libssh2_sftp_close_handle(download);
    if (!QFileInfo(localDownload).size())
        return fail(QStringLiteral("downloaded README.md is empty"));
    out() << "sftp download ok\n";

    const QString localUpload = tempDir.filePath(QStringLiteral("nyan-upload.txt"));
    QFile uploadFile(localUpload);
    if (!uploadFile.open(QIODevice::WriteOnly))
        return fail(QStringLiteral("cannot create local upload"));
    uploadFile.write("nyan upload smoke\n");
    uploadFile.close();

    LIBSSH2_SFTP_HANDLE *upload = libssh2_sftp_open_ex(
        sftp, "nyan-upload.txt", 15,
        LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC, 0644, LIBSSH2_SFTP_OPENFILE);
    if (!upload)
        return fail(QStringLiteral("cannot create remote upload"));
    const QByteArray payload("nyan upload smoke\n");
    qint64 offset = 0;
    while (offset < payload.size()) {
        const auto written = libssh2_sftp_write(upload, payload.constData() + offset,
                                                static_cast<size_t>(payload.size() - offset));
        if (written > 0)
            offset += written;
        else if (written == LIBSSH2_ERROR_EAGAIN)
            QThread::msleep(2);
        else
            return fail(QStringLiteral("sftp upload failed"));
    }
    libssh2_sftp_close_handle(upload);

    const int removed = libssh2_sftp_unlink_ex(sftp, "nyan-upload.txt", 15);
    if (removed != 0)
        return fail(QStringLiteral("cannot remove uploaded file"));
    out() << "sftp upload/remove ok\n";

    libssh2_sftp_shutdown(sftp);
    sftpConnection.disconnect();
    connection.disconnect();
    out() << "ALL OK\n";
    out().flush();
    return 0;
}
