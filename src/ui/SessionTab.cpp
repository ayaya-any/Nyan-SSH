#include "ui/SessionTab.h"

#include "core/HostKeyStore.h"
#include "core/SessionStore.h"
#include "core/SshWorker.h"
#include "term/TerminalWidget.h"
#include "ui/AuthDialogs.h"
#include "ui/Dialogs.h"
#include "ui/FilePanel.h"
#include "ui/Icons.h"
#include "ui/MaterialWidgets.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMetaObject>
#include <QSplitter>
#include <QStackedWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

namespace nyan::ui {
namespace {

QString cleanTitle(const QString &raw)
{
    QString title = raw.trimmed();
    title.remove(QChar(0x1B));
    if (title.size() > 120)
        title = title.left(120);
    return title;
}

} // namespace

SessionTab::SessionTab(const SessionProfile &profile, SessionStore *sessions, HostKeyStore *hostKeys,
                       AppSettings *settings, QWidget *parent)
    : QWidget(parent)
    , m_profile(profile)
    , m_sessions(sessions)
    , m_hostKeys(hostKeys)
    , m_settings(settings)
{
    m_effectivePassword = m_profile.password();
    m_effectivePassphrase = m_profile.passphrase();
    buildUi();
    if (m_settings)
        applySettings(m_settings);
    updateTitle();
    updateToolbar();
}

SessionTab::~SessionTab()
{
    m_destroying = true;
    if (m_filePanel) {
        m_filePanel->shutdown();
        delete m_filePanel;
        m_filePanel = nullptr;
    }
    cleanupWorker();
}

void SessionTab::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Toolbar ---------------------------------------------------------------
    auto *toolbar = new QWidget;
    toolbar->setFixedHeight(42);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(14, 0, 10, 0);
    toolbarLayout->setSpacing(8);

    m_statusDot = new QLabel;
    m_statusDot->setFixedSize(8, 8);
    toolbarLayout->addWidget(m_statusDot);

    m_stateLabel = new QLabel(QStringLiteral("准备连接"));
    m_stateLabel->setFont(Theme::instance().uiFont(9, QFont::DemiBold));
    toolbarLayout->addWidget(m_stateLabel);

    m_targetLabel = new QLabel(m_profile.subtitle());
    m_targetLabel->setFont(Theme::instance().uiFont(8.5));
    m_targetLabel->setProperty("role", QStringLiteral("muted"));
    m_targetLabel->setToolTip(QStringLiteral("%1\n用户：%2\n端口：%3")
                                  .arg(m_profile.host, m_profile.username)
                                  .arg(m_profile.port));
    toolbarLayout->addWidget(m_targetLabel);
    toolbarLayout->addStretch();

    m_securityButton = new MIconButton(QStringLiteral("shield"), QStringLiteral("主机密钥"), toolbar);
    m_securityButton->setButtonSize(30);
    m_securityButton->setActive(true);
    m_securityButton->hide();
    connect(m_securityButton, &QAbstractButton::clicked, this, [this] {
        if (!m_connectedFingerprint.isEmpty()) {
            Dialogs::message(this, QStringLiteral("主机密钥已验证"),
                             QStringLiteral("类型：%1\n指纹：%2\n\n该指纹已与本地记录匹配，本次会话免受中间人攻击。")
                                 .arg(m_connectedKeyType, m_connectedFingerprint),
                             QStringLiteral("关闭"));
        }
    });
    toolbarLayout->addWidget(m_securityButton);

    auto *separator = new QFrame;
    separator->setFixedSize(1, 18);
    separator->setStyleSheet(
        QStringLiteral("background:%1;").arg(Theme::instance().palette().border.name()));
    toolbarLayout->addWidget(separator);

    m_sftpButton = new MIconButton(QStringLiteral("folder-open"), QStringLiteral("文件面板 (Ctrl+Shift+F)"),
                                   toolbar);
    m_sftpButton->setButtonSize(30);
    connect(m_sftpButton, &QAbstractButton::clicked, this, &SessionTab::toggleFilePanel);
    toolbarLayout->addWidget(m_sftpButton);

