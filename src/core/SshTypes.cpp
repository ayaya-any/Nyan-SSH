#include "core/SshTypes.h"

#include <QObject>

namespace nyan {

QString sshStateLabel(SshState state)
{
    switch (state) {
    case SshState::Idle:
        return QStringLiteral("空闲");
    case SshState::Connecting:
        return QStringLiteral("正在连接");
    case SshState::Authenticating:
        return QStringLiteral("正在认证");
    case SshState::Connected:
        return QStringLiteral("已连接");
    case SshState::Disconnecting:
        return QStringLiteral("正在断开");
    case SshState::Disconnected:
        return QStringLiteral("已断开");
    case SshState::Failed:
        return QStringLiteral("连接失败");
    }
    return QStringLiteral("未知");
}

QString hostKeyTypeLabel(const QString &rawType)
{
    const QString type = rawType.toUpper();
    if (type.contains(QStringLiteral("RSA")))
        return QStringLiteral("RSA");
    if (type.contains(QStringLiteral("ED25519")))
        return QStringLiteral("Ed25519");
    if (type.contains(QStringLiteral("ECDSA")))
        return QStringLiteral("ECDSA");
    if (type.contains(QStringLiteral("DSS")) || type.contains(QStringLiteral("DSA")))
        return QStringLiteral("DSA");
    return rawType.isEmpty() ? QStringLiteral("未知") : rawType;
}

} // namespace nyan
