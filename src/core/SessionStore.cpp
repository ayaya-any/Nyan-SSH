#include "core/SessionStore.h"

#include "core/AppPaths.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>

namespace nyan {

SessionStore::SessionStore(QString path)
    : m_path(path.isEmpty() ? AppPaths::sessionsFile() : std::move(path))
{
}

bool SessionStore::load(QString *error)
{
    m_profiles.clear();
    QFile file(m_path);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("无法读取会话文件：%1").arg(file.errorString());
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error)
            *error = QStringLiteral("会话文件格式无效：%1").arg(parseError.errorString());
        return false;
    }
    if (!document.isObject()) {
        if (error)
            *error = QStringLiteral("会话文件格式无效");
        return false;
    }

    const QJsonArray sessions = document.object().value(QStringLiteral("sessions")).toArray();
    m_profiles.reserve(sessions.size());
    for (const QJsonValue &value : sessions) {
        if (!value.isObject())
            continue;
        SessionProfile profile = SessionProfile::fromJson(value.toObject());
        if (profile.host.isEmpty() && profile.name.isEmpty())
            continue;
        m_profiles.append(profile);
    }
    return true;
}

bool SessionStore::save(QString *error) const
{
    if (!AppPaths::ensureDataDir(error))
        return false;

    QJsonArray sessions;
    for (const SessionProfile &profile : m_profiles)
        sessions.append(profile.toJson());

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("sessions"), sessions);

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("无法写入会话文件：%1").arg(file.errorString());
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("无法保存会话文件：%1").arg(file.errorString());
        return false;
    }
    return true;
}

SessionProfile *SessionStore::find(const QString &id)
{
    const int index = indexOf(id);
    return index >= 0 ? &m_profiles[index] : nullptr;
}

const SessionProfile *SessionStore::find(const QString &id) const
{
    const int index = indexOf(id);
    return index >= 0 ? &m_profiles.at(index) : nullptr;
}

int SessionStore::indexOf(const QString &id) const
{
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles.at(i).id == id)
            return i;
    }
    return -1;
}

void SessionStore::upsert(const SessionProfile &profile)
{
    const int index = indexOf(profile.id);
    if (index >= 0)
        m_profiles[index] = profile;
    else
        m_profiles.append(profile);
}

bool SessionStore::remove(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0)
        return false;
    m_profiles.removeAt(index);
    return true;
}

void SessionStore::touch(const QString &id)
{
    SessionProfile *profile = find(id);
    if (!profile)
        return;
    profile->lastUsedAt = QDateTime::currentDateTimeUtc();
}

QVector<SessionProfile> SessionStore::recent(int limit) const
{
    QVector<SessionProfile> copy = m_profiles;
    std::sort(copy.begin(), copy.end(), [](const SessionProfile &a, const SessionProfile &b) {
        const QDateTime at = a.lastUsedAt.isValid() ? a.lastUsedAt : a.createdAt;
        const QDateTime bt = b.lastUsedAt.isValid() ? b.lastUsedAt : b.createdAt;
        if (at == bt)
            return a.name.toLower() < b.name.toLower();
        return at > bt;
    });
    if (limit > 0 && copy.size() > limit)
        copy.resize(limit);
    return copy;
}

} // namespace nyan
