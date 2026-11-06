// Exercises SshWorker: host-key prompt handshake, password auth, shell I/O and
// clean shutdown.  Requires the local test server.

#include "core/SshWorker.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QTextStream>
#include <QThread>
#include <QTimer>

using namespace nyan;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    SshConfig config;
    config.host = QStringLiteral("127.0.0.1");
    config.port = 2222;
    config.username = QStringLiteral("nyan");
    config.auth = AuthType::Password;
    config.password = QStringLiteral("nyan123");
    config.terminalType = QStringLiteral("xterm-256color");
    config.cols = 90;
    config.rows = 24;

    QThread thread;
    auto *worker = new SshWorker;
    worker->moveToThread(&thread);

    QString failure;
    bool prompted = false;
    bool connected = false;
    bool sawEcho = false;
    bool finished = false;

    auto stop = [&](const QString &message) {
        if (!message.isEmpty())
            failure = message;
        worker->requestStop();
        QTimer::singleShot(1500, &app, &QCoreApplication::quit);
    };

    QObject::connect(worker, &SshWorker::hostKeyPrompt, &app,
                     [&](const QString &keyType, const QString &fingerprint, const QString &expected) {
                         out << "host key prompt: " << keyType << " " << fingerprint
                             << " expected=" << (expected.isEmpty() ? QStringLiteral("<none>") : expected)
                             << "\n";
                         prompted = expected.isEmpty();
                         worker->respondHostKey(true);
                     });
    QObject::connect(worker, &SshWorker::connected, &app, [&](const QString &type, const QString &fp) {
        out << "connected with " << type << " " << fp << "\n";
        connected = true;
        worker->enqueueWrite(QByteArrayLiteral("echo worker-ok\r"));
    });
    QObject::connect(worker, &SshWorker::dataReceived, &app, [&](const QByteArray &data) {
        if (data.contains("worker-ok") && !sawEcho) {
            sawEcho = true;
            out << "shell write/read ok\n";
            worker->requestStop();
        }
    });
    QObject::connect(worker, &SshWorker::authenticationFailed, &app,
                     [&](const QStringList &methods, const QString &message) {
                         stop(QStringLiteral("auth failed: %1 (%2)")
                                  .arg(message, methods.join(QLatin1Char(','))));
                     });
    QObject::connect(worker, &SshWorker::errorOccurred, &app, [&](const QString &message) {
        if (!sawEcho && !message.contains(QStringLiteral("断开")))
            out << "worker message: " << message << "\n";
    });
    QObject::connect(worker, &SshWorker::finished, &app, [&] {
        finished = true;
        out << "worker finished\n";
        thread.quit();
    });
    QObject::connect(&thread, &QThread::finished, &app, &QCoreApplication::quit);

    QTimer::singleShot(25000, &app, [&] {
        failure = QStringLiteral("timeout");
        worker->requestStop();
        QCoreApplication::quit();
    });

    thread.start();
    QMetaObject::invokeMethod(
        worker, [=] { worker->start(config, QString()); }, Qt::QueuedConnection);

    app.exec();
    thread.quit();
    thread.wait(5000);
    delete worker;

    if (!failure.isEmpty()) {
        out << "FAIL: " << failure << "\n";
        return 1;
    }
    if (!prompted || !connected || !sawEcho || !finished) {
        out << "FAIL: prompted=" << prompted << " connected=" << connected
            << " echo=" << sawEcho << " finished=" << finished << "\n";
        return 1;
    }
    out << "ALL OK\n";
    return 0;
}
