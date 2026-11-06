#pragma once

#include "core/SshConnection.h"

#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QWaitCondition>

#include <atomic>
#include <optional>

Q_DECLARE_METATYPE(nyan::SshState)

namespace nyan {

/// Owns one interactive SSH shell.  The object is moved to a dedicated thread;
/// start() blocks that thread for the lifetime of the connection while
/// enqueueWrite()/requestResize() are safe to call from the UI thread.
class SshWorker : public QObject {
    Q_OBJECT
public:
    explicit SshWorker(QObject *parent = nullptr);
    ~SshWorker() override;

    void start(const SshConfig &config, const QString &expectedFingerprint);

    void enqueueWrite(const QByteArray &data);
    void requestResize(int cols, int rows);
    void respondHostKey(bool accept);
    void requestStop();

    bool isStopping() const { return m_stop.load(std::memory_order_relaxed); }

signals:
    void stateChanged(nyan::SshState state, const QString &detail);
    void hostKeyPrompt(const QString &keyType, const QString &fingerprint, const QString &expectedFingerprint);
    void connected(const QString &keyType, const QString &fingerprint);
    void dataReceived(const QByteArray &data);
    void authenticationFailed(const QStringList &methods, const QString &message);
    void errorOccurred(const QString &message);
    void finished();

private:
    bool askHostKey(const QString &keyType, const QString &fingerprint, const QString &expected);
    QByteArray takePendingWrite();
    void setState(SshState state, const QString &detail);

    SshConnection m_connection;
    std::atomic<bool> m_stop{false};
    QMutex m_writeMutex;
    QByteArray m_pendingWrite;
    std::atomic<int> m_pendingCols{0};
    std::atomic<int> m_pendingRows{0};

    QMutex m_promptMutex;
    QWaitCondition m_promptCondition;
    std::optional<bool> m_promptAnswer;
    bool m_started = false;
};

} // namespace nyan
