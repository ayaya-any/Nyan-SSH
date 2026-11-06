#pragma once

#include "core/SftpTypes.h"
#include "core/SshConnection.h"

#include <QObject>

#include <atomic>

namespace nyan {

/// Runs a second SSH connection dedicated to SFTP so directory browsing and
/// transfers never block the interactive terminal.  The object lives in its
/// own thread; all public slots are invoked through queued connections, except
/// cancelTransfer() which is intentionally thread-safe and called directly.
class SftpWorker : public QObject {
    Q_OBJECT
public:
    explicit SftpWorker(QObject *parent = nullptr);
    ~SftpWorker() override;

    void cancelTransfer(int id);

public slots:
    void open(const nyan::SshConfig &config, const QString &expectedFingerprint);
    void close();
    void listDirectory(const QString &path);
    void makeDirectory(const QString &path);
    void renameEntry(const QString &oldPath, const QString &newPath);
    void removeEntry(const QString &path, bool recursive);
    void downloadFile(int id, const QString &remotePath, const QString &localPath);
    void uploadFile(int id, const QString &localPath, const QString &remotePath);

signals:
    void opened(const QString &homePath, const QString &hostKeyType);
    void openFailed(const QString &message);
    void closed();
    void directoryListed(const QString &path, const QVector<nyan::SftpEntry> &entries);
    void directoryListFailed(const QString &path, const QString &message);
    void operationFinished(const QString &operation, const QString &path, bool ok, const QString &message);
    void transferProgress(int id, qint64 done, qint64 total);
    void transferFinished(int id, bool ok, const QString &message);
    void errorOccurred(const QString &message);

private:
    bool requireSftp(QString *error) const;
    bool removeRecursive(const QString &path, QString *error);
    QString sftpErrorText(const QString &context, int code = -1) const;
    bool transferCancelled(int id) const;

    SshConnection m_connection;
    LIBSSH2_SFTP *m_sftp = nullptr;
    QString m_homePath;
    std::atomic<int> m_cancelRequestedId{-1};
    std::atomic<int> m_activeTransferId{-1};
    bool m_closing = false;
};

} // namespace nyan
