#include "ui/MainWindow.h"

#include "core/AppPaths.h"
#include "ui/ConnectionDialog.h"
#include "ui/Dialogs.h"
#include "ui/Icons.h"
#include "ui/MaterialWidgets.h"
#include "ui/SessionTab.h"
#include "ui/SettingsDialog.h"
#include "ui/Sidebar.h"
#include "ui/Theme.h"
#include "ui/TitleBar.h"
#include "ui/WelcomePage.h"

#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QCursor>
#include <QDateTime>
#include <QEnterEvent>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <dwmapi.h>
#  include <windows.h>
#endif

namespace nyan::ui {
namespace {

/// A tab close button with asymmetric internal padding: the icon is nudged
/// away from the tab's right edge while the hit area remains comfortable.
class TabCloseButton : public QAbstractButton {
public:
    explicit TabCloseButton(QWidget *parent = nullptr)
        : QAbstractButton(parent)
    {
        setFixedSize(28, 28);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setToolTip(QStringLiteral("关闭标签页"));
        setAccessibleName(QStringLiteral("关闭标签页"));
        setAttribute(Qt::WA_Hover, true);
        connect(&Theme::instance(), &Theme::changed, this, qOverload<>(&QWidget::update));
    }

    QSize sizeHint() const override { return QSize(28, 28); }

protected:
    void enterEvent(QEnterEvent *event) override
    {
        update();
        QAbstractButton::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        update();
        QAbstractButton::leaveEvent(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        const Palette &palette = Theme::instance().palette();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const bool hovered = underMouse() && isEnabled();
        const QRectF hoverRect(2, (height() - 20) / 2.0, 20, 20);
        if (hovered) {
            QColor hoverFill = palette.danger;
            hoverFill.setAlpha(isDown() ? 90 : 36);
            painter.setPen(Qt::NoPen);
            painter.setBrush(hoverFill);
            painter.drawEllipse(hoverRect);
        }

        QColor iconColor = hovered ? palette.danger : palette.textMuted;
        if (!isEnabled())
            iconColor = palette.textFaint;
        const int iconSize = 14;
        // Center the icon at x=12, leaving a 9px gap on the right.
        const QRect target(5, (height() - iconSize) / 2, iconSize, iconSize);
        painter.drawPixmap(target, Icons::pixmap(QStringLiteral("close"), iconColor, iconSize,
                                                 devicePixelRatioF()));
    }
};

QIcon dotIcon(const QColor &color)
{
    QPixmap pixmap(24, 24);
    pixmap.setDevicePixelRatio(2.0);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    QColor glow = color;
    glow.setAlpha(55);
    painter.setBrush(glow);
    painter.drawEllipse(QRectF(1, 1, 10, 10));
    painter.setBrush(color);
    painter.drawEllipse(QRectF(3.2, 3.2, 5.6, 5.6));
    painter.end();
    return QIcon(pixmap);
}

QIcon stateIcon(SshState state)
{
    const Palette &palette = Theme::instance().palette();
    switch (state) {
    case SshState::Connected:
        return dotIcon(palette.success);
    case SshState::Connecting:
    case SshState::Authenticating:
    case SshState::Disconnecting:
        return dotIcon(palette.warning);
    case SshState::Failed:
        return dotIcon(palette.danger);
    default:
        return dotIcon(palette.textFaint);
    }
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_settings.load();
    Theme::instance().setMode(m_settings.theme);

    setWindowTitle(QStringLiteral("Nyan SSH"));
    setWindowIcon(QIcon(Icons::logo(64, 2.0)));
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint
                   | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_NativeWindow, true);
    resize(1320, 840);
    setMinimumSize(1040, 660);

    buildUi();
    loadData();
    restoreWindowState();
}

MainWindow::~MainWindow()
{
    saveWindowState();
    m_settings.save();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget;
    auto *outer = new QVBoxLayout(central);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_titleBar = new TitleBar(central);
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeToggleRequested, this, [this] {
        if (windowIsMaximized())
            showNormal();
        else
            showMaximized();
    });
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);
    outer->addWidget(m_titleBar);

