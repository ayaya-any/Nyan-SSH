#pragma once

#include <QWidget>

class QAbstractButton;
class QLabel;

namespace nyan::ui {

class MIconButton;

class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setMaximized(bool maximized);
    bool isMaximized() const { return m_maximized; }
    void setSidebarCollapsed(bool collapsed);
    void setBackdropActive(bool active);

    /// True when the global position lies on a caption button.  MainWindow's
    /// native hit test uses this to leave those pixels as client area.
    bool isControlAt(const QPoint &globalPosition) const;
    bool isOnTitleBar(const QPoint &globalPosition) const;

signals:
    void minimizeRequested();
    void maximizeToggleRequested();
    void closeRequested();
    void sidebarToggleRequested();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updateControlIcons();

    QLabel *m_logo = nullptr;
    QLabel *m_title = nullptr;
    MIconButton *m_sidebarToggle = nullptr;
    QAbstractButton *m_minimize = nullptr;
    QAbstractButton *m_maximize = nullptr;
    QAbstractButton *m_close = nullptr;
    QWidget *m_controls = nullptr;
    bool m_maximized = false;
    bool m_backdropActive = false;
};

} // namespace nyan::ui
