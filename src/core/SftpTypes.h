#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>

namespace nyan {

struct SftpEntry {
    QString name;
    QString path;
    bool isDirectory = false;
    bool isSymlink = false;
    qint64 size = 0;
    QDateTime modified;
    QDateTime accessed;
    quint32 mode = 0;
    quint32 uid = 0;
    quint32 gid = 0;

    QString permissionText() const;
    QString sizeText() const;
    QString iconName() const;
};

QString joinRemotePath(const QString &base, const QString &name);
QString parentRemotePath(const QString &path);
QString normalizeRemotePath(const QString &path);

} // namespace nyan

Q_DECLARE_METATYPE(nyan::SftpEntry)
Q_DECLARE_METATYPE(QVector<nyan::SftpEntry>)
