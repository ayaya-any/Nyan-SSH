#include "term/TerminalWidget.h"

#include "ui/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace nyan::term {
namespace {

constexpr int kBlinkIntervalMs = 530;

QByteArray keySequence(int key, bool applicationCursor)
{
    switch (key) {
    case Qt::Key_Up:
        return applicationCursor ? QByteArrayLiteral("\x1bOA") : QByteArrayLiteral("\x1b[A");
    case Qt::Key_Down:
        return applicationCursor ? QByteArrayLiteral("\x1bOB") : QByteArrayLiteral("\x1b[B");
    case Qt::Key_Right:
        return applicationCursor ? QByteArrayLiteral("\x1bOC") : QByteArrayLiteral("\x1b[C");
    case Qt::Key_Left:
        return applicationCursor ? QByteArrayLiteral("\x1bOD") : QByteArrayLiteral("\x1b[D");
    case Qt::Key_Home:
        return applicationCursor ? QByteArrayLiteral("\x1bOH") : QByteArrayLiteral("\x1b[H");
    case Qt::Key_End:
        return applicationCursor ? QByteArrayLiteral("\x1bOF") : QByteArrayLiteral("\x1b[F");
    case Qt::Key_Insert:
        return QByteArrayLiteral("\x1b[2~");
    case Qt::Key_Delete:
        return QByteArrayLiteral("\x1b[3~");
    case Qt::Key_PageUp:
        return QByteArrayLiteral("\x1b[5~");
    case Qt::Key_PageDown:
        return QByteArrayLiteral("\x1b[6~");
    case Qt::Key_Backtab:
        return QByteArrayLiteral("\x1b[Z");
    case Qt::Key_F1:
        return QByteArrayLiteral("\x1bOP");
    case Qt::Key_F2:
        return QByteArrayLiteral("\x1bOQ");
    case Qt::Key_F3:
        return QByteArrayLiteral("\x1bOR");
    case Qt::Key_F4:
        return QByteArrayLiteral("\x1bOS");
    case Qt::Key_F5:
        return QByteArrayLiteral("\x1b[15~");
    case Qt::Key_F6:
        return QByteArrayLiteral("\x1b[17~");
    case Qt::Key_F7:
        return QByteArrayLiteral("\x1b[18~");
    case Qt::Key_F8:
        return QByteArrayLiteral("\x1b[19~");
    case Qt::Key_F9:
        return QByteArrayLiteral("\x1b[20~");
    case Qt::Key_F10:
        return QByteArrayLiteral("\x1b[21~");
    case Qt::Key_F11:
        return QByteArrayLiteral("\x1b[23~");
    case Qt::Key_F12:
        return QByteArrayLiteral("\x1b[24~");
    default:
        return {};
    }
}

} // namespace

TerminalWidget::TerminalWidget(QWidget *parent)
    : QWidget(parent)
    , m_screen(100, 30)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setCursor(Qt::IBeamCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_screen.setMaxScrollback(5000);
    updateMetrics();

    m_blinkTimer = new QTimer(this);
    m_blinkTimer->setInterval(kBlinkIntervalMs);
    connect(m_blinkTimer, &QTimer::timeout, this, [this] {
        m_cursorOn = !m_cursorOn;
        update();
    });

    connect(&ui::Theme::instance(), &ui::Theme::changed, this, [this] {
        updateMetrics();
        update();
    });
}

void TerminalWidget::updateMetrics()
{
    m_baseFont = ui::Theme::instance().monoFont(m_fontSize, m_fontFamily);
    m_boldFont = m_baseFont;
    m_boldFont.setBold(true);
    m_italicFont = m_baseFont;
    m_italicFont.setItalic(true);
    m_boldItalicFont = m_boldFont;
    m_boldItalicFont.setItalic(true);

    const QFontMetricsF metrics(m_baseFont);
    m_cellWidth = qMax(4, qCeil(metrics.horizontalAdvance(QStringLiteral("M"))));
    m_cellHeight = qMax(10, qCeil(metrics.height()));
    m_ascent = qCeil(metrics.ascent());
    setFont(m_baseFont);
}

