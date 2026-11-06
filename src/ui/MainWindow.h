#pragma once

#include "core/AppSettings.h"
#include "core/HostKeyStore.h"
#include "core/SessionProfile.h"
#include "core/SessionStore.h"

#include <QMainWindow>

class QLabel;
class QStackedWidget;
class QTabWidget;

namespace nyan::ui {

class MSnackbar;
class SessionTab;
class Sidebar;
class TitleBar;
class WelcomePage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void connectQuick(const SessionProfile &profile);
    void openSavedProfile(const QString &id);

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void buildUi();
    void loadData();
    void refreshLibrary();
    void updateStatusBar();
    void updateActiveCount();

    void newConnectionDialog();
    void editProfile(const QString &id);
    void duplicateProfile(const QString &id);
    void deleteProfile(const QString &id);
    void showSettings();
    void showAbout();

    void openConnection(const SessionProfile &profile, bool persist);
    void closeTab(int index);
    void closeAllTabs();
    void updateTabAppearance(SessionTab *tab);
    void updateTabIcon(SessionTab *tab);
    SessionTab *currentTab() const;
    SessionTab *tabForProfile(const QString &id) const;
    int activeSessionCount() const;

    void showSnackbar(const QString &message, const QString &actionText = QString());
    void saveWindowState();
    void restoreWindowState();
    void runAutoLogin();
    void applySettings();
    void applyTheme(const QString &message = QString());
    void prepareNativeFrame();
    bool windowIsMaximized();

    AppSettings m_settings;
    SessionStore m_sessions;
    HostKeyStore m_hostKeys;

    Sidebar *m_sidebar = nullptr;
    TitleBar *m_titleBar = nullptr;
    QStackedWidget *m_content = nullptr;
    WelcomePage *m_welcome = nullptr;
    QTabWidget *m_tabs = nullptr;
    MSnackbar *m_snackbar = nullptr;
    QLabel *m_statusLeft = nullptr;
    QLabel *m_statusRight = nullptr;

    bool m_shown = false;
    bool m_closing = false;
    bool m_autoLoginStarted = false;
    bool m_nativeFramePrepared = false;
};

} // namespace nyan::ui
