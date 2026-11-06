#pragma once

#include <QString>
#include <QStringList>

namespace nyan {

enum class SshState {
    Idle,
    Connecting,
    Authenticating,
    Connected,
    Disconnecting,
    Disconnected,
    Failed,
};

enum class AuthType {
    Password,
    PublicKey,
    KeyboardInteractive,
};

struct SshEndpoint {
    QString host;
    quint16 port = 22;
    QString username;

    QString display() const
    {
        return QStringLiteral("%1@%2:%3").arg(username, host).arg(port);
    }
};

struct SshConfig {
    QString host;
    quint16 port = 22;
    QString username;
    AuthType auth = AuthType::Password;
    QString password;
    QString privateKeyPath;
    QString passphrase;
    QString terminalType = QStringLiteral("xterm-256color");
    int cols = 100;
    int rows = 30;

    SshEndpoint endpoint() const { return {host, port, username}; }
};

struct HostKeyInfo {
    QString keyType;
    QString fingerprint;

    bool isValid() const { return !fingerprint.isEmpty(); }
};

QString sshStateLabel(SshState state);
QString hostKeyTypeLabel(const QString &rawType);

} // namespace nyan