void TerminalWidget::recomputeGrid()
{
    const int availableWidth = width() - m_paddingLeft - m_paddingRight;
    const int availableHeight = height() - m_paddingTop - m_paddingBottom;
    if (availableWidth < 20 || availableHeight < 20)
        return;

    const int columns = qBound(2, availableWidth / qMax(1, m_cellWidth), 1000);
    const int rows = qBound(2, availableHeight / qMax(1, m_cellHeight), 400);
    if (columns == m_screen.columns() && rows == m_screen.rows())
        return;

    m_screen.resize(columns, rows);
    m_scrollOffset = qBound(0, m_scrollOffset, m_screen.scrollbackLines());
    emit sizeChanged(columns, rows);
    update();
}

void TerminalWidget::feed(const QByteArray &data)
{
    if (data.isEmpty())
        return;

    const int previousTotal = m_screen.totalLines();
    m_screen.feed(data);

    QString title;
    if (m_screen.takeTitleChanged(&title)) {
        m_lastTitle = title;
        emit titleChanged(title);
    }
    if (m_screen.takeBell())
        emit bell();
    const QByteArray response = m_screen.takeResponse();
    if (!response.isEmpty())
        emit responseData(response);

    if (m_screen.isAltScreen()) {
        m_scrollOffset = 0;
    } else if (m_scrollOffset > 0) {
        const int delta = m_screen.totalLines() - previousTotal;
        m_scrollOffset = qBound(0, m_scrollOffset + qMax(0, delta), m_screen.scrollbackLines());
    }
    update();
}

void TerminalWidget::setFontFamily(const QString &family)
{
    if (m_fontFamily == family)
        return;
    m_fontFamily = family;
    updateMetrics();
    recomputeGrid();
    update();
}

void TerminalWidget::setFontSize(int pointSize)
{
    pointSize = qBound(7, pointSize, 32);
    if (pointSize == m_fontSize)
        return;
    m_fontSize = pointSize;
    updateMetrics();
    recomputeGrid();
    update();
}

void TerminalWidget::setScrollbackLimit(int lines)
{
    m_screen.setMaxScrollback(lines);
    m_scrollOffset = qBound(0, m_scrollOffset, m_screen.scrollbackLines());
    update();
}

void TerminalWidget::setCopyOnSelect(bool enabled)
{
    m_copyOnSelect = enabled;
}

void TerminalWidget::resetScreen()
{
    m_screen.reset();
    m_scrollOffset = 0;
    m_hasSelection = false;
    m_selecting = false;
    m_lastTitle.clear();
    recomputeGrid();
    update();
}

void TerminalWidget::sendText(const QByteArray &data)
{
    if (data.isEmpty())
        return;
    scrollToBottom();
    emit inputData(data);
}

void TerminalWidget::scrollToBottom()
{
    if (m_scrollOffset == 0)
        return;
    m_scrollOffset = 0;
    update();
}

void TerminalWidget::applyScroll(int deltaLines)
{
    const int maximum = m_screen.scrollbackLines();
    if (maximum <= 0) {
        m_scrollOffset = 0;
        return;
    }
    const int updated = qBound(0, m_scrollOffset + deltaLines, maximum);
    if (updated == m_scrollOffset)
        return;
    m_scrollOffset = updated;
    update();
}

