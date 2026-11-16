#include "ui/SessionWindow.h"

#include "ui/Icons.h"
#include "ui/SessionTab.h"
#include "ui/Theme.h"

#include <QCloseEvent>
#include <QIcon>
#include <QShowEvent>

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

SessionWindow::SessionWindow(SessionTab *tab, QWidget *parent)
    : QMainWindow(parent)
    , m_tab(tab)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowIcon(QIcon(Icons::logo(64, 2.0)));
    setMinimumSize(760, 520);
    resize(1100, 760);
    if (tab) {
        setWindowTitle(tab->displayTitle());
        setCentralWidget(tab);
        tab->show();
        connect(tab, &SessionTab::titleChanged, this, [this](const QString &title) {
            setWindowTitle(title.isEmpty() && m_tab ? m_tab->profile().displayName() : title);
        });
    }
    connect(&Theme::instance(), &Theme::changed, this, [this] {
        updateWindowAppearance();
        setWindowIcon(QIcon(Icons::logo(64, 2.0)));
    });
}

SessionWindow::~SessionWindow() = default;

SessionTab *SessionWindow::takeSession()
{
    SessionTab *tab = m_tab;
    m_tab = nullptr;
    if (centralWidget() == tab)
        takeCentralWidget();
    if (tab && tab->parentWidget() == this)
        tab->setParent(nullptr);
    return tab;
}

void SessionWindow::prepareForClose()
{
    m_closing = true;
}

void SessionWindow::closeEvent(QCloseEvent *event)
{
    if (m_closing || !m_tab) {
        event->accept();
        return;
    }
    event->ignore();
    emit reattachRequested(this);
}

void SessionWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_tab)
        m_tab->show();
    updateWindowAppearance();
}

void SessionWindow::updateWindowAppearance()
{
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return;
    const BOOL dark = Theme::instance().mode() == ThemeMode::Dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
#endif
}

} // namespace nyan::ui
