#include "core/SftpWorker.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QThread>
#include <QTimeZone>

#include <libssh2_sftp.h>

#include <algorithm>

namespace nyan {
namespace {

QDateTime fromEpoch(unsigned long seconds)
{
    if (seconds == 0)
        return {};
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(seconds), QTimeZone::UTC).toLocalTime();
}

} // namespace

SftpWorker::SftpWorker(QObject *parent)
    : QObject(parent)
{
}

SftpWorker::~SftpWorker()
{
    if (m_sftp) {
        libssh2_sftp_shutdown(m_sftp);
        m_sftp = nullptr;
    }
    m_connection.disconnect();
}

bool SftpWorker::requireSftp(QString *error) const
{
    if (m_sftp && m_connection.isConnected())
        return true;
    if (error)
        *error = QStringLiteral("SFTP 连接尚未建立");
    return false;
}

QString SftpWorker::sftpErrorText(const QString &context, int code) const
{
    int sftpCode = code;
    if (sftpCode < 0 && m_sftp)
        sftpCode = static_cast<int>(libssh2_sftp_last_error(m_sftp));

    QString detail;
    switch (sftpCode) {
    case LIBSSH2_FX_NO_SUCH_FILE:
        detail = QStringLiteral("文件或目录不存在");
        break;
    case LIBSSH2_FX_PERMISSION_DENIED:
        detail = QStringLiteral("权限不足");
        break;
    case LIBSSH2_FX_NO_SUCH_PATH:
        detail = QStringLiteral("路径不存在");
        break;
    case LIBSSH2_FX_FILE_ALREADY_EXISTS:
        detail = QStringLiteral("文件已存在");
        break;
    case LIBSSH2_FX_WRITE_PROTECT:
        detail = QStringLiteral("目标为只读");
        break;
    case LIBSSH2_FX_DIR_NOT_EMPTY:
        detail = QStringLiteral("目录不为空");
        break;
    case LIBSSH2_FX_NOT_A_DIRECTORY:
        detail = QStringLiteral("目标不是目录");
        break;
    case LIBSSH2_FX_NO_CONNECTION:
    case LIBSSH2_FX_CONNECTION_LOST:
        detail = QStringLiteral("连接已断开");
        break;
    default:
        break;
    }

    if (detail.isEmpty()) {
        if (m_connection.session())
            return SshConnection::lastErrorText(m_connection.session(), context);
        return context;
    }
    return QStringLiteral("%1：%2").arg(context, detail);
}

void SftpWorker::open(const SshConfig &config, const QString &expectedFingerprint)
{
    if (m_sftp)
        close();
    m_closing = false;
    m_homePath.clear();

    const auto rejectHostKey = [](const QString &, const QString &, const QString &) { return false; };
    QString error;
    if (!m_connection.connect(config, expectedFingerprint, rejectHostKey, &error)) {
        emit openFailed(error);
        return;
    }

    m_sftp = m_connection.openSftp(&error);
    if (!m_sftp) {
        emit openFailed(SshConnection::lastErrorText(m_connection.session(), QStringLiteral("无法启动 SFTP 子系统")));
        m_connection.disconnect();
        return;
    }

    char homeBuffer[4096] = {};
    const int homeLength = libssh2_sftp_realpath(m_sftp, ".", homeBuffer, sizeof(homeBuffer) - 1);
    m_homePath = homeLength > 0 ? QString::fromUtf8(homeBuffer, homeLength) : QStringLiteral("/");
    if (m_homePath.isEmpty())
        m_homePath = QStringLiteral("/");

    emit opened(m_homePath, m_connection.hostKeyInfo().keyType);
}

void SftpWorker::close()
{
    if (m_closing)
        return;
    m_closing = true;
    m_cancelRequestedId.store(-1);
    if (m_sftp) {
        libssh2_sftp_shutdown(m_sftp);
        m_sftp = nullptr;
    }
    m_connection.disconnect();
    emit closed();
}