void TerminalWidget::copySelection()
{
    if (!m_hasSelection)
        return;
    const QString text = selectedText();
    if (text.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(text);
}

void TerminalWidget::pasteClipboard()
{
    QString text = QGuiApplication::clipboard()->text();
    if (text.isEmpty())
        return;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    text.replace(QLatin1Char('\n'), QLatin1Char('\r'));
    QByteArray data = text.toUtf8();
    if (m_screen.bracketedPaste())
        data = QByteArrayLiteral("\x1b[200~") + data + QByteArrayLiteral("\x1b[201~");
    sendText(data);
}

void TerminalWidget::selectAll()
{
    m_selectionStart = {0, 0};
    const int lastRow = qMax(0, m_screen.totalLines() - 1);
    m_selectionEnd = {lastRow, qMax(0, m_screen.columns() - 1)};
    m_hasSelection = true;
    m_selecting = false;
    update();
}

void TerminalWidget::clearScrollback()
{
    m_screen.clearScrollback();
    m_scrollOffset = 0;
    update();
}

QSize TerminalWidget::sizeHint() const
{
    return QSize(900, 520);
}

QSize TerminalWidget::minimumSizeHint() const
{
    return QSize(320, 160);
}

QString TerminalWidget::selectedText() const
{
    if (!m_hasSelection)
        return {};
    SelectionPoint start = m_selectionStart;
    SelectionPoint end = m_selectionEnd;
    if (end.row < start.row || (end.row == start.row && end.col < start.col))
        std::swap(start, end);
    return m_screen.textRange(start.row, start.col, end.row, end.col);
}

bool TerminalWidget::hasSelection() const
{
    return m_hasSelection && (m_selectionStart.row != m_selectionEnd.row
                              || m_selectionStart.col != m_selectionEnd.col);
}

QColor TerminalWidget::resolveColor(quint32 color, bool foreground) const
{
    const ui::Palette &palette = ui::Theme::instance().palette();
    if (color == DefaultColor)
        return foreground ? palette.terminalForeground : palette.terminalBackground;
    if (color & RgbColorFlag) {
        return QColor(static_cast<int>((color >> 16) & 0xFF), static_cast<int>((color >> 8) & 0xFF),
                      static_cast<int>(color & 0xFF));
    }
    const int index = static_cast<int>(color & PaletteColorMask);
    if (index < 16)
        return palette.ansi.at(index);
    if (index < 232) {
        static const int levels[] = {0, 95, 135, 175, 215, 255};
        const int n = index - 16;
        return QColor(levels[n / 36], levels[(n / 6) % 6], levels[n % 6]);
    }
    const int gray = 8 + (index - 232) * 10;
    return QColor(gray, gray, gray);
}

QColor TerminalWidget::blended(const QColor &color, int alpha) const
{
    QColor copy = color;
    copy.setAlpha(alpha);
    return copy;
}

int TerminalWidget::selectionRowAt(int y) const
{
    const int contentHeight = m_screen.rows() * m_cellHeight;
    const int localY = qBound(0, y - m_paddingTop, qMax(0, contentHeight - 1));
    const int screenRow = localY / qMax(1, m_cellHeight);
    const int absolute = m_screen.screenTop() - m_scrollOffset + screenRow;
    return qBound(0, absolute, qMax(0, m_screen.totalLines() - 1));
}

int TerminalWidget::selectionColumnAt(int x) const
{
    const int contentWidth = m_screen.columns() * m_cellWidth;
    const int localX = qBound(0, x - m_paddingLeft, qMax(0, contentWidth - 1));
    return qBound(0, localX / qMax(1, m_cellWidth), qMax(0, m_screen.columns() - 1));
}

void TerminalWidget::moveSelectionTo(const QPoint &position)
{
    if (!m_selecting)
        return;
    if (position.y() < m_paddingTop) {
        applyScroll(1);
        m_selectionEnd = {m_screen.screenTop() - m_scrollOffset, 0};
        m_hasSelection = true;
        update();
        return;
    }
    if (position.y() > height() - m_paddingBottom) {
        applyScroll(-1);
        m_selectionEnd = {m_screen.screenTop() - m_scrollOffset + m_screen.rows() - 1,
                          m_screen.columns() - 1};
        m_hasSelection = true;
        update();
        return;
    }
    m_selectionEnd = {selectionRowAt(position.y()), selectionColumnAt(position.x())};
    m_hasSelection = m_selectionStart.row != m_selectionEnd.row
                     || m_selectionStart.col != m_selectionEnd.col;
    update();
}

void TerminalWidget::paintEvent(QPaintEvent *)
{
    const ui::Palette &palette = ui::Theme::instance().palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(rect(), palette.terminalBackground);

    painter.setFont(m_baseFont);
    painter.setClipRect(QRect(m_paddingLeft, m_paddingTop,
                              m_screen.columns() * m_cellWidth, m_screen.rows() * m_cellHeight));

    const int top = m_screen.screenTop() - m_scrollOffset;
    const int rows = m_screen.rows();
    const int columns = m_screen.columns();
    const QRectF contentRect(m_paddingLeft, m_paddingTop, columns * m_cellWidth, rows * m_cellHeight);

    SelectionPoint selStart = m_selectionStart;
    SelectionPoint selEnd = m_selectionEnd;
    if (selEnd.row < selStart.row || (selEnd.row == selStart.row && selEnd.col < selStart.col))
        std::swap(selStart, selEnd);

    for (int row = 0; row < rows; ++row) {
        const int absoluteRow = top + row;
        if (absoluteRow < 0 || absoluteRow >= m_screen.totalLines())
            continue;
        const qreal y = m_paddingTop + row * m_cellHeight;

        for (int col = 0; col < columns; ++col) {
            const Cell &cell = m_screen.cellAt(absoluteRow, col);
            if (cell.width == 0)
                continue;
            QColor background = resolveColor(cell.bg, false);
            QColor foreground = resolveColor(cell.fg, true);
            if (cell.flags & Inverse)
                std::swap(background, foreground);
            if (cell.flags & Hidden)
                foreground = background;
            const int width = qMax(1, static_cast<int>(cell.width));
            if (background != palette.terminalBackground) {
                painter.fillRect(QRectF(m_paddingLeft + col * m_cellWidth, y,
                                        width * m_cellWidth, m_cellHeight),
                                 background);
            }
            if (col + width - 1 < columns && width == 2)
                ++col;
        }

        if (m_hasSelection && absoluteRow >= selStart.row && absoluteRow <= selEnd.row) {
            const int from = absoluteRow == selStart.row ? selStart.col : 0;
            const int to = absoluteRow == selEnd.row ? selEnd.col : columns - 1;
            if (to >= from) {
                const QRectF selectionRect(m_paddingLeft + from * m_cellWidth, y,
                                           (to - from + 1) * m_cellWidth, m_cellHeight);
                painter.fillRect(selectionRect.intersected(contentRect),
                                 blended(palette.terminalSelection, 170));
            }
        }

        int currentStyle = -1;
        for (int col = 0; col < columns; ++col) {
            const Cell &cell = m_screen.cellAt(absoluteRow, col);
            if (cell.width == 0 || cell.ch == 0)
                continue;
            const int width = qMax(1, static_cast<int>(cell.width));
            QColor foreground = resolveColor(cell.fg, true);
            QColor background = resolveColor(cell.bg, false);
            if (cell.flags & Inverse)
                std::swap(foreground, background);
            if (cell.flags & Dim)
                foreground = blended(foreground, 155);
            if (cell.flags & Hidden)
                foreground = background;

            int style = 0;
            if (cell.flags & Bold)
                style |= 1;
            if (cell.flags & Italic)
                style |= 2;
            if (style != currentStyle) {
                switch (style) {
                case 1:
                    painter.setFont(m_boldFont);
                    break;
                case 2:
                    painter.setFont(m_italicFont);
                    break;
                case 3:
                    painter.setFont(m_boldItalicFont);
                    break;
                default:
                    painter.setFont(m_baseFont);
                    break;
                }
                currentStyle = style;
            }

            painter.setPen(foreground);
            const qreal x = m_paddingLeft + col * m_cellWidth;
            const QString character = QString::fromUcs4(&cell.ch, 1);
            painter.drawText(QPointF(x, y + m_ascent), character);

            if (cell.flags & Underline) {
                painter.setPen(QPen(foreground, 1.0));
                painter.drawLine(QPointF(x, y + m_cellHeight - 2.0),
                                 QPointF(x + width * m_cellWidth, y + m_cellHeight - 2.0));
            }
            if (cell.flags & Strike) {
                painter.setPen(QPen(foreground, 1.0));
                painter.drawLine(QPointF(x, y + m_cellHeight * 0.55),
                                 QPointF(x + width * m_cellWidth, y + m_cellHeight * 0.55));
            }
            if (width == 2)
                ++col;
        }
    }

    if (m_scrollOffset == 0 && m_screen.cursorVisible()) {
        const int cursorRow = m_screen.absoluteRow(m_screen.cursorRow());
        if (cursorRow >= top && cursorRow < top + rows) {
            const int screenRow = cursorRow - top;
            const int col = qBound(0, m_screen.cursorCol(), columns - 1);
            const Cell &cell = m_screen.cellAt(cursorRow, col);
            const int width = qMax(1, static_cast<int>(cell.width));
            const qreal x = m_paddingLeft + col * m_cellWidth;
            const qreal y = m_paddingTop + screenRow * m_cellHeight;
            const QRectF cursorRect(x, y, width * m_cellWidth, m_cellHeight);

            if (m_focused && m_cursorOn) {
                painter.fillRect(cursorRect, blended(palette.terminalCursor, 235));
                if (cell.ch != 0 && cell.width != 0) {
                    QFont cursorFont = m_baseFont;
                    if (cell.flags & Bold)
                        cursorFont.setBold(true);
                    if (cell.flags & Italic)
                        cursorFont.setItalic(true);
                    painter.setFont(cursorFont);
                    painter.setPen(palette.terminalBackground);
                    painter.drawText(QPointF(x, y + m_ascent), QString::fromUcs4(&cell.ch, 1));
                }
            } else if (!m_focused) {
                painter.setPen(QPen(blended(palette.terminalForeground, 150), 1.0));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(cursorRect.adjusted(0.5, 0.5, -0.5, -0.5));
            }
        }
    }

    painter.setClipping(false);

    if (m_scrollOffset > 0) {
        const QString badgeText = QStringLiteral("回滚 %1 行").arg(m_scrollOffset);
        QFont badgeFont = ui::Theme::instance().uiFont(8.5);
        const QFontMetrics badgeMetrics(badgeFont);
        const int badgeWidth = badgeMetrics.horizontalAdvance(badgeText) + 18;
        const QRectF badgeRect(width() - badgeWidth - 16, m_paddingTop + 4, badgeWidth, 22);
        painter.setFont(badgeFont);
        painter.setPen(QPen(blended(palette.borderStrong, 160), 1));
        painter.setBrush(blended(palette.elevated, 235));
        painter.drawRoundedRect(badgeRect, 7, 7);
        painter.setPen(palette.textMuted);
        painter.drawText(badgeRect, Qt::AlignCenter, badgeText);
    }

    if (m_screen.scrollbackLines() > 0) {
        const qreal trackTop = m_paddingTop + 4;
        const qreal trackHeight = rows * m_cellHeight - 8;
        const qreal ratio = static_cast<qreal>(rows) / static_cast<qreal>(m_screen.totalLines());
        const qreal thumbHeight = qMax<qreal>(24, trackHeight * ratio);
        const qreal scrollRange = qMax(1, m_screen.totalLines() - rows);
        const qreal position = static_cast<qreal>(m_screen.screenTop() - m_scrollOffset) / scrollRange;
        const qreal thumbTop = trackTop + position * (trackHeight - thumbHeight);
        painter.setPen(Qt::NoPen);
        painter.setBrush(blended(palette.textMuted, 42));
        painter.drawRoundedRect(QRectF(width() - 7, trackTop, 3, trackHeight), 1.5, 1.5);
        painter.setBrush(blended(palette.textMuted, 130));
        painter.drawRoundedRect(QRectF(width() - 7, thumbTop, 3, thumbHeight), 1.5, 1.5);
    }
}

void TerminalWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    recomputeGrid();
}

