#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>

namespace nyan {

struct KnownHostRecord {
    QString algorithm;
    QString fingerprint;
    QDateTime firstSeen;
    QDateTime lastSeen;
};

/// Trust-on-first-use host key storage keyed by "host:port".
class HostKeyStore {
public:
    struct Lookup {
        bool known = false;
        bool matches = false;
        QString fingerprint;
        KnownHostRecord record;
    };

    explicit HostKeyStore(QString path = {});

    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr) const;

    QString path() const { return m_path; }

    Lookup lookup(const QString &host, quint16 port, const QString &fingerprint) const;
    QString fingerprintFor(const QString &host, quint16 port) const;

    void trust(const QString &host, quint16 port, const QString &algorithm, const QString &fingerprint);
    bool forget(const QString &host, quint16 port);
    int count() const { return m_records.size(); }

    static QString keyFor(const QString &host, quint16 port);

private:
    QString m_path;
    QHash<QString, KnownHostRecord> m_records;
};

} // namespace nyan