    m_reconnectButton = new MIconButton(QStringLiteral("refresh"), QStringLiteral("重新连接"), toolbar);
    m_reconnectButton->setButtonSize(30);
    connect(m_reconnectButton, &QAbstractButton::clicked, this, [this] {
        if (m_state == SshState::Connecting || m_state == SshState::Authenticating)
            return;
        if (m_state == SshState::Connected)
            startWorker();
        else
            start();
    });
    toolbarLayout->addWidget(m_reconnectButton);

    m_disconnectButton = new MIconButton(QStringLiteral("power"), QStringLiteral("断开连接"), toolbar);
    m_disconnectButton->setButtonSize(30);
    connect(m_disconnectButton, &QAbstractButton::clicked, this, &SessionTab::disconnectSession);
    toolbarLayout->addWidget(m_disconnectButton);
    root->addWidget(toolbar);

    auto *hairline = new QFrame;
    hairline->setObjectName(QStringLiteral("hairline"));
    hairline->setFixedHeight(1);
    root->addWidget(hairline);

    // Stack -----------------------------------------------------------------
    m_stack = new QStackedWidget;

    m_connectingPage = new QWidget;
    auto *connectingLayout = new QVBoxLayout(m_connectingPage);
    connectingLayout->setContentsMargins(32, 32, 32, 32);
    connectingLayout->addStretch();
    m_connectingSpinner = new MSpinner;
    connectingLayout->addWidget(m_connectingSpinner, 0, Qt::AlignHCenter);
    m_connectingLabel = new QLabel(QStringLiteral("正在建立安全连接"));
    m_connectingLabel->setFont(Theme::instance().displayFont(15, QFont::DemiBold));
    m_connectingLabel->setAlignment(Qt::AlignCenter);
    connectingLayout->addWidget(m_connectingLabel);
    m_connectingDetail = new QLabel(m_profile.subtitle());
    m_connectingDetail->setProperty("role", QStringLiteral("muted"));
    m_connectingDetail->setFont(Theme::instance().uiFont(9.5));
    m_connectingDetail->setAlignment(Qt::AlignCenter);
    m_connectingDetail->setWordWrap(true);
    connectingLayout->addWidget(m_connectingDetail);
    connectingLayout->addStretch();
    m_stack->addWidget(m_connectingPage);

    m_terminalPage = new QWidget;
    auto *terminalLayout = new QVBoxLayout(m_terminalPage);
    terminalLayout->setContentsMargins(10, 8, 10, 10);
    terminalLayout->setSpacing(0);
    m_terminal = new term::TerminalWidget;
    terminalLayout->addWidget(m_terminal, 1);
    m_stack->addWidget(m_terminalPage);

    m_errorPage = new QWidget;
    auto *errorLayout = new QVBoxLayout(m_errorPage);
    errorLayout->setContentsMargins(32, 32, 32, 32);
    errorLayout->addStretch();
    auto *errorIcon = new QLabel;
    errorIcon->setPixmap(Icons::pixmap(QStringLiteral("alert"), Theme::instance().palette().danger, 44, 2.0));
    errorIcon->setAlignment(Qt::AlignCenter);
    errorLayout->addWidget(errorIcon);
    m_errorTitle = new QLabel(QStringLiteral("连接失败"));
    m_errorTitle->setFont(Theme::instance().displayFont(16, QFont::DemiBold));
    m_errorTitle->setAlignment(Qt::AlignCenter);
    errorLayout->addWidget(m_errorTitle);
    m_errorDetail = new QLabel;
    m_errorDetail->setProperty("role", QStringLiteral("muted"));
    m_errorDetail->setFont(Theme::instance().uiFont(9.5));
    m_errorDetail->setAlignment(Qt::AlignCenter);
    m_errorDetail->setWordWrap(true);
    m_errorDetail->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_errorDetail->setMaximumWidth(560);
    errorLayout->addWidget(m_errorDetail, 0, Qt::AlignHCenter);
    auto *retryButton = new MButton(QStringLiteral("重试"), MButton::Variant::Filled);
    retryButton->setIconName(QStringLiteral("refresh"));
    connect(retryButton, &QAbstractButton::clicked, this, &SessionTab::start);
    errorLayout->addWidget(retryButton, 0, Qt::AlignHCenter);
    errorLayout->addStretch();
    m_stack->addWidget(m_errorPage);

    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(1);
    m_splitter->addWidget(m_stack);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    root->addWidget(m_splitter, 1);

