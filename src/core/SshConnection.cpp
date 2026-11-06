#include "core/SshConnection.h"

#include <QCryptographicHash>
#include <QFileInfo>

#include <libssh2_sftp.h>

#include <cstring>
#include <mutex>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace nyan {
namespace {

#ifdef Q_OS_WIN
constexpr auto kInvalidSocket = INVALID_SOCKET;

void closeNativeSocket(SOCKET socket)
{
    ::closesocket(socket);
}

QString wsaErrorText(int code)
{
    switch (code) {
    case WSAECONNREFUSED:
        return QStringLiteral("连接被拒绝（目标端口未开放）");
    case WSAETIMEDOUT:
        return QStringLiteral("连接超时");
    case WSAHOST_NOT_FOUND:
    case WSATRY_AGAIN:
    case WSANO_RECOVERY:
        return QStringLiteral("无法解析主机名");
    case WSAENETUNREACH:
        return QStringLiteral("网络不可达");
    case WSAEHOSTUNREACH:
        return QStringLiteral("主机不可达");
    case WSAEACCES:
        return QStringLiteral("连接被系统策略阻止");
    default:
        return QStringLiteral("网络错误（WSA %1）").arg(code);
    }
}
#else
constexpr auto kInvalidSocket = -1;

void closeNativeSocket(int socket)
{
    ::close(socket);
}
#endif

QByteArray toSshString(const QString &value)
{
    return value.toUtf8();
}

} // namespace

SshConnection::SshConnection() = default;

SshConnection::~SshConnection()
{
    disconnect();
}

void SshConnection::globalInit()
{
    static std::once_flag once;
    std::call_once(once, [] {
#ifdef Q_OS_WIN
        WSADATA data{};
        WSAStartup(MAKEWORD(2, 2), &data);
#endif
        libssh2_init(0);
    });
}

QString SshConnection::computeFingerprint(const QByteArray &keyBlob)
{
    const QByteArray digest = QCryptographicHash::hash(keyBlob, QCryptographicHash::Sha256);
    return QStringLiteral("SHA256:") + QString::fromLatin1(digest.toBase64(QByteArray::Base64Encoding
                                                                         | QByteArray::OmitTrailingEquals));
}

QString SshConnection::hostKeyTypeName(int type)
{
    switch (type) {
    case LIBSSH2_HOSTKEY_TYPE_RSA:
        return QStringLiteral("ssh-rsa");
    case LIBSSH2_HOSTKEY_TYPE_DSS:
        return QStringLiteral("ssh-dss");
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_256:
        return QStringLiteral("ecdsa-sha2-nistp256");
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_384:
        return QStringLiteral("ecdsa-sha2-nistp384");
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_521:
        return QStringLiteral("ecdsa-sha2-nistp521");
    case LIBSSH2_HOSTKEY_TYPE_ED25519:
        return QStringLiteral("ssh-ed25519");
    default:
        return QStringLiteral("未知密钥");
    }
}

QString SshConnection::lastErrorText(LIBSSH2_SESSION *session, const QString &context)
{
    if (!session)
        return context;
    char *message = nullptr;
    int length = 0;
    const int code = libssh2_session_last_error(session, &message, &length, 0);
    const QString detail = message && length > 0 ? QString::fromUtf8(message, length) : QStringLiteral("未知错误");
    return QStringLiteral("%1：%2（libssh2 %3）").arg(context, detail, QString::number(code));
}