    auto *root = new QHBoxLayout;
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_sidebar = new Sidebar;
    connect(m_sidebar, &Sidebar::newConnectionRequested, this, &MainWindow::newConnectionDialog);
    connect(m_sidebar, &Sidebar::connectRequested, this, &MainWindow::openSavedProfile);
    connect(m_sidebar, &Sidebar::editRequested, this, &MainWindow::editProfile);
    connect(m_sidebar, &Sidebar::duplicateRequested, this, &MainWindow::duplicateProfile);
    connect(m_sidebar, &Sidebar::deleteRequested, this, &MainWindow::deleteProfile);
    connect(m_sidebar, &Sidebar::settingsRequested, this, &MainWindow::showSettings);
    root->addWidget(m_sidebar);

    auto *right = new QWidget;
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    m_content = new QStackedWidget;
    m_welcome = new WelcomePage;
    connect(m_welcome, &WelcomePage::newConnectionRequested, this, &MainWindow::newConnectionDialog);
    connect(m_welcome, &WelcomePage::connectRequested, this, &MainWindow::openSavedProfile);
    connect(m_welcome, &WelcomePage::quickConnectRequested, this, [this](const SessionProfile &profile) {
        connectQuick(profile);
    });
    m_content->addWidget(m_welcome);

    m_tabs = new QTabWidget;
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setElideMode(Qt::ElideRight);
    m_tabs->setIconSize(QSize(12, 12));
    m_tabs->setUsesScrollButtons(true);
    m_tabs->tabBar()->setExpanding(false);
    m_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int) {
        if (SessionTab *tab = currentTab())
            tab->focusTerminal();
        updateStatusBar();
    });
    connect(m_tabs->tabBar(), &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        const int index = m_tabs->tabBar()->tabAt(position);
        if (index < 0)
            return;
        auto *tab = qobject_cast<SessionTab *>(m_tabs->widget(index));
        if (!tab)
            return;

        QMenu menu(this);
        QAction *reconnect = menu.addAction(QStringLiteral("重新连接"));
        QAction *filePanel = menu.addAction(QStringLiteral("文件面板"));
        filePanel->setEnabled(tab->state() == SshState::Connected);
        QAction *copyHost = menu.addAction(QStringLiteral("复制主机地址"));
        menu.addSeparator();
        QAction *closeCurrent = menu.addAction(QStringLiteral("关闭标签页"));
        QAction *closeOthers = menu.addAction(QStringLiteral("关闭其他标签页"));
        QAction *chosen = menu.exec(m_tabs->tabBar()->mapToGlobal(position));
        if (!chosen)
            return;
        if (chosen == reconnect) {
            m_tabs->setCurrentIndex(index);
            tab->start();
        } else if (chosen == filePanel) {
            m_tabs->setCurrentIndex(index);
            tab->setFilePanelVisible(!tab->isFilePanelVisible());
        } else if (chosen == copyHost) {
            QGuiApplication::clipboard()->setText(tab->profile().subtitle());
            showSnackbar(QStringLiteral("主机地址已复制"));
        } else if (chosen == closeCurrent) {
            closeTab(index);
        } else if (chosen == closeOthers) {
            for (int i = m_tabs->count() - 1; i >= 0; --i) {
                if (i != index)
                    closeTab(i);
            }
        }
    });

    auto *corner = new QWidget;
    auto *cornerLayout = new QHBoxLayout(corner);
    cornerLayout->setContentsMargins(6, 0, 10, 0);
    cornerLayout->setSpacing(2);
    auto *newButton = new MIconButton(QStringLiteral("plus"), QStringLiteral("新建连接 (Ctrl+N)"), corner);
    newButton->setButtonSize(28);
    connect(newButton, &QAbstractButton::clicked, this, &MainWindow::newConnectionDialog);
    cornerLayout->addWidget(newButton);
    auto *moreButton = new MIconButton(QStringLiteral("more"), QStringLiteral("更多"), corner);
    moreButton->setButtonSize(28);
    connect(moreButton, &QAbstractButton::clicked, this, [this, moreButton] {
        QMenu menu(this);
        QAction *newAction = menu.addAction(QStringLiteral("新建连接"));
        QAction *settingsAction = menu.addAction(QStringLiteral("设置"));
        menu.addSeparator();
        QAction *aboutAction = menu.addAction(QStringLiteral("关于 Nyan SSH"));
        QAction *quitAction = menu.addAction(QStringLiteral("退出"));
        QAction *chosen = menu.exec(moreButton->mapToGlobal(QPoint(0, moreButton->height())));
        if (chosen == newAction)
            newConnectionDialog();
        else if (chosen == settingsAction)
            showSettings();
        else if (chosen == aboutAction)
            showAbout();
        else if (chosen == quitAction)
            close();
    });
    cornerLayout->addWidget(moreButton);
    m_tabs->setCornerWidget(corner, Qt::TopRightCorner);
    m_content->addWidget(m_tabs);
    rightLayout->addWidget(m_content, 1);

    auto *statusBar = new QWidget;
    statusBar->setObjectName(QStringLiteral("appStatusBar"));
    statusBar->setFixedHeight(27);
    auto *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(14, 0, 14, 0);
    statusLayout->setSpacing(8);
    m_statusLeft = new QLabel(QStringLiteral("就绪"));
    m_statusLeft->setFont(Theme::instance().uiFont(8.3));
    m_statusLeft->setProperty("role", QStringLiteral("muted"));
    statusLayout->addWidget(m_statusLeft);
    statusLayout->addStretch();
    m_statusRight = new QLabel;
    m_statusRight->setFont(Theme::instance().uiFont(8.3));
    m_statusRight->setProperty("role", QStringLiteral("faint"));
    statusLayout->addWidget(m_statusRight);
    rightLayout->addWidget(statusBar);

    root->addWidget(right, 1);
    outer->addLayout(root, 1);
    setCentralWidget(central);

    m_snackbar = new MSnackbar(this);

    auto *newShortcut = new QShortcut(QKeySequence::New, this);
    connect(newShortcut, &QShortcut::activated, this, &MainWindow::newConnectionDialog);
    auto *settingsShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma), this);
    connect(settingsShortcut, &QShortcut::activated, this, &MainWindow::showSettings);
    auto *closeShortcut = new QShortcut(QKeySequence::Close, this);
    connect(closeShortcut, &QShortcut::activated, this, [this] {
        if (m_tabs->count() > 0)
            closeTab(m_tabs->currentIndex());
    });
    auto *fileShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")), this);
    connect(fileShortcut, &QShortcut::activated, this, [this] {
        if (SessionTab *tab = currentTab())
            tab->setFilePanelVisible(!tab->isFilePanelVisible());
    });
    auto *nextTabShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Tab")), this);
    connect(nextTabShortcut, &QShortcut::activated, this, [this] {
        if (m_tabs->count() > 1)
            m_tabs->setCurrentIndex((m_tabs->currentIndex() + 1) % m_tabs->count());
    });
    auto *prevTabShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Tab")), this);
    connect(prevTabShortcut, &QShortcut::activated, this, [this] {
        if (m_tabs->count() > 1)
            m_tabs->setCurrentIndex((m_tabs->currentIndex() - 1 + m_tabs->count()) % m_tabs->count());
    });

    m_content->setCurrentWidget(m_welcome);
    m_statusRight->setText(QStringLiteral("Nyan SSH %1 · Qt %2")
                               .arg(QApplication::applicationVersion(), QString::fromLatin1(qVersion())));

    connect(&Theme::instance(), &Theme::changed, this, [this] {
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (auto *tab = qobject_cast<SessionTab *>(m_tabs->widget(i)))
                updateTabIcon(tab);
        }
        updateStatusBar();
        update();
    });
}