    connect(m_terminal, &term::TerminalWidget::inputData, this, [this](const QByteArray &data) {
        if (m_worker)
            m_worker->enqueueWrite(data);
    });
    connect(m_terminal, &term::TerminalWidget::sizeChanged, this, [this](int cols, int rows) {
        if (m_worker)
            m_worker->requestResize(cols, rows);
    });
    connect(m_terminal, &term::TerminalWidget::responseData, this, [this](const QByteArray &data) {
        if (m_worker)
            m_worker->enqueueWrite(data);
    });
    connect(m_terminal, &term::TerminalWidget::titleChanged, this, [this](const QString &title) {
        m_oscTitle = cleanTitle(title);
        updateTitle();
    });
    connect(m_terminal, &term::TerminalWidget::bell, this, [this] {
        if (!window()->isActiveWindow())
            QApplication::alert(window(), 1500);
    });
    connect(m_terminal, &term::TerminalWidget::fontSizeChangeRequested, this, [this](int size) {
        m_terminal->setFontSize(size);
        emit termFontSizeChanged(size);
    });
    connect(&Theme::instance(), &Theme::changed, this, [this] {
        setState(m_state, m_stateDetail);
        update();
    });

    setPage(Page::Connecting);
    setConnectingText(QStringLiteral("准备连接 %1").arg(m_profile.subtitle()));
    setState(SshState::Idle, QStringLiteral("准备连接"));
}

void SessionTab::applySettings(AppSettings *settings)
{
    m_settings = settings;
    if (!m_settings)
        return;
    if (m_terminal) {
        m_terminal->setFontFamily(m_settings->terminalFontFamily);
        m_terminal->setFontSize(m_settings->terminalFontSize);
        m_terminal->setScrollbackLimit(m_settings->scrollbackLines);
        m_terminal->setCopyOnSelect(m_settings->copyOnSelect);
    }
    if (m_filePanel)
        m_filePanel->setSettings(m_settings);
}

void SessionTab::start()
{
    if (m_destroying)
        return;
    if (m_state == SshState::Connecting || m_state == SshState::Authenticating
        || m_state == SshState::Connected) {
        return;
    }

    if (m_profile.auth == AuthType::Password && m_effectivePassword.isEmpty()) {
        if (!promptForSecret(false, QString(), QStringList())) {
            showErrorPage(QStringLiteral("需要密码"),
                          QStringLiteral("连接已取消：未提供登录密码。"));
            return;
        }
    }
    startWorker();
}

SshConfig SessionTab::buildConfig() const
{
    SshConfig config;
    config.host = m_profile.host;
    config.port = m_profile.port;
    config.username = m_profile.username;
    config.auth = m_profile.auth;
    config.password = m_effectivePassword;
    config.privateKeyPath = m_profile.privateKeyPath;
    config.passphrase = m_effectivePassphrase;
    config.terminalType = QStringLiteral("xterm-256color");
    if (m_terminal) {
        config.cols = m_terminal->screen().columns();
        config.rows = m_terminal->screen().rows();
    }
    return config;
}

