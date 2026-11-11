#include "ui/LatencyProbe.h"

#include <QNetworkProxy>

namespace nyan::ui {

LatencyProbe::LatencyProbe(QObject *parent)
    : QObject(parent)
{
    m_socket.setProxy(QNetworkProxy::NoProxy);
    m_interval.setInterval(5000);
    m_interval.setTimerType(Qt::CoarseTimer);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(2500);

    connect(&m_interval, &QTimer::timeout, this, &LatencyProbe::probe);
    connect(&m_timeout, &QTimer::timeout, this, &LatencyProbe::onTimeout);
    connect(&m_socket, &QTcpSocket::connected, this, &LatencyProbe::onConnected);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &LatencyProbe::onError);
}

void LatencyProbe::setEndpoint(const QString &host, quint16 port)
{
    stop();
    m_host = host.trimmed();
    m_port = port;
}

void LatencyProbe::start()
{
    if (m_active || m_host.isEmpty() || m_port == 0)
        return;
    m_active = true;
    probe();
    m_interval.start();
}

void LatencyProbe::stop()
{
    m_active = false;
    m_interval.stop();
    m_timeout.stop();
    m_probing = false;
    m_socket.abort();
}

void LatencyProbe::probe()
{
    if (!m_active || m_probing || m_host.isEmpty() || m_port == 0)
        return;

    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.abort();

    m_probing = true;
    m_elapsed.start();
    m_timeout.start();
    m_socket.connectToHost(m_host, m_port);
}

void LatencyProbe::finish(int milliseconds)
{
    m_probing = false;
    m_timeout.stop();
    m_socket.abort();
    emit latencyChanged(milliseconds);
}

void LatencyProbe::onConnected()
{
    if (!m_probing)
        return;
    finish(static_cast<int>(m_elapsed.elapsed()));
}

void LatencyProbe::onError()
{
    if (!m_probing)
        return;
    finish(-1);
}

void LatencyProbe::onTimeout()
{
    if (!m_probing)
        return;
    finish(-1);
}

} // namespace nyan::ui
