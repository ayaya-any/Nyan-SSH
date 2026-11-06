#pragma once

#include <QWidget>

class QAbstractButton;
class QLabel;

namespace nyan::ui {

class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setMaximized(bool maximized);
    bool isMaximized() const { return m_maximized; }

    /// True when the global position lies on a caption button.  MainWindow's
    /// native hit test uses this to leave those pixels as client area.
    bool isControlAt(const QPoint &globalPosition) const;
    bool isOnTitleBar(const QPoint &globalPosition) const;

signals:
    void minimizeRequested();
    void maximizeToggleRequested();
    void closeRequested();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updateControlIcons();

    QLabel *m_logo = nullptr;
    QLabel *m_title = nullptr;
    QAbstractButton *m_minimize = nullptr;
    QAbstractButton *m_maximize = nullptr;
    QAbstractButton *m_close = nullptr;
    QWidget *m_controls = nullptr;
    bool m_maximized = false;
};

} // namespace nyan::ui