void SftpWorker::listDirectory(const QString &path)
{
    QString error;
    if (!requireSftp(&error)) {
        emit directoryListFailed(path, error);
        return;
    }

    const QByteArray remote = (path.isEmpty() ? QStringLiteral(".") : path).toUtf8();
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_opendir(m_sftp, remote.constData());
    if (!handle) {
        emit directoryListFailed(path, sftpErrorText(QStringLiteral("无法打开目录")));
        return;
    }

    QVector<SftpEntry> entries;
    char fileName[4096];
    char longEntry[4096];
    LIBSSH2_SFTP_ATTRIBUTES attributes{};

    for (;;) {
        const int length = libssh2_sftp_readdir_ex(handle, fileName, sizeof(fileName), longEntry,
                                                   sizeof(longEntry), &attributes);
        if (length == 0)
            break;
        if (length < 0) {
            if (length == LIBSSH2_ERROR_EAGAIN) {
                QThread::msleep(2);
                continue;
            }
            libssh2_sftp_closedir(handle);
            emit directoryListFailed(path, sftpErrorText(QStringLiteral("读取目录失败"), length));
            return;
        }

        const QString name = QString::fromUtf8(fileName, length);
        if (name == QStringLiteral(".") || name == QStringLiteral(".."))
            continue;

        SftpEntry entry;
        entry.name = name;
        entry.path = joinRemotePath(path, name);
        entry.mode = attributes.permissions;
        entry.isDirectory = LIBSSH2_SFTP_S_ISDIR(attributes.permissions);
        entry.isSymlink = LIBSSH2_SFTP_S_ISLNK(attributes.permissions);
        if (attributes.flags & LIBSSH2_SFTP_ATTR_SIZE)
            entry.size = static_cast<qint64>(attributes.filesize);
        if (attributes.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) {
            const QDateTime modified = fromEpoch(attributes.mtime);
            entry.modified = modified;
            entry.accessed = fromEpoch(attributes.atime);
        }
        if (attributes.flags & LIBSSH2_SFTP_ATTR_UIDGID) {
            entry.uid = attributes.uid;
            entry.gid = attributes.gid;
        }
        entries.append(entry);
    }

    libssh2_sftp_closedir(handle);

    std::sort(entries.begin(), entries.end(), [](const SftpEntry &a, const SftpEntry &b) {
        if (a.isDirectory != b.isDirectory)
            return a.isDirectory;
        return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
    });

    emit directoryListed(path, entries);
}

void SftpWorker::makeDirectory(const QString &path)
{
    QString error;
    if (!requireSftp(&error)) {
        emit operationFinished(QStringLiteral("mkdir"), path, false, error);
        return;
    }
    const QByteArray remote = path.toUtf8();
    const int rc = libssh2_sftp_mkdir_ex(m_sftp, remote.constData(), static_cast<unsigned int>(remote.size()),
                                         0755);
    if (rc == 0) {
        emit operationFinished(QStringLiteral("mkdir"), path, true, QStringLiteral("已创建文件夹"));
    } else {
        emit operationFinished(QStringLiteral("mkdir"), path, false,
                               sftpErrorText(QStringLiteral("创建文件夹失败"), rc));
    }
}

void SftpWorker::renameEntry(const QString &oldPath, const QString &newPath)
{
    QString error;
    if (!requireSftp(&error)) {
        emit operationFinished(QStringLiteral("rename"), oldPath, false, error);
        return;
    }
    const QByteArray source = oldPath.toUtf8();
    const QByteArray target = newPath.toUtf8();
    const int rc = libssh2_sftp_rename_ex(m_sftp, source.constData(), static_cast<unsigned int>(source.size()),
                                          target.constData(), static_cast<unsigned int>(target.size()),
                                          LIBSSH2_SFTP_RENAME_OVERWRITE | LIBSSH2_SFTP_RENAME_ATOMIC
                                              | LIBSSH2_SFTP_RENAME_NATIVE);
    if (rc == 0) {
        emit operationFinished(QStringLiteral("rename"), newPath, true, QStringLiteral("重命名成功"));
    } else {
        emit operationFinished(QStringLiteral("rename"), oldPath, false,
                               sftpErrorText(QStringLiteral("重命名失败"), rc));
    }
}

void SftpWorker::removeEntry(const QString &path, bool recursive)
{
    QString error;
    if (!requireSftp(&error)) {
        emit operationFinished(QStringLiteral("remove"), path, false, error);
        return;
    }

    bool ok = false;
    if (recursive) {
        ok = removeRecursive(path, &error);
    } else {
        const QByteArray remote = path.toUtf8();
        const int rc = libssh2_sftp_unlink_ex(m_sftp, remote.constData(), static_cast<unsigned int>(remote.size()));
        if (rc == 0) {
            ok = true;
        } else {
            const int dirRc = libssh2_sftp_rmdir_ex(m_sftp, remote.constData(),
                                                    static_cast<unsigned int>(remote.size()));
            if (dirRc == 0)
                ok = true;
            else
                error = sftpErrorText(QStringLiteral("删除失败"), rc);
        }
    }

    emit operationFinished(QStringLiteral("remove"), path, ok,
                           ok ? QStringLiteral("已删除") : error);
}