bool SshConnection::createSocket(const QString &host, quint16 port, qintptr *socketHandle, QString *error)
{
    const QByteArray hostUtf8 = host.toUtf8();
    const QByteArray portText = QByteArray::number(port);

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo *addresses = nullptr;
    const int resolved = ::getaddrinfo(hostUtf8.constData(), portText.constData(), &hints, &addresses);
    if (resolved != 0 || addresses == nullptr) {
        if (error)
            *error = QStringLiteral("无法解析主机 %1").arg(host);
        return false;
    }

    QString lastError = QStringLiteral("无法连接到 %1:%2").arg(host).arg(port);
    for (addrinfo *address = addresses; address; address = address->ai_next) {
#ifdef Q_OS_WIN
        SOCKET native = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (native == kInvalidSocket)
            continue;

        u_long nonBlocking = 1;
        ::ioctlsocket(native, FIONBIO, &nonBlocking);

        const int rc = ::connect(native, address->ai_addr, static_cast<int>(address->ai_addrlen));
        if (rc == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
            const int code = WSAGetLastError();
            lastError = wsaErrorText(code);
            closeNativeSocket(native);
            continue;
        }

        fd_set writeSet;
        fd_set errorSet;
        FD_ZERO(&writeSet);
        FD_ZERO(&errorSet);
        FD_SET(native, &writeSet);
        FD_SET(native, &errorSet);
        timeval timeout{};
        timeout.tv_sec = 15;
        timeout.tv_usec = 0;
        const int selected = ::select(0, nullptr, &writeSet, &errorSet, &timeout);
        if (selected <= 0) {
            lastError = selected == 0 ? QStringLiteral("连接超时") : wsaErrorText(WSAGetLastError());
            closeNativeSocket(native);
            continue;
        }

        int socketError = 0;
        int optionLength = sizeof(socketError);
        ::getsockopt(native, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&socketError), &optionLength);
        if (socketError != 0) {
            lastError = wsaErrorText(socketError);
            closeNativeSocket(native);
            continue;
        }

        nonBlocking = 0;
        ::ioctlsocket(native, FIONBIO, &nonBlocking);
        int noDelay = 1;
        ::setsockopt(native, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&noDelay), sizeof(noDelay));

        *socketHandle = static_cast<qintptr>(native);
        ::freeaddrinfo(addresses);
        return true;
#else
        const int native = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (native == kInvalidSocket)
            continue;
        if (::connect(native, address->ai_addr, address->ai_addrlen) == 0) {
            int noDelay = 1;
            ::setsockopt(native, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));
            *socketHandle = native;
            ::freeaddrinfo(addresses);
            return true;
        }
        lastError = QString::fromLocal8Bit(std::strerror(errno));
        closeNativeSocket(native);
#endif
    }

    ::freeaddrinfo(addresses);
    if (error)
        *error = lastError;
    return false;
}

void SshConnection::cleanupSocket()
{
    if (m_socket >= 0) {
        closeNativeSocket(static_cast<decltype(kInvalidSocket)>(m_socketNative));
        m_socket = -1;
        m_socketNative = static_cast<libssh2_socket_t>(~0ull);
    }
}

bool SshConnection::connect(const SshConfig &config, const QString &expectedFingerprint,
                            const HostKeyCallback &hostKeyCallback, QString *error)
{
    globalInit();
    disconnect();

    m_config = config;
    m_lastErrorCode = 0;
    if (config.host.trimmed().isEmpty() || config.username.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("主机与用户名不能为空");
        return false;
    }

    qintptr socket = -1;
    if (!createSocket(config.host, config.port, &socket, error))
        return false;
    m_socket = socket;
    m_socketNative = static_cast<libssh2_socket_t>(socket);

    m_session = libssh2_session_init();
    if (!m_session) {
        if (error)
            *error = QStringLiteral("无法初始化 SSH 会话");
        disconnect();
        return false;
    }
    libssh2_session_set_timeout(m_session, 20000);
    libssh2_session_set_blocking(m_session, 1);

    int rc = libssh2_session_handshake(m_session, m_socketNative);
    if (rc != 0) {
        m_lastErrorCode = rc;
        if (error)
            *error = lastErrorText(m_session, QStringLiteral("SSH 握手失败"));
        disconnect();
        return false;
    }

    size_t keyLength = 0;
    int keyType = 0;
    const char *key = libssh2_session_hostkey(m_session, &keyLength, &keyType);
    if (!key || keyLength == 0) {
        if (error)
            *error = QStringLiteral("服务器未提供主机密钥");
        disconnect();
        return false;
    }

    const QByteArray keyBlob(key, static_cast<int>(keyLength));
    m_hostKey.keyType = hostKeyTypeName(keyType);
    m_hostKey.fingerprint = computeFingerprint(keyBlob);

    if (expectedFingerprint.isEmpty()) {
        if (!hostKeyCallback || !hostKeyCallback(m_hostKey.keyType, m_hostKey.fingerprint, QString())) {
            if (error)
                *error = QStringLiteral("已取消连接：主机密钥未被信任");
            disconnect();
            return false;
        }
    } else if (expectedFingerprint != m_hostKey.fingerprint) {
        if (!hostKeyCallback
            || !hostKeyCallback(m_hostKey.keyType, m_hostKey.fingerprint, expectedFingerprint)) {
            if (error)
                *error = QStringLiteral("主机密钥已改变，连接已被阻止");
            disconnect();
            return false;
        }
    }

    if (!authenticate(config, error)) {
        disconnect();
        return false;
    }

    libssh2_keepalive_config(m_session, 1, 20);
    return true;
}

