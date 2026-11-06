#pragma once

#include "core/SshTypes.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>

namespace nyan {

/// A persisted connection profile.  Passwords and key passphrases are kept in
/// memory and, only when the user opts in, encrypted with DPAPI on save.
class SessionProfile {
public:
    SessionProfile();

    QString id;
    QString name;
    QString host;
    quint16 port = 22;
    QString username;
    AuthType auth = AuthType::Password;
    QString privateKeyPath;
    QString note;
    QString color;
    bool rememberPassword = false;
    bool autoLogin = false;
    QDateTime createdAt;
    QDateTime lastUsedAt;

    QString displayName() const;
    QString subtitle() const;
    SshEndpoint endpoint() const;

    QString password() const;
    void setPassword(const QString &plain, bool remember);
    bool hasPassword() const;
    QString passphrase() const;
    void setPassphrase(const QString &plain, bool remember);
    bool hasPassphrase() const;

    bool isUsableForAutoLogin() const;
    bool validate(QString *error) const;

    QJsonObject toJson() const;
    static SessionProfile fromJson(const QJsonObject &object);

    static QStringList accentPalette();
    static QString defaultColorFor(const QString &seed);

private:
    QString m_passwordBlob;
    mutable QString m_passwordCache;
    mutable bool m_passwordDecoded = false;

    QString m_passphraseBlob;
    mutable QString m_passphraseCache;
    mutable bool m_passphraseDecoded = false;

    void clearSecretCache();
};

} // namespace nyan
