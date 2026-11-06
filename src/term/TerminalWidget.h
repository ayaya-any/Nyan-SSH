#pragma once

#include "term/TerminalScreen.h"

#include <QWidget>

class QTimer;

namespace nyan::term {

class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);

    TerminalScreen &screen() { return m_screen; }
    const TerminalScreen &screen() const { return m_screen; }

    void feed(const QByteArray &data);
    void setFontFamily(const QString &family);
    void setFontSize(int pointSize);
    int fontSize() const { return m_fontSize; }
    void setScrollbackLimit(int lines);
    void setCopyOnSelect(bool enabled);
    void resetScreen();

    void copySelection();
    void pasteClipboard();
    void selectAll();
    void clearScrollback();
    void scrollToBottom();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void inputData(const QByteArray &data);
    void sizeChanged(int columns, int rows);
    void titleChanged(const QString &title);
    void bell();
    void responseData(const QByteArray &data);
    void fontSizeChangeRequested(int pointSize);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct SelectionPoint {
        int row = 0;
        int col = 0;
    };

    void updateMetrics();
    void recomputeGrid();
    void sendText(const QByteArray &data);
    void moveSelectionTo(const QPoint &position);
    bool hasSelection() const;
    QString selectedText() const;
    QColor resolveColor(quint32 color, bool foreground) const;
    QColor blended(const QColor &color, int alpha) const;
    int selectionRowAt(int y) const;
    int selectionColumnAt(int x) const;
    void applyScroll(int deltaLines);
    void showTerminalMenu(const QPoint &globalPosition);

    TerminalScreen m_screen;
    QFont m_baseFont;
    QFont m_boldFont;
    QFont m_italicFont;
    QFont m_boldItalicFont;
    int m_fontSize = 11;
    QString m_fontFamily;

    int m_cellWidth = 8;
    int m_cellHeight = 16;
    int m_ascent = 12;
    int m_paddingLeft = 12;
    int m_paddingTop = 10;
    int m_paddingRight = 14;
    int m_paddingBottom = 10;

    int m_scrollOffset = 0;
    bool m_selecting = false;
    SelectionPoint m_selectionStart;
    SelectionPoint m_selectionEnd;
    bool m_hasSelection = false;
    bool m_copyOnSelect = false;
    bool m_mouseDragged = false;

    QTimer *m_blinkTimer = nullptr;
    bool m_cursorOn = true;
    bool m_focused = false;
    QString m_lastTitle;
};

} // namespace nyan::term
