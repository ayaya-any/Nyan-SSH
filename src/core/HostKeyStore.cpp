#include "core/HostKeyStore.h"

#include "core/AppPaths.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace nyan {

HostKeyStore::HostKeyStore(QString path)
    : m_path(path.isEmpty() ? AppPaths::knownHostsFile() : std::move(path))
{
}

QString HostKeyStore::keyFor(const QString &host, quint16 port)
{
    return QStringLiteral("%1:%2").arg(host.toLower(), QString::number(port));
}

bool HostKeyStore::load(QString *error)
{
    m_records.clear();
    QFile file(m_path);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("无法读取主机密钥文件：%1").arg(file.errorString());
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("主机密钥文件格式无效：%1").arg(parseError.errorString());
        return false;
    }

    const QJsonObject hosts = document.object().value(QStringLiteral("hosts")).toObject();
    for (auto it = hosts.begin(); it != hosts.end(); ++it) {
        const QJsonObject object = it.value().toObject();
        KnownHostRecord record;
        record.algorithm = object.value(QStringLiteral("algorithm")).toString();
        record.fingerprint = object.value(QStringLiteral("fingerprint")).toString();
        record.firstSeen = QDateTime::fromString(object.value(QStringLiteral("firstSeen")).toString(), Qt::ISODate);
        record.lastSeen = QDateTime::fromString(object.value(QStringLiteral("lastSeen")).toString(), Qt::ISODate);
        if (!record.fingerprint.isEmpty())
            m_records.insert(it.key(), record);
    }
    return true;
}

bool HostKeyStore::save(QString *error) const
{
    if (!AppPaths::ensureDataDir(error))
        return false;

    QJsonObject hosts;
    for (auto it = m_records.cbegin(); it != m_records.cend(); ++it) {
        QJsonObject object;
        object.insert(QStringLiteral("algorithm"), it.value().algorithm);
        object.insert(QStringLiteral("fingerprint"), it.value().fingerprint);
        object.insert(QStringLiteral("firstSeen"), it.value().firstSeen.toUTC().toString(Qt::ISODate));
        object.insert(QStringLiteral("lastSeen"), it.value().lastSeen.toUTC().toString(Qt::ISODate));
        hosts.insert(it.key(), object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("hosts"), hosts);

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("无法写入主机密钥文件：%1").arg(file.errorString());
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("无法保存主机密钥文件：%1").arg(file.errorString());
        return false;
    }
    return true;
}

HostKeyStore::Lookup HostKeyStore::lookup(const QString &host, quint16 port, const QString &fingerprint) const
{
    Lookup result;
    const auto it = m_records.constFind(keyFor(host, port));
    if (it == m_records.cend())
        return result;
    result.known = true;
    result.record = it.value();
    result.fingerprint = it.value().fingerprint;
    result.matches = !fingerprint.isEmpty() && it.value().fingerprint == fingerprint;
    return result;
}

QString HostKeyStore::fingerprintFor(const QString &host, quint16 port) const
{
    const auto it = m_records.constFind(keyFor(host, port));
    return it == m_records.cend() ? QString() : it.value().fingerprint;
}

void HostKeyStore::trust(const QString &host, quint16 port, const QString &algorithm, const QString &fingerprint)
{
    const QString key = keyFor(host, port);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    auto it = m_records.find(key);
    if (it == m_records.end()) {
        KnownHostRecord record;
        record.algorithm = algorithm;
        record.fingerprint = fingerprint;
        record.firstSeen = now;
        record.lastSeen = now;
        m_records.insert(key, record);
        return;
    }
    it->algorithm = algorithm;
    it->fingerprint = fingerprint;
    it->lastSeen = now;
    if (!it->firstSeen.isValid())
        it->firstSeen = now;
}

bool HostKeyStore::forget(const QString &host, quint16 port)
{
    return m_records.remove(keyFor(host, port));
}

} // namespace nyan
