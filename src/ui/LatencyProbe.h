#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

namespace nyan::ui {

/// Lightweight asynchronous TCP round-trip probe used for the per-session
/// latency indicator.  It measures connect() latency to the SSH endpoint, so
/// an unavailable server or a blocked port simply shows "—".
class LatencyProbe : public QObject {
    Q_OBJECT
public:
    explicit LatencyProbe(QObject *parent = nullptr);

    void setEndpoint(const QString &host, quint16 port);
    void start();
    void stop();

signals:
    void latencyChanged(int milliseconds);

private:
    void probe();
    void finish(int milliseconds);
    void onConnected();
    void onError();
    void onTimeout();

    QTimer m_interval;
    QTimer m_timeout;
    QTcpSocket m_socket;
    QElapsedTimer m_elapsed;
    QString m_host;
    quint16 m_port = 0;
    bool m_active = false;
    bool m_probing = false;
};

} // namespace nyan::ui