void TerminalWidget::keyPressEvent(QKeyEvent *event)
{
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    const bool control = modifiers.testFlag(Qt::ControlModifier);
    const bool shift = modifiers.testFlag(Qt::ShiftModifier);
    const bool alt = modifiers.testFlag(Qt::AltModifier);
    const int key = event->key();

    if ((control && shift && key == Qt::Key_C) || (control && key == Qt::Key_Insert)) {
        copySelection();
        event->accept();
        return;
    }
    if ((control && shift && key == Qt::Key_V) || (shift && key == Qt::Key_Insert)) {
        pasteClipboard();
        event->accept();
        return;
    }
    if (control && (key == Qt::Key_Plus || key == Qt::Key_Equal)) {
        emit fontSizeChangeRequested(qMin(32, m_fontSize + 1));
        event->accept();
        return;
    }
    if (control && key == Qt::Key_Minus) {
        emit fontSizeChangeRequested(qMax(7, m_fontSize - 1));
        event->accept();
        return;
    }
    if (control && key == Qt::Key_0) {
        emit fontSizeChangeRequested(11);
        event->accept();
        return;
    }

    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        sendText(QByteArrayLiteral("\r"));
        event->accept();
        return;
    }
    if (key == Qt::Key_Backspace) {
        sendText(QByteArrayLiteral("\x7f"));
        event->accept();
        return;
    }
    if (key == Qt::Key_Tab) {
        sendText(QByteArrayLiteral("\t"));
        event->accept();
        return;
    }
    if (key == Qt::Key_Escape) {
        sendText(QByteArrayLiteral("\x1b"));
        event->accept();
        return;
    }

    if (control && !shift && !alt && key >= Qt::Key_A && key <= Qt::Key_Z) {
        const char controlCode = static_cast<char>(key - Qt::Key_A + 1);
        sendText(QByteArray(1, controlCode));
        event->accept();
        return;
    }
    if (control && !shift && !alt && key == Qt::Key_Space) {
        sendText(QByteArray(1, '\0'));
        event->accept();
        return;
    }

    const QByteArray sequence = keySequence(key, m_screen.applicationCursorKeys());
    if (!sequence.isEmpty()) {
        QByteArray result = sequence;
        if (shift && key >= Qt::Key_F1 && key <= Qt::Key_F4) {
            // Shift+F1..F4 are not standard; pass the base sequence.
        }
        sendText(result);
        event->accept();
        return;
    }

    QString text = event->text();
    if (text.isEmpty())
        return;
    if (control && !alt) {
        if (text.size() == 1 && static_cast<unsigned char>(text.at(0).toLatin1()) < 0x20) {
            sendText(text.toUtf8());
            event->accept();
        }
        return;
    }

    QByteArray payload = text.toUtf8();
    if (alt && !payload.isEmpty())
        payload.prepend('\x1b');
    sendText(payload);
    event->accept();
}