bool SshConnection::authenticate(const SshConfig &config, QString *error)
{
    const QByteArray user = toSshString(config.username);
    int rc = 0;

    switch (config.auth) {
    case AuthType::Password:
    case AuthType::KeyboardInteractive: {
        const QByteArray password = toSshString(config.password);
        rc = libssh2_userauth_password_ex(m_session, user.constData(), static_cast<unsigned int>(user.size()),
                                          password.constData(), static_cast<unsigned int>(password.size()), nullptr);
        break;
    }
    case AuthType::PublicKey: {
        const QString path = config.privateKeyPath;
        if (path.isEmpty() || !QFileInfo::exists(path)) {
            if (error)
                *error = QStringLiteral("私钥文件不存在：%1").arg(path);
            m_lastErrorCode = LIBSSH2_ERROR_FILE;
            return false;
        }
        const QByteArray keyPath = path.toUtf8();
        const QByteArray passphrase = config.passphrase.toUtf8();
        rc = libssh2_userauth_publickey_fromfile_ex(m_session, user.constData(),
                                                    static_cast<unsigned int>(user.size()), nullptr,
                                                    keyPath.constData(),
                                                    passphrase.isEmpty() ? nullptr : passphrase.constData());
        break;
    }
    }

    if (rc == 0) {
        m_lastErrorCode = 0;
        return true;
    }

    m_lastErrorCode = rc;
    char *methods = libssh2_userauth_list(m_session, user.constData(), static_cast<unsigned int>(user.size()));
    if (methods) {
        const QStringList parts = QString::fromLatin1(methods).split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString &part : parts)
            m_allowedAuthMethods.append(part.trimmed());
    }

    if (error) {
        if (rc == LIBSSH2_ERROR_AUTHENTICATION_FAILED) {
            *error = QStringLiteral("认证失败，请检查用户名与凭据");
        } else if (rc == LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED) {
            *error = QStringLiteral("私钥认证失败，服务器拒绝了该密钥");
        } else if (rc == LIBSSH2_ERROR_FILE) {
            *error = QStringLiteral("无法读取私钥文件（可能格式不受支持或口令错误）");
        } else {
            *error = lastErrorText(m_session, QStringLiteral("认证失败"));
        }
        if (!m_allowedAuthMethods.isEmpty())
            *error += QStringLiteral("；服务器允许：%1").arg(m_allowedAuthMethods.join(QStringLiteral(", ")));
    }
    return false;
}

bool SshConnection::openShell(QString *error)
{
    if (!m_session) {
        if (error)
            *error = QStringLiteral("SSH 会话尚未建立");
        return false;
    }
    if (m_channel)
        return true;

    m_channel = libssh2_channel_open_session(m_session);
    if (!m_channel) {
        if (error)
            *error = lastErrorText(m_session, QStringLiteral("无法打开 SSH 通道"));
        return false;
    }

    const QByteArray term = m_config.terminalType.toUtf8();
    const int cols = qBound(2, m_config.cols, 1000);
    const int rows = qBound(2, m_config.rows, 500);
    int rc = libssh2_channel_request_pty_ex(m_channel, term.constData(), static_cast<unsigned int>(term.size()),
                                            nullptr, 0, cols, rows, 0, 0);
    if (rc != 0) {
        if (error)
            *error = lastErrorText(m_session, QStringLiteral("无法申请伪终端"));
        libssh2_channel_free(m_channel);
        m_channel = nullptr;
        return false;
    }

    rc = libssh2_channel_shell(m_channel);
    if (rc != 0) {
        if (error)
            *error = lastErrorText(m_session, QStringLiteral("无法启动远程 Shell"));
        libssh2_channel_free(m_channel);
        m_channel = nullptr;
        return false;
    }

    libssh2_channel_set_blocking(m_channel, 0);
    return true;
}

LIBSSH2_SFTP *SshConnection::openSftp(QString *error)
{
    if (!m_session) {
        if (error)
            *error = QStringLiteral("SSH 会话尚未建立");
        return nullptr;
    }
    LIBSSH2_SFTP *sftp = libssh2_sftp_init(m_session);
    if (!sftp && error)
        *error = lastErrorText(m_session, QStringLiteral("无法启动 SFTP 子系统"));
    return sftp;
}

void SshConnection::disconnect()
{
    if (m_channel) {
        libssh2_channel_free(m_channel);
        m_channel = nullptr;
    }
    if (m_session) {
        libssh2_session_disconnect_ex(m_session, SSH_DISCONNECT_BY_APPLICATION, "bye", "");
        libssh2_session_free(m_session);
        m_session = nullptr;
    }
    cleanupSocket();
}

} // namespace nyan