bool SftpWorker::removeRecursive(const QString &path, QString *error)
{
    LIBSSH2_SFTP_ATTRIBUTES attributes{};
    const QByteArray remote = path.toUtf8();
    const int statResult = libssh2_sftp_stat_ex(m_sftp, remote.constData(),
                                                static_cast<unsigned int>(remote.size()), LIBSSH2_SFTP_LSTAT,
                                                &attributes);
    if (statResult != 0) {
        if (error)
            *error = sftpErrorText(QStringLiteral("无法读取目标信息"), statResult);
        return false;
    }

    if (!LIBSSH2_SFTP_S_ISDIR(attributes.permissions)) {
        const int rc = libssh2_sftp_unlink_ex(m_sftp, remote.constData(), static_cast<unsigned int>(remote.size()));
        if (rc != 0) {
            if (error)
                *error = sftpErrorText(QStringLiteral("删除文件失败"), rc);
            return false;
        }
        return true;
    }

    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_opendir(m_sftp, remote.constData());
    if (!handle) {
        if (error)
            *error = sftpErrorText(QStringLiteral("无法读取目录"));
        return false;
    }

    QStringList children;
    char fileName[4096];
    LIBSSH2_SFTP_ATTRIBUTES childAttributes{};
    for (;;) {
        const int length = libssh2_sftp_readdir_ex(handle, fileName, sizeof(fileName), nullptr, 0,
                                                   &childAttributes);
        if (length == 0)
            break;
        if (length < 0) {
            if (length == LIBSSH2_ERROR_EAGAIN) {
                QThread::msleep(2);
                continue;
            }
            libssh2_sftp_closedir(handle);
            if (error)
                *error = sftpErrorText(QStringLiteral("读取目录失败"), length);
            return false;
        }
        const QString name = QString::fromUtf8(fileName, length);
        if (name == QStringLiteral(".") || name == QStringLiteral(".."))
            continue;
        children.append(name);
    }
    libssh2_sftp_closedir(handle);

    for (const QString &child : children) {
        if (!removeRecursive(joinRemotePath(path, child), error))
            return false;
    }

    const int rc = libssh2_sftp_rmdir_ex(m_sftp, remote.constData(), static_cast<unsigned int>(remote.size()));
    if (rc != 0) {
        if (error)
            *error = sftpErrorText(QStringLiteral("删除目录失败"), rc);
        return false;
    }
    return true;
}

bool SftpWorker::transferCancelled(int id) const
{
    return m_cancelRequestedId.load(std::memory_order_relaxed) == id;
}

void SftpWorker::cancelTransfer(int id)
{
    m_cancelRequestedId.store(id);
}

void SftpWorker::downloadFile(int id, const QString &remotePath, const QString &localPath)
{
    QString error;
    if (!requireSftp(&error)) {
        emit transferFinished(id, false, error);
        return;
    }

    m_activeTransferId.store(id);
    m_cancelRequestedId.store(-1);

    const QByteArray remote = remotePath.toUtf8();
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open_ex(m_sftp, remote.constData(),
                                                       static_cast<unsigned int>(remote.size()),
                                                       LIBSSH2_FXF_READ, 0, LIBSSH2_SFTP_OPENFILE);
    if (!handle) {
        emit transferFinished(id, false, sftpErrorText(QStringLiteral("无法打开远程文件")));
        return;
    }

    qint64 total = 0;
    LIBSSH2_SFTP_ATTRIBUTES attributes{};
    if (libssh2_sftp_stat_ex(m_sftp, remote.constData(), static_cast<unsigned int>(remote.size()),
                             LIBSSH2_SFTP_STAT, &attributes)
            == 0
        && (attributes.flags & LIBSSH2_SFTP_ATTR_SIZE)) {
        total = static_cast<qint64>(attributes.filesize);
    }

    QDir().mkpath(QFileInfo(localPath).absolutePath());
    QFile localFile(localPath);
    if (!localFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        libssh2_sftp_close_handle(handle);
        emit transferFinished(id, false, QStringLiteral("无法写入本地文件：%1").arg(localFile.errorString()));
        return;
    }

    QByteArray buffer(65536, Qt::Uninitialized);
    qint64 done = 0;
    bool ok = true;
    bool cancelled = false;
    QString failure;
    QElapsedTimer progressTimer;
    progressTimer.start();
    QElapsedTimer stallTimer;
    stallTimer.start();

    for (;;) {
        if (transferCancelled(id)) {
            cancelled = true;
            break;
        }
        const auto read = libssh2_sftp_read(handle, buffer.data(), static_cast<size_t>(buffer.size()));
        if (read > 0) {
            if (localFile.write(buffer.constData(), read) != read) {
                ok = false;
                failure = QStringLiteral("写入本地文件失败：%1").arg(localFile.errorString());
                break;
            }
            done += read;
            stallTimer.restart();
            if (progressTimer.elapsed() > 60) {
                emit transferProgress(id, done, total);
                progressTimer.restart();
            }
            continue;
        }
        if (read == 0)
            break;
        if (read == LIBSSH2_ERROR_EAGAIN) {
            if (stallTimer.elapsed() > 30000) {
                ok = false;
                failure = QStringLiteral("下载超时");
                break;
            }
            QThread::msleep(5);
            continue;
        }
        ok = false;
        failure = SshConnection::lastErrorText(m_connection.session(), QStringLiteral("下载失败"));
        break;
    }

    libssh2_sftp_close_handle(handle);
    localFile.close();

    if (cancelled || !ok) {
        localFile.remove();
        m_activeTransferId.store(-1);
        emit transferFinished(id, false, cancelled ? QStringLiteral("下载已取消") : failure);
        return;
    }

    emit transferProgress(id, done, total > 0 ? total : done);
    m_activeTransferId.store(-1);
    emit transferFinished(id, true, QStringLiteral("下载完成"));
}