void MainWindow::loadData()
{
    QString error;
    if (!AppPaths::ensureDataDir(&error))
        QTimer::singleShot(300, this, [this, error] { showSnackbar(error); });
    if (!m_sessions.load(&error))
        QTimer::singleShot(350, this, [this, error] { showSnackbar(error); });
    if (!m_hostKeys.load(&error))
        QTimer::singleShot(400, this, [this, error] { showSnackbar(error); });
    refreshLibrary();
    updateStatusBar();
}

void MainWindow::refreshLibrary()
{
    m_sidebar->setProfiles(m_sessions.profiles());
    m_welcome->setProfiles(m_sessions.recent(4));
    m_welcome->setStats(m_sessions.profiles().size(), m_hostKeys.count());
    updateActiveCount();
    updateStatusBar();
}

void MainWindow::updateActiveCount()
{
    const int active = activeSessionCount();
    m_sidebar->setActiveSessionCount(active);
}

int MainWindow::activeSessionCount() const
{
    int count = 0;
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *tab = qobject_cast<SessionTab *>(m_tabs->widget(i));
        if (!tab)
            continue;
        const SshState state = tab->state();
        if (state == SshState::Connected || state == SshState::Connecting
            || state == SshState::Authenticating)
            ++count;
    }
    return count;
}