void TerminalWidget::inputMethodEvent(QInputMethodEvent *event)
{
    if (!event->commitString().isEmpty()) {
        sendText(event->commitString().toUtf8());
        event->accept();
        return;
    }
    event->ignore();
}

QVariant TerminalWidget::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (query == Qt::ImEnabled)
        return true;
    if (query == Qt::ImCursorRectangle) {
        const int col = qBound(0, m_screen.cursorCol(), m_screen.columns() - 1);
        const int row = qBound(0, m_screen.cursorRow(), m_screen.rows() - 1);
        return QRectF(m_paddingLeft + col * m_cellWidth, m_paddingTop + row * m_cellHeight,
                      m_cellWidth, m_cellHeight);
    }
    return QWidget::inputMethodQuery(query);
}

void TerminalWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setFocus(Qt::MouseFocusReason);
        const SelectionPoint point{selectionRowAt(event->position().toPoint().y()),
                                   selectionColumnAt(event->position().toPoint().x())};
        if (event->modifiers().testFlag(Qt::ShiftModifier) && m_hasSelection) {
            m_selectionEnd = point;
        } else {
            m_selectionStart = point;
            m_selectionEnd = point;
            m_hasSelection = false;
        }
        m_selecting = true;
        m_mouseDragged = false;
        update();
    }
    QWidget::mousePressEvent(event);
}

void TerminalWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_selecting && event->buttons().testFlag(Qt::LeftButton)) {
        m_mouseDragged = true;
        moveSelectionTo(event->position().toPoint());
    }
    QWidget::mouseMoveEvent(event);
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        m_hasSelection = hasSelection();
        if (m_hasSelection && m_copyOnSelect && m_mouseDragged)
            copySelection();
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void TerminalWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    const int row = selectionRowAt(event->position().toPoint().y());
    int col = selectionColumnAt(event->position().toPoint().x());
    const int columns = m_screen.columns();

    const Cell &cell = m_screen.cellAt(row, col);
    const QChar character = cell.ch != 0 ? QChar(static_cast<char16_t>(cell.ch)) : QChar(QLatin1Char(' '));
    const bool wordChar = character.isLetterOrNumber() || character == QLatin1Char('_')
                          || character == QLatin1Char('-') || character == QLatin1Char('.')
                          || character == QLatin1Char('/');
    int start = col;
    int end = col;
    if (wordChar) {
        while (start > 0) {
            const Cell &previous = m_screen.cellAt(row, start - 1);
            const QChar prevChar = previous.ch != 0 ? QChar(static_cast<char16_t>(previous.ch)) : QChar();
            if (!(prevChar.isLetterOrNumber() || prevChar == QLatin1Char('_') || prevChar == QLatin1Char('-')
                  || prevChar == QLatin1Char('.') || prevChar == QLatin1Char('/'))) {
                break;
            }
            --start;
        }
        while (end + 1 < columns) {
            const Cell &next = m_screen.cellAt(row, end + 1);
            const QChar nextChar = next.ch != 0 ? QChar(static_cast<char16_t>(next.ch)) : QChar();
            if (!(nextChar.isLetterOrNumber() || nextChar == QLatin1Char('_') || nextChar == QLatin1Char('-')
                  || nextChar == QLatin1Char('.') || nextChar == QLatin1Char('/'))) {
                break;
            }
            ++end;
        }
    }
    m_selectionStart = {row, start};
    m_selectionEnd = {row, end};
    m_hasSelection = true;
    m_selecting = false;
    if (m_copyOnSelect)
        copySelection();
    update();
    event->accept();
}

void TerminalWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        emit fontSizeChangeRequested(event->angleDelta().y() > 0 ? qMin(32, m_fontSize + 1)
                                                                : qMax(7, m_fontSize - 1));
        event->accept();
        return;
    }

    int lines = 0;
    if (!event->angleDelta().isNull())
        lines = event->angleDelta().y() / 120 * 3;
    else if (!event->pixelDelta().isNull())
        lines = event->pixelDelta().y() / qMax(1, m_cellHeight);
    if (lines == 0) {
        event->ignore();
        return;
    }
    applyScroll(lines);
    event->accept();
}

void TerminalWidget::focusInEvent(QFocusEvent *event)
{
    m_focused = true;
    m_cursorOn = true;
    m_blinkTimer->start();
    update();
    QWidget::focusInEvent(event);
}

void TerminalWidget::focusOutEvent(QFocusEvent *event)
{
    m_focused = false;
    m_blinkTimer->stop();
    m_cursorOn = true;
    update();
    QWidget::focusOutEvent(event);
}

void TerminalWidget::contextMenuEvent(QContextMenuEvent *event)
{
    showTerminalMenu(event->globalPos());
    event->accept();
}

void TerminalWidget::showTerminalMenu(const QPoint &globalPosition)
{
    QMenu menu(this);
    QAction *copyAction = menu.addAction(QStringLiteral("复制"));
    copyAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
    copyAction->setEnabled(hasSelection());
    QAction *pasteAction = menu.addAction(QStringLiteral("粘贴"));
    pasteAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+V")));
    QAction *selectAllAction = menu.addAction(QStringLiteral("全选"));
    menu.addSeparator();
    QAction *clearAction = menu.addAction(QStringLiteral("清空回滚缓冲"));
    clearAction->setEnabled(m_screen.scrollbackLines() > 0);
    menu.addSeparator();
    QAction *zoomInAction = menu.addAction(QStringLiteral("放大字体"));
    QAction *zoomOutAction = menu.addAction(QStringLiteral("缩小字体"));
    QAction *resetZoomAction = menu.addAction(QStringLiteral("重置字号"));

    QAction *chosen = menu.exec(globalPosition);
    if (!chosen)
        return;
    if (chosen == copyAction)
        copySelection();
    else if (chosen == pasteAction)
        pasteClipboard();
    else if (chosen == selectAllAction)
        selectAll();
    else if (chosen == clearAction)
        clearScrollback();
    else if (chosen == zoomInAction)
        emit fontSizeChangeRequested(qMin(32, m_fontSize + 1));
    else if (chosen == zoomOutAction)
        emit fontSizeChangeRequested(qMax(7, m_fontSize - 1));
    else if (chosen == resetZoomAction)
        emit fontSizeChangeRequested(11);
}

void TerminalWidget::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
}

} // namespace nyan::term