void SessionTab::startWorker()
{
    if (m_destroying)
        return;
    if (m_filePanel && m_filePanel->isVisible())
        closeFilePanelSession();
    cleanupWorker();
    m_workerThread = new QThread;
    m_worker = new SshWorker;

    const QString storedFingerprint = m_hostKeys ? m_hostKeys->fingerprintFor(m_profile.host, m_profile.port)
                                                 : QString();
    const QString expectedFingerprint = !m_sessionFingerprint.isEmpty() ? m_sessionFingerprint
                                                                       : storedFingerprint;

    connect(m_worker, &SshWorker::stateChanged, this,
            [this](SshState state, const QString &detail) {
                if (m_destroying)
                    return;
                if (state == SshState::Connecting || state == SshState::Authenticating) {
                    setPage(Page::Connecting);
                    setConnectingText(detail);
                    setState(state, detail);
                } else if (state == SshState::Connected) {
                    m_everConnected = true;
                    setPage(Page::Terminal);
                    m_terminal->setFocus(Qt::OtherFocusReason);
                    setState(state, detail);
                    if (m_settings && m_settings->autoOpenFilePanel)
                        setFilePanelVisible(true);
                } else if (state == SshState::Failed) {
                    setState(state, detail);
                    if (!m_everConnected) {
                        showErrorPage(QStringLiteral("连接失败"), detail);
                    }
                } else if (state == SshState::Disconnecting) {
                    setState(state, detail);
                } else if (state == SshState::Disconnected) {
                    setState(state, detail);
                    closeFilePanelSession();
                    if (m_everConnected) {
                        m_terminal->setFocus(Qt::OtherFocusReason);
                        emit notify(QStringLiteral("连接已断开：%1").arg(detail), QString());
                    } else {
                        showErrorPage(QStringLiteral("连接已断开"), detail);
                    }
                }
            });

    connect(m_worker, &SshWorker::hostKeyPrompt, this,
            [this](const QString &keyType, const QString &fingerprint, const QString &expectedKey) {
                handleHostKeyPrompt(keyType, fingerprint, expectedKey);
            });
    connect(m_worker, &SshWorker::connected, this,
            [this](const QString &keyType, const QString &fingerprint) {
                m_connectedKeyType = keyType;
                m_connectedFingerprint = fingerprint;
                m_sessionFingerprint = fingerprint;
                m_securityButton->setVisible(true);
                m_securityButton->setToolTip(
                    QStringLiteral("%1\n%2").arg(keyType, fingerprint));
            });
    connect(m_worker, &SshWorker::dataReceived, this, [this](const QByteArray &data) {
        if (m_destroying)
            return;
        if (m_stack->currentWidget() != m_terminalPage)
            setPage(Page::Terminal);
        m_terminal->feed(data);
    });
    connect(m_worker, &SshWorker::authenticationFailed, this,
            [this](const QStringList &methods, const QString &message) {
                handleAuthenticationFailed(methods, message);
            });
    connect(m_worker, &SshWorker::errorOccurred, this, [this](const QString &message) {
        if (m_destroying)
            return;
        if (!m_everConnected)
            showErrorPage(QStringLiteral("连接失败"), message);
        else
            emit notify(message, QString());
    });
    connect(m_worker, &SshWorker::finished, m_workerThread, &QThread::quit);

    m_worker->moveToThread(m_workerThread);
    m_workerThread->start();

    if (m_sessions && m_sessions->find(m_profile.id)) {
        m_profile.lastUsedAt = QDateTime::currentDateTimeUtc();
        m_sessions->upsert(m_profile);
        m_sessions->save();
        emit sessionProfileChanged(m_profile);
    }

    setPage(Page::Connecting);
    setConnectingText(QStringLiteral("正在连接 %1:%2").arg(m_profile.host).arg(m_profile.port));
    setState(SshState::Connecting, QStringLiteral("正在连接 %1:%2").arg(m_profile.host).arg(m_profile.port));
    m_securityButton->setVisible(false);
    m_connectedFingerprint.clear();
    m_connectedKeyType.clear();
    m_terminal->resetScreen();
    m_oscTitle.clear();
    updateTitle();

    const SshConfig config = buildConfig();
    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker, config, expectedFingerprint] { worker->start(config, expectedFingerprint); },
        Qt::QueuedConnection);
}

