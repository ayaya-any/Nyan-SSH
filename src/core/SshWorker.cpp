#include "core/SshWorker.h"

#include <QElapsedTimer>
#include <QThread>

#include <libssh2.h>

namespace nyan {

SshWorker::SshWorker(QObject *parent)
    : QObject(parent)
{
}

SshWorker::~SshWorker() = default;

void SshWorker::start(const SshConfig &config, const QString &expectedFingerprint)
{
    if (m_started)
        return;
    m_started = true;
    m_stop = false;

    setState(SshState::Connecting, QStringLiteral("正在连接 %1:%2").arg(config.host).arg(config.port));

    const auto hostKeyCallback = [this](const QString &keyType, const QString &fingerprint,
                                        const QString &expected) {
        return askHostKey(keyType, fingerprint, expected);
    };

    setState(SshState::Authenticating, QStringLiteral("正在验证服务器身份"));
    QString error;
    if (!m_connection.connect(config, expectedFingerprint, hostKeyCallback, &error)) {
        const bool authIssue = !m_connection.allowedAuthMethods().isEmpty()
                               || error.contains(QStringLiteral("认证"));
        setState(SshState::Failed, error);
        if (authIssue)
            emit authenticationFailed(m_connection.allowedAuthMethods(), error);
        else
            emit errorOccurred(error);
        emit finished();
        return;
    }

    emit connected(m_connection.hostKeyInfo().keyType, m_connection.hostKeyInfo().fingerprint);

    setState(SshState::Connected, QStringLiteral("正在打开终端"));
    if (!m_connection.openShell(&error)) {
        setState(SshState::Failed, error);
        emit errorOccurred(error);
        m_connection.disconnect();
        emit finished();
        return;
    }

    setState(SshState::Connected, QStringLiteral("已连接"));

    QElapsedTimer keepAliveTimer;
    keepAliveTimer.start();
    QByteArray pending;
    QString disconnectReason = QStringLiteral("连接已关闭");

    while (!m_stop.load(std::memory_order_relaxed)) {
        LIBSSH2_CHANNEL *channel = m_connection.channel();
        if (!channel) {
            disconnectReason = QStringLiteral("SSH 通道已关闭");
            break;
        }

        if (pending.isEmpty())
            pending = takePendingWrite();

        if (!pending.isEmpty()) {
            const ssize_t written = libssh2_channel_write_ex(channel, 0, pending.constData(),
                                                             static_cast<size_t>(pending.size()));
            if (written > 0) {
                pending.remove(0, static_cast<int>(written));
            } else if (written < 0 && written != LIBSSH2_ERROR_EAGAIN) {
                disconnectReason = SshConnection::lastErrorText(m_connection.session(),
                                                                QStringLiteral("写入终端失败"));
                emit errorOccurred(disconnectReason);
                break;
            }
        }

        char buffer[32768];
        ssize_t read = 0;
        while ((read = libssh2_channel_read_ex(channel, 0, buffer, sizeof(buffer))) > 0) {
            emit dataReceived(QByteArray(buffer, static_cast<int>(read)));
        }

        if (read < 0 && read != LIBSSH2_ERROR_EAGAIN) {
            disconnectReason = SshConnection::lastErrorText(m_connection.session(),
                                                            QStringLiteral("读取终端数据失败"));
            emit errorOccurred(disconnectReason);
            break;
        }

        if (libssh2_channel_eof(channel)) {
            disconnectReason = QStringLiteral("远程主机关闭了连接");
            break;
        }

        const int cols = m_pendingCols.exchange(0);
        const int rows = m_pendingRows.exchange(0);
        if (cols > 0 && rows > 0 && m_connection.channel()) {
            libssh2_channel_request_pty_size_ex(m_connection.channel(), cols, rows, 0, 0);
        }

        if (keepAliveTimer.elapsed() > 20000) {
            keepAliveTimer.restart();
            int secondsToNext = 0;
            libssh2_keepalive_send(m_connection.session(), &secondsToNext);
        }

        QThread::msleep(8);
    }

    if (pending.isEmpty())
        pending = takePendingWrite();
    if (!pending.isEmpty() && m_connection.channel() && !m_stop.load()) {
        libssh2_channel_write_ex(m_connection.channel(), 0, pending.constData(),
                                 static_cast<size_t>(pending.size()));
    }

    if (m_stop.load(std::memory_order_relaxed))
        disconnectReason = QStringLiteral("会话已断开");

    setState(SshState::Disconnecting, QStringLiteral("正在关闭会话"));
    m_connection.disconnect();
    setState(SshState::Disconnected, disconnectReason);
    emit finished();
}

bool SshWorker::askHostKey(const QString &keyType, const QString &fingerprint, const QString &expected)
{
    {
        QMutexLocker locker(&m_promptMutex);
        m_promptAnswer.reset();
    }

    emit hostKeyPrompt(keyType, fingerprint, expected);

    QMutexLocker locker(&m_promptMutex);
    int ticks = 0;
    while (!m_promptAnswer.has_value() && !m_stop.load(std::memory_order_relaxed)) {
        m_promptCondition.wait(&m_promptMutex, 200);
        if (++ticks > 300) // 60 seconds
            break;
    }

    const bool accepted = m_promptAnswer.value_or(false);
    m_promptAnswer.reset();
    return accepted;
}

QByteArray SshWorker::takePendingWrite()
{
    QMutexLocker locker(&m_writeMutex);
    QByteArray data;
    data.swap(m_pendingWrite);
    return data;
}

void SshWorker::enqueueWrite(const QByteArray &data)
{
    if (data.isEmpty())
        return;
    QMutexLocker locker(&m_writeMutex);
    m_pendingWrite.append(data);
}

void SshWorker::requestResize(int cols, int rows)
{
    if (cols > 0)
        m_pendingCols.store(cols);
    if (rows > 0)
        m_pendingRows.store(rows);
}

void SshWorker::respondHostKey(bool accept)
{
    QMutexLocker locker(&m_promptMutex);
    m_promptAnswer = accept;
    m_promptCondition.wakeAll();
}

void SshWorker::requestStop()
{
    m_stop.store(true, std::memory_order_relaxed);
    {
        QMutexLocker locker(&m_promptMutex);
        m_promptCondition.wakeAll();
    }
}

void SshWorker::setState(SshState state, const QString &detail)
{
    emit stateChanged(state, detail);
}

} // namespace nyan