void MainWindow::updateStatusBar()
{
    if (!m_statusLeft)
        return;
    SessionTab *tab = currentTab();
    if (tab) {
        QString text = QStringLiteral("%1 · %2").arg(sshStateLabel(tab->state()), tab->profile().subtitle());
        if (tab->state() == SshState::Failed)
            text += QStringLiteral(" · 连接异常");
        m_statusLeft->setText(text);
    } else {
        m_statusLeft->setText(QStringLiteral("就绪 · 从侧边栏选择会话，或使用快速连接"));
    }
    if (m_statusRight) {
        m_statusRight->setText(QStringLiteral("%1 个会话 · %2 台已知主机 · %3 个活动")
                                   .arg(m_sessions.profiles().size())
                                   .arg(m_hostKeys.count())
                                   .arg(activeSessionCount()));
    }
}

SessionTab *MainWindow::currentTab() const
{
    return qobject_cast<SessionTab *>(m_tabs->currentWidget());
}

SessionTab *MainWindow::tabForProfile(const QString &id) const
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *tab = qobject_cast<SessionTab *>(m_tabs->widget(i));
        if (tab && tab->sessionId() == id)
            return tab;
    }
    return nullptr;
}

void MainWindow::openSavedProfile(const QString &id)
{
    const SessionProfile *profile = m_sessions.find(id);
    if (!profile) {
        showSnackbar(QStringLiteral("会话不存在或已被删除"));
        return;
    }
    if (SessionTab *tab = tabForProfile(id)) {
        m_tabs->setCurrentWidget(tab);
        m_content->setCurrentWidget(m_tabs);
        if (tab->state() == SshState::Idle || tab->state() == SshState::Disconnected
            || tab->state() == SshState::Failed) {
            tab->start();
        } else {
            tab->focusTerminal();
        }
        return;
    }
    openConnection(*profile, false);
}

void MainWindow::connectQuick(const SessionProfile &profile)
{
    openConnection(profile, false);
}