void SessionTab::cleanupWorker()
{
    SshWorker *worker = m_worker;
    QThread *thread = m_workerThread;
    m_worker = nullptr;
    m_workerThread = nullptr;

    if (worker) {
        worker->requestStop();
        worker->respondHostKey(false);
    }

    if (thread) {
        thread->quit();
        if (!thread->wait(12000)) {
            // Never destroy a running QThread.  Detach the objects and let the
            // worker finish naturally; they self-delete afterwards.
            if (worker)
                worker->setParent(nullptr);
            thread->setParent(nullptr);
            if (worker) {
                QObject::connect(thread, &QThread::finished, worker,
                                 [worker] { worker->deleteLater(); });
            }
            QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
            return;
        }
    }

    delete worker;
    delete thread;
}

void SessionTab::setPage(Page page)
{
    if (!m_stack)
        return;
    switch (page) {
    case Page::Connecting:
        m_stack->setCurrentWidget(m_connectingPage);
        m_connectingSpinner->start();
        break;
    case Page::Terminal:
        m_stack->setCurrentWidget(m_terminalPage);
        m_connectingSpinner->stop();
        break;
    case Page::Error:
        m_stack->setCurrentWidget(m_errorPage);
        m_connectingSpinner->stop();
        break;
    }
}

void SessionTab::setConnectingText(const QString &message)
{
    if (m_connectingDetail)
        m_connectingDetail->setText(message);
}

void SessionTab::showErrorPage(const QString &title, const QString &detail)
{
    if (m_destroying)
        return;
    m_errorTitle->setText(title);
    m_errorDetail->setText(detail);
    setPage(Page::Error);
}

void SessionTab::disconnectSession()
{
    if (!m_worker)
        return;
    m_worker->requestStop();
    setState(SshState::Disconnecting, QStringLiteral("正在断开连接"));
}

void SessionTab::focusTerminal()
{
    if (m_stack && m_stack->currentWidget() == m_terminalPage && m_terminal)
        m_terminal->setFocus(Qt::OtherFocusReason);
}

QString SessionTab::displayTitle() const
{
    if (!m_oscTitle.isEmpty())
        return m_oscTitle;
    return m_profile.displayName();
}

void SessionTab::updateTitle()
{
    emit titleChanged(displayTitle());
}

void SessionTab::updateStatusDot(const QColor &color)
{
    if (!m_statusDot)
        return;
    m_statusDot->setStyleSheet(
        QStringLiteral("background:%1;border-radius:4px;").arg(color.name()));
}

void SessionTab::setState(SshState state, const QString &detail)
{
    m_state = state;
    m_stateDetail = detail;
    if (m_stateLabel)
        m_stateLabel->setText(sshStateLabel(state));
    if (m_stateLabel)
        m_stateLabel->setToolTip(detail);

    const Palette &palette = Theme::instance().palette();
    QColor dot = palette.textFaint;
    switch (state) {
    case SshState::Connected:
        dot = palette.success;
        break;
    case SshState::Connecting:
    case SshState::Authenticating:
    case SshState::Disconnecting:
        dot = palette.warning;
        break;
    case SshState::Failed:
        dot = palette.danger;
        break;
    default:
        dot = palette.textFaint;
        break;
    }
    updateStatusDot(dot);
    updateToolbar();
    emit stateChanged(state, detail);
}

void SessionTab::updateToolbar()
{
    if (!m_sftpButton)
        return;
    const bool connected = m_state == SshState::Connected;
    const bool busy = m_state == SshState::Connecting || m_state == SshState::Authenticating
                      || m_state == SshState::Disconnecting;
    m_sftpButton->setEnabled(connected);
    m_reconnectButton->setEnabled(!busy);
    m_disconnectButton->setEnabled(connected || busy);
    m_sftpButton->setActive(isFilePanelVisible());
}

