#include "core/SessionProfile.h"

#include "core/SecretStore.h"

#include <QCryptographicHash>
#include <QJsonValue>
#include <QUuid>

namespace nyan {
namespace {

QString authToString(AuthType type)
{
    switch (type) {
    case AuthType::Password:
        return QStringLiteral("password");
    case AuthType::PublicKey:
        return QStringLiteral("key");
    case AuthType::KeyboardInteractive:
        return QStringLiteral("keyboard-interactive");
    }
    return QStringLiteral("password");
}

AuthType authFromString(const QString &value)
{
    if (value == QStringLiteral("key") || value == QStringLiteral("publickey"))
        return AuthType::PublicKey;
    if (value == QStringLiteral("keyboard-interactive"))
        return AuthType::KeyboardInteractive;
    return AuthType::Password;
}

QDateTime parseTime(const QJsonValue &value)
{
    const QString text = value.toString();
    if (text.isEmpty())
        return {};
    QDateTime dt = QDateTime::fromString(text, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(text, Qt::ISODateWithMs);
    return dt;
}

} // namespace

SessionProfile::SessionProfile()
    : id(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , createdAt(QDateTime::currentDateTimeUtc())
{
}

QString SessionProfile::displayName() const
{
    if (!name.trimmed().isEmpty())
        return name.trimmed();
    if (!username.isEmpty() && !host.isEmpty())
        return QStringLiteral("%1@%2").arg(username, host);
    return QStringLiteral("新建连接");
}

QString SessionProfile::subtitle() const
{
    if (host.isEmpty())
        return QStringLiteral("尚未配置主机");
    return QStringLiteral("%1@%2:%3").arg(username, host).arg(port);
}

SshEndpoint SessionProfile::endpoint() const
{
    return {host, port, username};
}

QString SessionProfile::password() const
{
    if (!m_passwordDecoded) {
        m_passwordCache = SecretStore::unprotect(m_passwordBlob);
        m_passwordDecoded = true;
    }
    return m_passwordCache;
}

void SessionProfile::setPassword(const QString &plain, bool remember)
{
    m_passwordCache = plain;
    m_passwordDecoded = true;
    rememberPassword = remember;
    m_passwordBlob = remember && !plain.isEmpty() ? SecretStore::protect(plain) : QString();
}

bool SessionProfile::hasPassword() const
{
    return !m_passwordBlob.isEmpty() || !m_passwordCache.isEmpty();
}

QString SessionProfile::passphrase() const
{
    if (!m_passphraseDecoded) {
        m_passphraseCache = SecretStore::unprotect(m_passphraseBlob);
        m_passphraseDecoded = true;
    }
    return m_passphraseCache;
}

void SessionProfile::setPassphrase(const QString &plain, bool remember)
{
    m_passphraseCache = plain;
    m_passphraseDecoded = true;
    m_passphraseBlob = remember && !plain.isEmpty() ? SecretStore::protect(plain) : QString();
}

bool SessionProfile::hasPassphrase() const
{
    return !m_passphraseBlob.isEmpty() || !m_passphraseCache.isEmpty();
}

bool SessionProfile::isUsableForAutoLogin() const
{
    if (!autoLogin || host.trimmed().isEmpty() || username.trimmed().isEmpty())
        return false;
    if (auth == AuthType::PublicKey)
        return !privateKeyPath.trimmed().isEmpty();
    return rememberPassword && hasPassword();
}

bool SessionProfile::validate(QString *error) const
{
    if (host.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("请填写主机地址");
        return false;
    }
    if (port == 0) {
        if (error)
            *error = QStringLiteral("端口必须在 1-65535 之间");
        return false;
    }
    if (username.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("请填写用户名");
        return false;
    }
    if (auth == AuthType::PublicKey && privateKeyPath.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("请选择私钥文件");
        return false;
    }
    return true;
}

QJsonObject SessionProfile::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("host"), host);
    object.insert(QStringLiteral("port"), static_cast<int>(port));
    object.insert(QStringLiteral("username"), username);
    object.insert(QStringLiteral("auth"), authToString(auth));
    object.insert(QStringLiteral("keyPath"), privateKeyPath);
    object.insert(QStringLiteral("note"), note);
    object.insert(QStringLiteral("color"), color);
    object.insert(QStringLiteral("rememberPassword"), rememberPassword);
    object.insert(QStringLiteral("autoLogin"), autoLogin);
    object.insert(QStringLiteral("password"), rememberPassword ? m_passwordBlob : QString());
    object.insert(QStringLiteral("passphrase"), rememberPassword ? m_passphraseBlob : QString());
    object.insert(QStringLiteral("createdAt"), createdAt.toUTC().toString(Qt::ISODate));
    object.insert(QStringLiteral("lastUsedAt"), lastUsedAt.toUTC().toString(Qt::ISODate));
    return object;
}

SessionProfile SessionProfile::fromJson(const QJsonObject &object)
{
    SessionProfile profile;
    profile.id = object.value(QStringLiteral("id")).toString(profile.id);
    if (profile.id.isEmpty())
        profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    profile.name = object.value(QStringLiteral("name")).toString();
    profile.host = object.value(QStringLiteral("host")).toString();
    profile.port = static_cast<quint16>(object.value(QStringLiteral("port")).toInt(22));
    profile.username = object.value(QStringLiteral("username")).toString();
    profile.auth = authFromString(object.value(QStringLiteral("auth")).toString());
    profile.privateKeyPath = object.value(QStringLiteral("keyPath")).toString();
    profile.note = object.value(QStringLiteral("note")).toString();
    profile.color = object.value(QStringLiteral("color")).toString();
    profile.rememberPassword = object.value(QStringLiteral("rememberPassword")).toBool(false);
    profile.autoLogin = object.value(QStringLiteral("autoLogin")).toBool(false);
    profile.m_passwordBlob = object.value(QStringLiteral("password")).toString();
    profile.m_passphraseBlob = object.value(QStringLiteral("passphrase")).toString();
    profile.createdAt = parseTime(object.value(QStringLiteral("createdAt")));
    profile.lastUsedAt = parseTime(object.value(QStringLiteral("lastUsedAt")));
    if (!profile.createdAt.isValid())
        profile.createdAt = QDateTime::currentDateTimeUtc();
    if (profile.color.isEmpty())
        profile.color = defaultColorFor(profile.id);
    return profile;
}

QStringList SessionProfile::accentPalette()
{
    return {
        QStringLiteral("#4C8DFF"),
        QStringLiteral("#8B7CF6"),
        QStringLiteral("#2DD4BF"),
        QStringLiteral("#F59E0B"),
        QStringLiteral("#F472B6"),
        QStringLiteral("#34D399"),
        QStringLiteral("#60A5FA"),
        QStringLiteral("#FB7185"),
    };
}

QString SessionProfile::defaultColorFor(const QString &seed)
{
    const QByteArray hash = QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Sha1);
    const int index = hash.isEmpty() ? 0 : static_cast<unsigned char>(hash.at(0)) % accentPalette().size();
    return accentPalette().at(index);
}

void SessionProfile::clearSecretCache()
{
    m_passwordCache.clear();
    m_passphraseCache.clear();
    m_passwordDecoded = false;
    m_passphraseDecoded = false;
}

} // namespace nyan