void MainWindow::openConnection(const SessionProfile &profile, bool persist)
{
    if (SessionTab *existing = tabForProfile(profile.id)) {
        m_tabs->setCurrentWidget(existing);
        m_content->setCurrentWidget(m_tabs);
        existing->start();
        return;
    }

    if (persist) {
        m_sessions.upsert(profile);
        QString error;
        if (!m_sessions.save(&error))
            showSnackbar(error);
        refreshLibrary();
    }

    auto *tab = new SessionTab(profile, &m_sessions, &m_hostKeys, &m_settings, this);
    connect(tab, &SessionTab::titleChanged, this, [this, tab](const QString &title) {
        const int index = m_tabs->indexOf(tab);
        if (index < 0)
            return;
        m_tabs->setTabText(index, title.isEmpty() ? tab->profile().displayName() : title);
        m_tabs->setTabToolTip(index, QStringLiteral("%1\n%2")
                                        .arg(tab->profile().displayName(), tab->profile().subtitle()));
    });
    connect(tab, &SessionTab::stateChanged, this, [this, tab](SshState, const QString &) {
        updateTabAppearance(tab);
        updateActiveCount();
        updateStatusBar();
    });
    connect(tab, &SessionTab::notify, this, [this](const QString &message, const QString &actionText) {
        showSnackbar(message, actionText);
    });
    connect(tab, &SessionTab::sessionProfileChanged, this, [this](const SessionProfile &profile) {
        const bool existed = m_sessions.find(profile.id) != nullptr;
        if (!existed && !profile.rememberPassword)
            return;
        m_sessions.upsert(profile);
        QString error;
        if (!m_sessions.save(&error))
            showSnackbar(error);
        if (!existed)
            showSnackbar(QStringLiteral("会话已保存到侧边栏"));
        refreshLibrary();
    });
    connect(tab, &SessionTab::termFontSizeChanged, this, [this](int size) {
        m_settings.terminalFontSize = qBound(7, size, 32);
        m_settings.save();
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (auto *other = qobject_cast<SessionTab *>(m_tabs->widget(i)))
                other->applySettings(&m_settings);
        }
    });

    const int index = m_tabs->addTab(tab, tab->displayTitle());
    m_tabs->setTabToolTip(index, QStringLiteral("%1\n%2")
                                     .arg(tab->profile().displayName(), tab->profile().subtitle()));
    auto *closeButton = new TabCloseButton(m_tabs->tabBar());
    connect(closeButton, &QAbstractButton::clicked, this, [this, tab] {
        const int tabIndex = m_tabs->indexOf(tab);
        if (tabIndex >= 0)
            closeTab(tabIndex);
    });
    m_tabs->tabBar()->setTabButton(index, QTabBar::RightSide, closeButton);
    updateTabIcon(tab);
    m_tabs->setCurrentIndex(index);
    m_content->setCurrentWidget(m_tabs);
    refreshLibrary();
    tab->start();
}

void MainWindow::updateTabIcon(SessionTab *tab)
{
    const int index = m_tabs->indexOf(tab);
    if (index >= 0)
        m_tabs->setTabIcon(index, stateIcon(tab->state()));
}

void MainWindow::updateTabAppearance(SessionTab *tab)
{
    if (!tab)
        return;
    const int index = m_tabs->indexOf(tab);
    if (index < 0)
        return;
    updateTabIcon(tab);
    const QString title = tab->displayTitle();
    m_tabs->setTabText(index, title);
}

void MainWindow::closeTab(int index)
{
    auto *tab = qobject_cast<SessionTab *>(m_tabs->widget(index));
    if (!tab)
        return;
    m_tabs->removeTab(index);
    tab->disconnectSession();
    tab->deleteLater();
    if (m_tabs->count() == 0)
        m_content->setCurrentWidget(m_welcome);
    updateActiveCount();
    updateStatusBar();
    refreshLibrary();
}

void MainWindow::closeAllTabs()
{
    while (m_tabs->count() > 0) {
        auto *tab = qobject_cast<SessionTab *>(m_tabs->widget(0));
        m_tabs->removeTab(0);
        delete tab;
    }
}

void MainWindow::newConnectionDialog()
{
    ConnectionDialog dialog(this, ConnectionDialog::Mode::Create);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const SessionProfile profile = dialog.profile();
    if (dialog.saveRequested()) {
        m_sessions.upsert(profile);
        QString error;
        if (!m_sessions.save(&error))
            showSnackbar(error);
        refreshLibrary();
    }
    openConnection(profile, false);
}

void MainWindow::editProfile(const QString &id)
{
    SessionProfile *profile = m_sessions.find(id);
    if (!profile)
        return;
    ConnectionDialog dialog(this, ConnectionDialog::Mode::Edit, profile);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const SessionProfile updated = dialog.profile();
    const bool reconnect = dialog.connectAfterSave() && tabForProfile(id) != nullptr;
    if (reconnect) {
        for (int i = m_tabs->count() - 1; i >= 0; --i) {
            auto *tab = qobject_cast<SessionTab *>(m_tabs->widget(i));
            if (tab && tab->sessionId() == id)
                closeTab(i);
        }
    }
    m_sessions.upsert(updated);
    QString error;
    if (!m_sessions.save(&error))
        showSnackbar(error);
    refreshLibrary();

    if (dialog.connectAfterSave())
        openConnection(updated, false);
}