void SftpWorker::uploadFile(int id, const QString &localPath, const QString &remotePath)
{
    QString error;
    if (!requireSftp(&error)) {
        emit transferFinished(id, false, error);
        return;
    }

    m_activeTransferId.store(id);
    m_cancelRequestedId.store(-1);

    QFile localFile(localPath);
    if (!localFile.open(QIODevice::ReadOnly)) {
        emit transferFinished(id, false, QStringLiteral("无法读取本地文件：%1").arg(localFile.errorString()));
        return;
    }
    const qint64 total = localFile.size();

    const QByteArray remote = remotePath.toUtf8();
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open_ex(
        m_sftp, remote.constData(), static_cast<unsigned int>(remote.size()),
        LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC, 0644, LIBSSH2_SFTP_OPENFILE);
    if (!handle) {
        emit transferFinished(id, false, sftpErrorText(QStringLiteral("无法创建远程文件")));
        return;
    }

    qint64 done = 0;
    bool ok = true;
    bool cancelled = false;
    QString failure;
    QElapsedTimer progressTimer;
    progressTimer.start();
    QElapsedTimer stallTimer;
    stallTimer.start();

    while (!localFile.atEnd()) {
        if (transferCancelled(id)) {
            cancelled = true;
            break;
        }
        const QByteArray chunk = localFile.read(65536);
        if (chunk.isEmpty() && localFile.error() != QFileDevice::NoError) {
            ok = false;
            failure = QStringLiteral("读取本地文件失败：%1").arg(localFile.errorString());
            break;
        }
        qint64 offset = 0;
        while (offset < chunk.size()) {
            if (transferCancelled(id)) {
                cancelled = true;
                break;
            }
            const auto written = libssh2_sftp_write(handle, chunk.constData() + offset,
                                                    static_cast<size_t>(chunk.size() - offset));
            if (written > 0) {
                offset += written;
                done += written;
                stallTimer.restart();
                if (progressTimer.elapsed() > 60) {
                    emit transferProgress(id, done, total);
                    progressTimer.restart();
                }
                continue;
            }
            if (written == LIBSSH2_ERROR_EAGAIN) {
                if (stallTimer.elapsed() > 30000) {
                    ok = false;
                    failure = QStringLiteral("上传超时");
                    break;
                }
                QThread::msleep(5);
                continue;
            }
            ok = false;
            failure = SshConnection::lastErrorText(m_connection.session(), QStringLiteral("上传失败"));
            break;
        }
        if (!ok || cancelled)
            break;
    }

    libssh2_sftp_close_handle(handle);
    localFile.close();

    if (cancelled || !ok) {
        libssh2_sftp_unlink_ex(m_sftp, remote.constData(), static_cast<unsigned int>(remote.size()));
        m_activeTransferId.store(-1);
        emit transferFinished(id, false, cancelled ? QStringLiteral("上传已取消") : failure);
        return;
    }

    emit transferProgress(id, done, total > 0 ? total : done);
    m_activeTransferId.store(-1);
    emit transferFinished(id, true, QStringLiteral("上传完成"));
}

} // namespace nyan