void SessionTab::handleHostKeyPrompt(const QString &keyType, const QString &fingerprint,
                                    const QString &expectedFingerprint)
{
    if (m_destroying || !m_worker)
        return;

    HostKeyDialog dialog(this, m_profile.host, m_profile.port, keyType, fingerprint, expectedFingerprint);
    if (dialog.exec() != QDialog::Accepted) {
        m_worker->respondHostKey(false);
        return;
    }

    m_sessionFingerprint = fingerprint;
    if (dialog.rememberKey() && m_hostKeys) {
        m_hostKeys->trust(m_profile.host, m_profile.port, keyType, fingerprint);
        QString error;
        if (!m_hostKeys->save(&error))
            emit notify(error, QString());
    }
    if (m_worker)
        m_worker->respondHostKey(true);
}

void SessionTab::handleAuthenticationFailed(const QStringList &methods, const QString &message)
{
    if (m_destroying)
        return;
    if (m_promptingSecret)
        return;

    if (!m_everConnected) {
        showErrorPage(QStringLiteral("认证失败"), message);
    }

    m_promptingSecret = true;
    const bool ok = promptForSecret(true, message, methods);
    m_promptingSecret = false;
    if (ok) {
        startWorker();
    } else {
        showErrorPage(QStringLiteral("认证失败"), message);
        setState(SshState::Failed, message);
    }
}

bool SessionTab::promptForSecret(bool retry, const QString &error, const QStringList &methods)
{
    Q_UNUSED(methods);
    const bool keyMode = m_profile.auth == AuthType::PublicKey;
    PasswordDialog dialog(this,
                          keyMode ? PasswordDialog::Mode::Passphrase : PasswordDialog::Mode::Password,
                          m_profile.subtitle(),
                          error.isEmpty() ? QString() : error,
                          m_profile.rememberPassword);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    const QString secret = dialog.secret();
    if (secret.isEmpty())
        return false;
    rememberSecret(secret, dialog.remember());
    return true;
}

void SessionTab::rememberSecret(const QString &secret, bool remember)
{
    if (m_profile.auth == AuthType::PublicKey) {
        m_effectivePassphrase = secret;
        m_profile.setPassphrase(secret, remember);
    } else {
        m_effectivePassword = secret;
        m_profile.setPassword(secret, remember);
    }
    m_profile.rememberPassword = remember;
    emit sessionProfileChanged(m_profile);
}

void SessionTab::toggleFilePanel()
{
    setFilePanelVisible(!isFilePanelVisible());
}

bool SessionTab::isFilePanelVisible() const
{
    return m_filePanel && !m_filePanel->isHidden();
}

void SessionTab::setFilePanelVisible(bool visible)
{
    if (visible) {
        if (m_state != SshState::Connected) {
            emit notify(QStringLiteral("请先建立 SSH 连接"), QString());
            return;
        }
        if (!m_filePanel) {
            m_filePanel = new FilePanel;
            m_filePanel->setSettings(m_settings);
            connect(m_filePanel, &FilePanel::notify, this, &SessionTab::notify);
            connect(m_filePanel, &FilePanel::closeRequested, this, [this] {
                setFilePanelVisible(false);
            });
            m_splitter->addWidget(m_filePanel);
            m_splitter->setStretchFactor(1, 0);
            const int width = qMax(1040, this->width());
            m_splitter->setSizes({width - 470, 470});
        }
        m_filePanel->show();
        m_sftpButton->setActive(true);
        openFilePanel();
    } else {
        if (m_filePanel)
            m_filePanel->hide();
        if (m_sftpButton)
            m_sftpButton->setActive(false);
    }
}

void SessionTab::openFilePanel()
{
    if (!m_filePanel || m_state != SshState::Connected)
        return;
    if (m_connectedFingerprint.isEmpty()) {
        emit notify(QStringLiteral("主机密钥尚未确认，无法建立 SFTP 会话"), QString());
        return;
    }
    m_filePanel->openSession(buildConfig(), m_connectedFingerprint, m_filePanel->currentPath());
}

void SessionTab::closeFilePanelSession()
{
    if (!m_filePanel)
        return;
    m_filePanel->closeSession();
    m_filePanel->hide();
    m_sftpButton->setActive(false);
    updateToolbar();
}

} // namespace nyan::ui
