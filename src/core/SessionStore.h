#pragma once

#include "core/SessionProfile.h"

#include <QString>
#include <QVector>

namespace nyan {

class SessionStore {
public:
    explicit SessionStore(QString path = {});

    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr) const;

    const QVector<SessionProfile> &profiles() const { return m_profiles; }
    QVector<SessionProfile> &profiles() { return m_profiles; }
    void setProfiles(const QVector<SessionProfile> &profiles) { m_profiles = profiles; }

    SessionProfile *find(const QString &id);
    const SessionProfile *find(const QString &id) const;
    int indexOf(const QString &id) const;

    void upsert(const SessionProfile &profile);
    bool remove(const QString &id);

    void touch(const QString &id);
    QVector<SessionProfile> recent(int limit) const;

    QString path() const { return m_path; }

private:
    QString m_path;
    QVector<SessionProfile> m_profiles;
};

} // namespace nyan
