#pragma once

#include "core/SshTypes.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include <functional>

namespace nyan {

/// Thin, blocking libssh2 wrapper shared by the terminal and SFTP workers.
/// The wrapper owns the TCP socket and all libssh2 objects it creates.
class SshConnection {
public:
    using HostKeyCallback = std::function<bool(const QString &keyType, const QString &fingerprint,
                                               const QString &expectedFingerprint)>;

    SshConnection();
    ~SshConnection();

    SshConnection(const SshConnection &) = delete;
    SshConnection &operator=(const SshConnection &) = delete;

    static void globalInit();
    static QString computeFingerprint(const QByteArray &keyBlob);
    static QString hostKeyTypeName(int type);

    bool connect(const SshConfig &config, const QString &expectedFingerprint, const HostKeyCallback &hostKeyCallback,
                 QString *error);
    bool openShell(QString *error);
    LIBSSH2_SFTP *openSftp(QString *error);

    void disconnect();

    bool isConnected() const { return m_session != nullptr; }
    LIBSSH2_SESSION *session() const { return m_session; }
    LIBSSH2_CHANNEL *channel() const { return m_channel; }

    const HostKeyInfo &hostKeyInfo() const { return m_hostKey; }
    QStringList allowedAuthMethods() const { return m_allowedAuthMethods; }
    const SshConfig &config() const { return m_config; }

    static QString lastErrorText(LIBSSH2_SESSION *session, const QString &context);
    int lastErrorCode() const { return m_lastErrorCode; }

private:
    bool authenticate(const SshConfig &config, QString *error);
    void cleanupSocket();
    static bool createSocket(const QString &host, quint16 port, qintptr *socketHandle, QString *error);

    libssh2_socket_t m_socketNative = static_cast<libssh2_socket_t>(~0ull);
    qintptr m_socket = -1;
    LIBSSH2_SESSION *m_session = nullptr;
    LIBSSH2_CHANNEL *m_channel = nullptr;
    HostKeyInfo m_hostKey;
    QStringList m_allowedAuthMethods;
    SshConfig m_config;
    int m_lastErrorCode = 0;
};

} // namespace nyan
