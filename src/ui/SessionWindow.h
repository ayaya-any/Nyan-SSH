#pragma once

#include <QMainWindow>

class QCloseEvent;
class QShowEvent;

namespace nyan::ui {

class SessionTab;

/// A native top-level window that owns an existing SessionTab while it is
/// detached from the main tab bar.  Closing it docks the session back into the
/// main window instead of destroying a live connection.
class SessionWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit SessionWindow(SessionTab *tab, QWidget *parent = nullptr);
    ~SessionWindow() override;

    SessionTab *takeSession();
    void prepareForClose();

signals:
    void reattachRequested(SessionWindow *window);

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void updateWindowAppearance();

    SessionTab *m_tab = nullptr;
    bool m_closing = false;
};

} // namespace nyan::ui