void MainWindow::duplicateProfile(const QString &id)
{
    const SessionProfile *profile = m_sessions.find(id);
    if (!profile)
        return;
    SessionProfile copy = *profile;
    copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copy.name = profile->displayName() + QStringLiteral(" 副本");
    copy.autoLogin = false;
    copy.createdAt = QDateTime::currentDateTimeUtc();
    copy.lastUsedAt = QDateTime();
    m_sessions.upsert(copy);
    QString error;
    if (!m_sessions.save(&error))
        showSnackbar(error);
    refreshLibrary();
    showSnackbar(QStringLiteral("已复制会话"));
}

void MainWindow::deleteProfile(const QString &id)
{
    const SessionProfile *profile = m_sessions.find(id);
    if (!profile)
        return;
    const bool open = tabForProfile(id) != nullptr;
    QString message = QStringLiteral("确定要从会话列表中删除“%1”吗？").arg(profile->displayName());
    if (open)
        message += QStringLiteral("\n\n该会话当前处于打开状态，删除后现有连接不会中断。");
    if (!Dialogs::confirm(this, QStringLiteral("删除会话"), message, QStringLiteral("删除"), true))
        return;
    m_sessions.remove(id);
    QString error;
    if (!m_sessions.save(&error))
        showSnackbar(error);
    refreshLibrary();
    showSnackbar(QStringLiteral("会话已从列表移除"));
}

void MainWindow::showSettings()
{
    SettingsDialog dialog(this, m_settings);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_settings = dialog.settings();
    m_settings.save();
    Theme::instance().setMode(m_settings.theme);
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *tab = qobject_cast<SessionTab *>(m_tabs->widget(i)))
            tab->applySettings(&m_settings);
    }
    refreshLibrary();
    showSnackbar(QStringLiteral("设置已保存"));
}

void MainWindow::showAbout()
{
    Dialogs::message(this, QStringLiteral("关于 Nyan SSH"),
                     QStringLiteral("Nyan SSH %1\n\n"
                                    "Material 风格 + VS2026 配色的 SSH 客户端。\n"
                                    "CMake · Qt %2 · C++20 · libssh2 · WinCNG\n\n"
                                    "功能：xterm 终端、SFTP 文件管理、会话记忆、"
                                    "DPAPI 凭据加密与自动登录。")
                         .arg(QApplication::applicationVersion(), QString::fromLatin1(qVersion())),
                     QStringLiteral("关闭"));
}

void MainWindow::showSnackbar(const QString &message, const QString &actionText)
{
    if (m_snackbar)
        m_snackbar->showMessage(message, actionText);
}

void MainWindow::runAutoLogin()
{
    if (m_autoLoginStarted)
        return;
    m_autoLoginStarted = true;

    QVector<SessionProfile> candidates;
    for (const SessionProfile &profile : m_sessions.profiles()) {
        if (profile.isUsableForAutoLogin())
            candidates.append(profile);
    }
    if (candidates.isEmpty())
        return;

    int delay = 200;
    for (const SessionProfile &profile : candidates) {
        const QString id = profile.id;
        QTimer::singleShot(delay, this, [this, id] {
            if (m_closing || tabForProfile(id))
                return;
            const SessionProfile *current = m_sessions.find(id);
            if (current)
                openConnection(*current, false);
        });
        delay += 450;
    }
    if (!candidates.isEmpty())
        showSnackbar(QStringLiteral("正在自动登录 %1 个会话").arg(candidates.size()));
}

void MainWindow::saveWindowState()
{
    QSettings settings(m_settings.path(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState());
    settings.sync();
}

void MainWindow::restoreWindowState()
{
    QSettings settings(m_settings.path(), QSettings::IniFormat);
    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    const QByteArray state = settings.value(QStringLiteral("window/state")).toByteArray();
    if (!state.isEmpty())
        restoreState(state);
}

void MainWindow::prepareNativeFrame()
{
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return;

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    // Qt creates frameless windows as WS_POPUP.  Turn it into a normal
    // overlapped window so Windows provides Aero Snap, the native system menu
    // and DWM shadow, while WM_NCCALCSIZE keeps the client area borderless.
    style &= ~WS_POPUP;
    style |= WS_OVERLAPPEDWINDOW;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_APPWINDOW;
    exStyle &= ~WS_EX_TOOLWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    // Windows 11 rounded corners for the frameless shell.  Ignored on older
    // systems and when DWM composition is unavailable.
    const int cornerPreference = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &cornerPreference,
                          sizeof(cornerPreference));
#endif
}

