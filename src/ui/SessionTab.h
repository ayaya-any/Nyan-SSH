#pragma once

#include "core/AppSettings.h"
#include "core/SessionProfile.h"
#include "core/SshTypes.h"

#include <QWidget>

class QLabel;
class QSplitter;
class QStackedWidget;
class QThread;
class QVBoxLayout;

namespace nyan {
class HostKeyStore;
class SessionStore;
class SshWorker;
}

namespace nyan::term {
class TerminalWidget;
}

namespace nyan::ui {

class FilePanel;
class LatencyProbe;
class MButton;
class MIconButton;
class MSpinner;

class SessionTab : public QWidget {
    Q_OBJECT
public:
    SessionTab(const SessionProfile &profile, SessionStore *sessions, HostKeyStore *hostKeys,
               AppSettings *settings, QWidget *parent = nullptr);
    ~SessionTab() override;

    void start();
    void focusTerminal();
    void disconnectSession();
    void setFilePanelVisible(bool visible);
    bool isFilePanelVisible() const;
    void applySettings(AppSettings *settings);

    const SessionProfile &profile() const { return m_profile; }
    QString sessionId() const { return m_profile.id; }
    QString displayTitle() const;
    SshState state() const { return m_state; }

signals:
    void titleChanged(const QString &title);
    void stateChanged(nyan::SshState state, const QString &detail);
    void notify(const QString &message, const QString &actionText);
    void sessionProfileChanged(const nyan::SessionProfile &profile);
    void termFontSizeChanged(int pointSize);

private:
    enum class Page {
        Connecting,
        Terminal,
        Error,
    };

    void buildUi();
    void startWorker();
    void cleanupWorker();
    void setPage(Page page);
    void setConnectingText(const QString &message);
    void showErrorPage(const QString &title, const QString &detail);
    void updateToolbar();
    void setState(SshState state, const QString &detail);
    void updateStatusDot(const QColor &color);
    void updateLatency(int milliseconds);
    void updateTitle();

    void handleHostKeyPrompt(const QString &keyType, const QString &fingerprint,
                             const QString &expectedFingerprint);
    void handleAuthenticationFailed(const QStringList &methods, const QString &message);
    bool promptForSecret(bool retry, const QString &error, const QStringList &methods);
    void rememberSecret(const QString &secret, bool remember);

    void toggleFilePanel();
    void openFilePanel();
    void closeFilePanelSession();
    SshConfig buildConfig() const;

    SessionProfile m_profile;
    SessionStore *m_sessions = nullptr;
    HostKeyStore *m_hostKeys = nullptr;
    AppSettings *m_settings = nullptr;

    SshWorker *m_worker = nullptr;
    QThread *m_workerThread = nullptr;
    SshState m_state = SshState::Idle;
    QString m_stateDetail;
    QString m_effectivePassword;
    QString m_effectivePassphrase;
    QString m_sessionFingerprint;
    QString m_connectedFingerprint;
    QString m_connectedKeyType;
    QString m_oscTitle;
    bool m_everConnected = false;
    bool m_promptingSecret = false;
    bool m_destroying = false;

    QLabel *m_statusDot = nullptr;
    QLabel *m_stateLabel = nullptr;
    QLabel *m_targetLabel = nullptr;
    QWidget *m_latencyChip = nullptr;
    QLabel *m_latencyIcon = nullptr;
    QLabel *m_latencyText = nullptr;
    LatencyProbe *m_latencyProbe = nullptr;
    int m_latencyMs = -1;
    MIconButton *m_securityButton = nullptr;
    MIconButton *m_sftpButton = nullptr;
    MIconButton *m_reconnectButton = nullptr;
    MIconButton *m_disconnectButton = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_connectingPage = nullptr;
    QWidget *m_terminalPage = nullptr;
    QWidget *m_errorPage = nullptr;
    MSpinner *m_connectingSpinner = nullptr;
    QLabel *m_connectingLabel = nullptr;
    QLabel *m_connectingDetail = nullptr;
    QLabel *m_errorTitle = nullptr;
    QLabel *m_errorDetail = nullptr;
    term::TerminalWidget *m_terminal = nullptr;
    FilePanel *m_filePanel = nullptr;
    QSplitter *m_splitter = nullptr;
};

} // namespace nyan::ui