bool MainWindow::windowIsMaximized()
{
#ifdef Q_OS_WIN
    return IsZoomed(reinterpret_cast<HWND>(winId())) != FALSE || isMaximized();
#else
    return isMaximized();
#endif
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if ((eventType == QByteArrayLiteral("windows_generic_MSG")
         || eventType == QByteArrayLiteral("windows_dispatcher_MSG"))
        && message && result) {
        auto *msg = static_cast<MSG *>(message);
        if (msg->message == WM_NCCALCSIZE && msg->wParam) {
            // Our frameless window has native WS_THICKFRAME/WS_CAPTION styles
            // for resizing and window shadow.  Keep the client area flush with
            // the window normally, but clamp it to the monitor work area when
            // maximized; otherwise the resize frame would push the content
            // eight pixels off-screen on every side.
            if (IsZoomed(msg->hwnd)) {
                auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
                const HMONITOR monitor = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO info{};
                info.cbSize = sizeof(info);
                if (GetMonitorInfoW(monitor, &info)) {
                    params->rgrc[0] = info.rcWork;
                    *result = 0;
                    return true;
                }
            }
            return false;
        }
        if (msg->message == WM_SIZE && m_titleBar) {
            m_titleBar->setMaximized(IsZoomed(msg->hwnd) != FALSE);
        }
        if (msg->message == WM_NCHITTEST) {
            const QPoint global = QCursor::pos();
            const QPoint local = mapFromGlobal(global);
            if (!rect().contains(local))
                return false;

            if (!isMaximized() && !isFullScreen()) {
                constexpr int border = 7;
                const bool left = local.x() < border;
                const bool right = local.x() >= width() - border;
                const bool top = local.y() < border;
                const bool bottom = local.y() >= height() - border;

                if (top && left)
                    *result = HTTOPLEFT;
                else if (top && right)
                    *result = HTTOPRIGHT;
                else if (bottom && left)
                    *result = HTBOTTOMLEFT;
                else if (bottom && right)
                    *result = HTBOTTOMRIGHT;
                else if (left)
                    *result = HTLEFT;
                else if (right)
                    *result = HTRIGHT;
                else if (top)
                    *result = HTTOP;
                else if (bottom)
                    *result = HTBOTTOM;
                else
                    *result = 0;

                if (*result != 0)
                    return true;
            }

            if (m_titleBar && m_titleBar->isOnTitleBar(global)) {
                if (m_titleBar->isControlAt(global)) {
                    *result = HTCLIENT;
                    return true;
                }
                *result = HTCAPTION;
                return true;
            }
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (!m_nativeFramePrepared) {
        prepareNativeFrame();
        m_nativeFramePrepared = true;
    }
    if (m_titleBar)
        m_titleBar->setMaximized(windowIsMaximized());
    if (!m_shown) {
        m_shown = true;
        QTimer::singleShot(700, this, &MainWindow::runAutoLogin);
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange && m_titleBar) {
        m_titleBar->setMaximized(windowIsMaximized());
        QTimer::singleShot(0, this, [this] {
            if (m_titleBar)
                m_titleBar->setMaximized(windowIsMaximized());
        });
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_closing) {
        event->accept();
        return;
    }

    if (m_settings.confirmOnExit) {
        const int active = activeSessionCount();
        if (active > 0) {
            if (!Dialogs::confirm(this, QStringLiteral("退出 Nyan SSH"),
                                  QStringLiteral("仍有 %1 个活动会话，退出将断开所有连接。").arg(active),
                                  QStringLiteral("退出"), true)) {
                event->ignore();
                return;
            }
        }
    }

    m_closing = true;
    saveWindowState();
    m_settings.save();
    closeAllTabs();
    event->accept();
}

} // namespace nyan::ui
