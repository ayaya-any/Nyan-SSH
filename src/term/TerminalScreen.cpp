#include "term/TerminalScreen.h"

#include <QChar>

#include <algorithm>
#include <cmath>

namespace nyan::term {
namespace {

constexpr int kTabWidth = 8;

bool isCombining(char32_t cp)
{
    return (cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x1AB0 && cp <= 0x1AFF)
           || (cp >= 0x1DC0 && cp <= 0x1DFF) || (cp >= 0x20D0 && cp <= 0x20FF)
           || (cp >= 0xFE20 && cp <= 0xFE2F) || (cp >= 0x1F3FB && cp <= 0x1F3FF)
           || cp == 0xFE0E || cp == 0xFE0F || cp == 0x200D || cp == 0x20E3;
}

bool isWide(char32_t cp)
{
    if (cp < 0x1100)
        return false;
    return (cp <= 0x115F) || cp == 0x2329 || cp == 0x232A || (cp >= 0x2E80 && cp <= 0x303E)
           || (cp >= 0x3041 && cp <= 0x33FF) || (cp >= 0x3400 && cp <= 0x4DBF)
           || (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0xA000 && cp <= 0xA4CF)
           || (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF)
           || (cp >= 0xFE10 && cp <= 0xFE19) || (cp >= 0xFE30 && cp <= 0xFE6F)
           || (cp >= 0xFF00 && cp <= 0xFF60) || (cp >= 0xFFE0 && cp <= 0xFFE6)
           || (cp >= 0x1F300 && cp <= 0x1FAFF) || (cp >= 0x20000 && cp <= 0x3FFFD);
}

QString stringForCodepoint(char32_t cp)
{
    if (cp > 0x10FFFF)
        return QString(QChar(0xFFFD));
    return QString::fromUcs4(&cp, 1);
}

} // namespace

TerminalScreen::TerminalScreen(int columns, int rows)
    : m_columns(qBound(2, columns, 1000))
    , m_rows(qBound(2, rows, 400))
{
    reset();
}

void TerminalScreen::reset()
{
    m_primary.clear();
    for (int i = 0; i < m_rows; ++i)
        m_primary.push_back(blankLine());

    m_alt.clear();
    m_alt.reserve(m_rows);
    for (int i = 0; i < m_rows; ++i)
        m_alt.append(blankLine());

    m_altActive = false;
    m_cursor = CursorState{};
    m_savedCursor = CursorState{};
    m_cursorVisible = true;
    m_cursorBlink = true;
    m_autoWrap = true;
    m_originMode = false;
    m_insertMode = false;
    m_newlineMode = false;
    m_applicationCursorKeys = false;
    m_bracketedPaste = false;
    m_scrollTop = 0;
    m_scrollBottom = m_rows - 1;

    m_tabStops.clear();
    for (int c = kTabWidth; c < m_columns; c += kTabWidth)
        m_tabStops.insert(c);

    m_parseState = ParseState::Ground;
    m_csiParams.clear();
    m_csiCurrent = 0;
    m_csiHasParam = false;
    m_csiPrefix = QChar();
    m_csiIntermediate = QChar();
    m_oscBuffer.clear();
    m_utf8Pending.clear();
    m_lastCharValid = false;
    m_response.clear();
    m_bell = false;
    m_title.clear();
    m_titleDirty = true;
    m_dirty = true;
}

void TerminalScreen::resize(int columns, int rows)
{
    columns = qBound(2, columns, 1000);
    rows = qBound(2, rows, 400);
    if (columns == m_columns && rows == m_rows)
        return;

    const int oldRows = m_rows;
    if (columns != m_columns) {
        const Cell blank = blankCell();
        for (Line &line : m_primary) {
            line.resize(columns);
            for (int c = 0; c < columns; ++c) {
                if (line[c].width == 0)
                    line[c] = blank;
            }
            for (int c = 0; c < columns; ++c) {
                if (line[c].width == 2 && c == columns - 1)
                    line[c] = blank;
            }
        }
        for (Line &line : m_alt) {
            line.resize(columns);
            for (int c = 0; c < columns; ++c) {
                if (line[c].width == 0 || (line[c].width == 2 && c == columns - 1))
                    line[c] = blank;
            }
        }
        m_columns = columns;
        m_tabStops.clear();
        for (int c = kTabWidth; c < m_columns; c += kTabWidth)
            m_tabStops.insert(c);
    }

    if (rows != oldRows) {
        if (rows > oldRows) {
            for (int i = 0; i < rows - oldRows; ++i)
                m_primary.push_back(blankLine());
            for (int i = 0; i < rows - oldRows; ++i)
                m_alt.append(blankLine());
        } else {
            for (int i = 0; i < oldRows - rows; ++i) {
                if (!m_primary.empty())
                    m_primary.pop_back();
            }
            while (m_alt.size() > rows)
                m_alt.removeLast();
        }
        m_rows = rows;
        while (static_cast<int>(m_primary.size()) < m_rows)
            m_primary.push_back(blankLine());
    }

    m_scrollTop = 0;
    m_scrollBottom = m_rows - 1;
    m_cursor.row = qBound(0, m_cursor.row, m_rows - 1);
    m_cursor.col = qBound(0, m_cursor.col, m_columns - 1);
    m_cursor.wrapPending = false;
    trimScrollback();
    m_dirty = true;
}

void TerminalScreen::setMaxScrollback(int lines)
{
    m_maxScrollback = qMax(0, lines);
    trimScrollback();
}

int TerminalScreen::totalLines() const
{
    if (m_altActive)
        return m_rows;
    return qMax(m_rows, static_cast<int>(m_primary.size()));
}

int TerminalScreen::screenTop() const
{
    if (m_altActive)
        return 0;
    return qMax(0, static_cast<int>(m_primary.size()) - m_rows);
}

int TerminalScreen::scrollbackLines() const
{
    return qMax(0, totalLines() - m_rows);
}

int TerminalScreen::primaryTop() const
{
    return qMax(0, static_cast<int>(m_primary.size()) - m_rows);
}

int TerminalScreen::activeTop() const
{
    return m_altActive ? 0 : primaryTop();
}

TerminalScreen::Line &TerminalScreen::lineRef(int absoluteRow)
{
    if (m_altActive) {
        const int index = qBound(0, absoluteRow, m_alt.size() - 1);
        return m_alt[index];
    }
    const int index = qBound(0, absoluteRow, static_cast<int>(m_primary.size()) - 1);
    return m_primary[static_cast<size_t>(index)];
}

const TerminalScreen::Line &TerminalScreen::lineRef(int absoluteRow) const
{
    if (m_altActive) {
        const int index = qBound(0, absoluteRow, m_alt.size() - 1);
        return m_alt[index];
    }
    const int index = qBound(0, absoluteRow, static_cast<int>(m_primary.size()) - 1);
    return m_primary[static_cast<size_t>(index)];
}

TerminalScreen::Line &TerminalScreen::screenLineRef(int screenRow)
{
    return lineRef(activeTop() + qBound(0, screenRow, m_rows - 1));
}

void TerminalScreen::ensurePrimaryLines()
{
    while (static_cast<int>(m_primary.size()) < m_rows)
        m_primary.push_back(blankLine());
    trimScrollback();
}

void TerminalScreen::trimScrollback()
{
    const int maximum = m_rows + qMax(0, m_maxScrollback);
    while (static_cast<int>(m_primary.size()) > maximum)
        m_primary.pop_front();
}

Cell TerminalScreen::blankCell(quint32 background) const
{
    Cell cell;
    cell.ch = U' ';
    cell.fg = DefaultColor;
    cell.bg = background;
    cell.flags = 0;
    cell.width = 1;
    return cell;
}

TerminalScreen::Line TerminalScreen::blankLine(quint32 background) const
{
    Line line;
    line.resize(m_columns);
    const Cell blank = blankCell(background);
    for (int i = 0; i < m_columns; ++i)
        line[i] = blank;
    return line;
}

const TerminalScreen::Line &TerminalScreen::lineAt(int absoluteRow) const
{
    static const Line empty;
    if (absoluteRow < 0 || absoluteRow >= totalLines())
        return empty;
    return lineRef(absoluteRow);
}

const Cell &TerminalScreen::cellAt(int absoluteRow, int column) const
{
    static const Cell blank;
    const Line &line = lineAt(absoluteRow);
    if (column < 0 || column >= line.size())
        return blank;
    return line[column];
}

int TerminalScreen::absoluteRow(int screenRow) const
{
    return activeTop() + qBound(0, screenRow, m_rows - 1);
}

void TerminalScreen::setCursor(int row, int col)
{
    m_cursor.row = qBound(0, row, m_rows - 1);
    m_cursor.col = qBound(0, col, m_columns - 1);
    m_cursor.wrapPending = false;
}

void TerminalScreen::moveCursor(int deltaRow, int deltaCol)
{
    setCursor(m_cursor.row + deltaRow, m_cursor.col + deltaCol);
}

void TerminalScreen::index()
{
    m_cursor.wrapPending = false;
    if (m_cursor.row == m_scrollBottom) {
        scrollUpRegion(m_scrollTop, m_scrollBottom, 1);
    } else if (m_cursor.row < m_rows - 1) {
        ++m_cursor.row;
    }
}

void TerminalScreen::reverseIndex()
{
    m_cursor.wrapPending = false;
    if (m_cursor.row == m_scrollTop) {
        scrollDownRegion(m_scrollTop, m_scrollBottom, 1);
    } else if (m_cursor.row > 0) {
        --m_cursor.row;
    }
}

void TerminalScreen::scrollUpRegion(int top, int bottom, int count)
{
    top = qBound(0, top, m_rows - 1);
    bottom = qBound(top, bottom, m_rows - 1);
    count = qBound(0, count, bottom - top + 1);
    if (count == 0)
        return;

    if (m_altActive) {
        for (int i = 0; i < count; ++i) {
            m_alt.removeAt(top);
            m_alt.insert(bottom, blankLine());
        }
    } else if (top == 0 && bottom == m_rows - 1) {
        for (int i = 0; i < count; ++i) {
            m_primary.push_back(blankLine());
            trimScrollback();
        }
    } else {
        const int base = primaryTop();
        for (int i = 0; i < count; ++i) {
            m_primary.erase(m_primary.begin() + base + top);
            m_primary.insert(m_primary.begin() + base + bottom, blankLine());
        }
    }
    m_dirty = true;
}

void TerminalScreen::scrollDownRegion(int top, int bottom, int count)
{
    top = qBound(0, top, m_rows - 1);
    bottom = qBound(top, bottom, m_rows - 1);
    count = qBound(0, count, bottom - top + 1);
    if (count == 0)
        return;

    if (m_altActive) {
        for (int i = 0; i < count; ++i) {
            m_alt.removeAt(bottom);
            m_alt.insert(top, blankLine());
        }
    } else {
        const int base = primaryTop();
        for (int i = 0; i < count; ++i) {
            m_primary.erase(m_primary.begin() + base + bottom);
            m_primary.insert(m_primary.begin() + base + top, blankLine());
        }
    }
    m_dirty = true;
}

void TerminalScreen::insertLines(int count)
{
    count = qMax(1, count);
    if (m_cursor.row < m_scrollTop || m_cursor.row > m_scrollBottom)
        return;
    count = qMin(count, m_scrollBottom - m_cursor.row + 1);
    if (m_altActive) {
        for (int i = 0; i < count; ++i) {
            m_alt.removeAt(m_scrollBottom);
            m_alt.insert(m_cursor.row, blankLine());
        }
    } else {
        const int base = primaryTop();
        for (int i = 0; i < count; ++i) {
            m_primary.erase(m_primary.begin() + base + m_scrollBottom);
            m_primary.insert(m_primary.begin() + base + m_cursor.row, blankLine());
        }
    }
    m_dirty = true;
}

void TerminalScreen::deleteLines(int count)
{
    count = qMax(1, count);
    if (m_cursor.row < m_scrollTop || m_cursor.row > m_scrollBottom)
        return;
    count = qMin(count, m_scrollBottom - m_cursor.row + 1);
    if (m_altActive) {
        for (int i = 0; i < count; ++i) {
            m_alt.removeAt(m_cursor.row);
            m_alt.insert(m_scrollBottom, blankLine());
        }
    } else {
        const int base = primaryTop();
        for (int i = 0; i < count; ++i) {
            m_primary.erase(m_primary.begin() + base + m_cursor.row);
            m_primary.insert(m_primary.begin() + base + m_scrollBottom, blankLine());
        }
    }
    m_dirty = true;
}

void TerminalScreen::insertChars(int count)
{
    count = qMax(1, count);
    Line &line = screenLineRef(m_cursor.row);
    const int safe = qMin(count, m_columns - m_cursor.col);
    if (safe <= 0)
        return;
    line.insert(m_cursor.col, safe, blankCell(m_cursor.bg));
    line.resize(m_columns);
    m_dirty = true;
}

void TerminalScreen::deleteChars(int count)
{
    count = qMax(1, count);
    Line &line = screenLineRef(m_cursor.row);
    const int safe = qMin(count, m_columns - m_cursor.col);
    if (safe <= 0)
        return;
    line.remove(m_cursor.col, safe);
    while (line.size() < m_columns)
        line.append(blankCell(m_cursor.bg));
    m_dirty = true;
}

void TerminalScreen::eraseChars(int count)
{
    count = qMax(1, count);
    Line &line = screenLineRef(m_cursor.row);
    const int end = qMin(m_columns, m_cursor.col + count);
    for (int c = m_cursor.col; c < end; ++c)
        line[c] = blankCell(m_cursor.bg);
    m_dirty = true;
}

void TerminalScreen::eraseInLine(int mode)
{
    Line &line = screenLineRef(m_cursor.row);
    int start = 0;
    int end = m_columns;
    switch (mode) {
    case 0:
        start = m_cursor.col;
        break;
    case 1:
        end = m_cursor.col + 1;
        break;
    case 2:
        start = 0;
        end = m_columns;
        break;
    default:
        return;
    }
    for (int c = start; c < end && c < m_columns; ++c)
        line[c] = blankCell(m_cursor.bg);
    m_dirty = true;
}

void TerminalScreen::eraseInDisplay(int mode)
{
    if (mode == 3) {
        for (int row = 0; row < m_rows; ++row) {
            Line &line = screenLineRef(row);
            for (int c = 0; c < m_columns; ++c)
                line[c] = blankCell(m_cursor.bg);
        }
        if (!m_altActive) {
            while (static_cast<int>(m_primary.size()) > m_rows)
                m_primary.pop_front();
        }
        m_dirty = true;
        return;
    }

    if (mode == 2) {
        for (int row = 0; row < m_rows; ++row) {
            Line &line = screenLineRef(row);
            for (int c = 0; c < m_columns; ++c)
                line[c] = blankCell(m_cursor.bg);
        }
        m_dirty = true;
        return;
    }

    if (mode == 0) {
        Line &line = screenLineRef(m_cursor.row);
        for (int c = m_cursor.col; c < m_columns; ++c)
            line[c] = blankCell(m_cursor.bg);
        for (int row = m_cursor.row + 1; row < m_rows; ++row) {
            Line &below = screenLineRef(row);
            for (int c = 0; c < m_columns; ++c)
                below[c] = blankCell(m_cursor.bg);
        }
        m_dirty = true;
        return;
    }

    if (mode == 1) {
        for (int row = 0; row < m_cursor.row; ++row) {
            Line &above = screenLineRef(row);
            for (int c = 0; c < m_columns; ++c)
                above[c] = blankCell(m_cursor.bg);
        }
        Line &line = screenLineRef(m_cursor.row);
        for (int c = 0; c <= m_cursor.col && c < m_columns; ++c)
            line[c] = blankCell(m_cursor.bg);
        m_dirty = true;
    }
}

void TerminalScreen::tabForward(int count)
{
    count = qMax(1, count);
    for (int i = 0; i < count; ++i) {
        int next = m_columns - 1;
        for (int c = m_cursor.col + 1; c < m_columns; ++c) {
            if (m_tabStops.contains(c)) {
                next = c;
                break;
            }
        }
        m_cursor.col = next;
    }
    m_cursor.wrapPending = false;
}

void TerminalScreen::tabBackward(int count)
{
    count = qMax(1, count);
    for (int i = 0; i < count; ++i) {
        int previous = 0;
        for (int c = m_cursor.col - 1; c >= 0; --c) {
            if (m_tabStops.contains(c)) {
                previous = c;
                break;
            }
        }
        m_cursor.col = previous;
    }
    m_cursor.wrapPending = false;
}

void TerminalScreen::saveCursor()
{
    m_savedCursor = m_cursor;
}

void TerminalScreen::restoreCursor()
{
    m_cursor = m_savedCursor;
    m_cursor.row = qBound(0, m_cursor.row, m_rows - 1);
    m_cursor.col = qBound(0, m_cursor.col, m_columns - 1);
    m_cursor.wrapPending = false;
    m_dirty = true;
}

void TerminalScreen::enterAltScreen(bool saveCursorState, bool clear)
{
    if (m_altActive)
        return;
    if (saveCursorState)
        m_savedCursor = m_cursor;
    m_altActive = true;
    if (clear) {
        for (Line &line : m_alt) {
            for (int c = 0; c < line.size(); ++c)
                line[c] = blankCell();
        }
    }
    while (m_alt.size() < m_rows)
        m_alt.append(blankLine());
    m_cursor.row = 0;
    m_cursor.col = 0;
    m_cursor.wrapPending = false;
    m_scrollTop = 0;
    m_scrollBottom = m_rows - 1;
    m_dirty = true;
}

void TerminalScreen::exitAltScreen(bool restoreCursorState)
{
    if (!m_altActive)
        return;
    m_altActive = false;
    if (restoreCursorState) {
        m_cursor = m_savedCursor;
        m_cursor.row = qBound(0, m_cursor.row, m_rows - 1);
        m_cursor.col = qBound(0, m_cursor.col, m_columns - 1);
        m_cursor.wrapPending = false;
    }
    m_scrollTop = 0;
    m_scrollBottom = m_rows - 1;
    m_dirty = true;
}

void TerminalScreen::repeatLastChar(int count)
{
    if (!m_lastCharValid)
        return;
    count = qBound(1, count, m_columns * m_rows);
    for (int i = 0; i < count; ++i)
        putChar(m_lastChar);
}

void TerminalScreen::putChar(char32_t codepoint)
{
    int width = charWidth(codepoint);
    if (width == 0)
        return;

    if (m_cursor.wrapPending && m_autoWrap) {
        m_cursor.col = 0;
        index();
    }
    m_cursor.wrapPending = false;

    if (width == 2 && m_cursor.col >= m_columns - 1) {
        if (m_autoWrap) {
            m_cursor.col = 0;
            index();
        } else {
            width = 1;
        }
    }

    Line &line = screenLineRef(m_cursor.row);
    if (m_insertMode) {
        const int safe = qMin(width, m_columns - m_cursor.col);
        line.insert(m_cursor.col, safe, blankCell(m_cursor.bg));
        line.resize(m_columns);
    }

    Cell cell;
    cell.ch = codepoint;
    cell.fg = m_cursor.fg;
    cell.bg = m_cursor.bg;
    cell.flags = m_cursor.flags;
    cell.width = static_cast<quint8>(width);
    line[m_cursor.col] = cell;

    if (width == 2 && m_cursor.col + 1 < m_columns) {
        Cell continuation = blankCell(m_cursor.bg);
        continuation.ch = 0;
        continuation.width = 0;
        continuation.flags = m_cursor.flags;
        line[m_cursor.col + 1] = continuation;
    }

    m_lastChar = codepoint;
    m_lastCharValid = true;
    m_cursor.col += width;
    if (m_cursor.col >= m_columns) {
        if (m_autoWrap) {
            m_cursor.col = m_columns - 1;
            m_cursor.wrapPending = true;
        } else {
            m_cursor.col = m_columns - 1;
        }
    }
    m_dirty = true;
}

void TerminalScreen::putAscii(char ch)
{
    putChar(static_cast<unsigned char>(ch));
}

void TerminalScreen::resetPen()
{
    m_cursor.fg = DefaultColor;
    m_cursor.bg = DefaultColor;
    m_cursor.flags = 0;
}

void TerminalScreen::setPenColor(bool foreground, quint32 color)
{
    if (foreground)
        m_cursor.fg = color;
    else
        m_cursor.bg = color;
}

void TerminalScreen::feed(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return;

    QByteArray data = m_utf8Pending;
    m_utf8Pending.clear();
    data.append(bytes);

    const auto *raw = reinterpret_cast<const unsigned char *>(data.constData());
    const int size = data.size();
    int i = 0;
    while (i < size) {
        const unsigned char byte = raw[i];
        if (byte < 0x80) {
            handleChar(byte);
            ++i;
            continue;
        }

        int length = 0;
        char32_t codepoint = 0;
        if ((byte & 0xE0) == 0xC0) {
            length = 2;
            codepoint = byte & 0x1F;
        } else if ((byte & 0xF0) == 0xE0) {
            length = 3;
            codepoint = byte & 0x0F;
        } else if ((byte & 0xF8) == 0xF0) {
            length = 4;
            codepoint = byte & 0x07;
        } else {
            handleChar(0xFFFD);
            ++i;
            continue;
        }

        if (i + length > size) {
            m_utf8Pending = data.mid(i);
            break;
        }

        bool valid = true;
        for (int j = 1; j < length; ++j) {
            if ((raw[i + j] & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6) | (raw[i + j] & 0x3F);
        }
        if (!valid || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            handleChar(0xFFFD);
            ++i;
            continue;
        }
        handleChar(codepoint);
        i += length;
    }

    m_dirty = true;
}

void TerminalScreen::handleChar(char32_t codepoint)
{
    switch (m_parseState) {
    case ParseState::Ground:
        if (codepoint == 0x1B) {
            m_parseState = ParseState::Escape;
            return;
        }
        if (codepoint < 0x20 || codepoint == 0x7F) {
            handleControl(codepoint);
            return;
        }
        if (codepoint >= 0x80 && codepoint <= 0x9F) {
            if (codepoint == 0x9B) {
                m_parseState = ParseState::Csi;
                m_csiParams.clear();
                m_csiCurrent = 0;
                m_csiHasParam = false;
                m_csiPrefix = QChar();
            } else if (codepoint == 0x9D) {
                m_parseState = ParseState::Osc;
                m_oscBuffer.clear();
            }
            return;
        }
        putChar(codepoint);
        return;

    case ParseState::Escape:
        handleEscape(codepoint);
        return;

    case ParseState::Csi:
        handleCsi(codepoint);
        return;

    case ParseState::Osc:
        handleOsc(codepoint);
        return;

    case ParseState::OscEscape:
        if (codepoint == '\\') {
            finishOsc();
            m_parseState = ParseState::Ground;
        } else if (codepoint == 0x1B) {
            // stay
        } else {
            m_parseState = ParseState::Osc;
        }
        return;

    case ParseState::Dcs:
        if (codepoint == 0x1B)
            m_parseState = ParseState::DcsEscape;
        else if (codepoint == 0x07)
            m_parseState = ParseState::Ground;
        return;

    case ParseState::DcsEscape:
        if (codepoint == '\\')
            m_parseState = ParseState::Ground;
        else if (codepoint != 0x1B)
            m_parseState = ParseState::Dcs;
        return;

    case ParseState::Charset:
        m_parseState = ParseState::Ground;
        return;
    }
}

void TerminalScreen::handleControl(char32_t codepoint)
{
    switch (codepoint) {
    case 0x07: // BEL
        m_bell = true;
        break;
    case 0x08: // BS
        if (m_cursor.col > 0)
            --m_cursor.col;
        m_cursor.wrapPending = false;
        break;
    case 0x09: // HT
        tabForward(1);
        break;
    case 0x0A: // LF
    case 0x0B:
    case 0x0C:
        index();
        if (m_newlineMode)
            m_cursor.col = 0;
        break;
    case 0x0D: // CR
        m_cursor.col = 0;
        m_cursor.wrapPending = false;
        break;
    case 0x18:
    case 0x1A:
        m_parseState = ParseState::Ground;
        break;
    default:
        break;
    }
}

void TerminalScreen::handleEscape(char32_t codepoint)
{
    switch (codepoint) {
    case '[':
        m_parseState = ParseState::Csi;
        m_csiParams.clear();
        m_csiCurrent = 0;
        m_csiHasParam = false;
        m_csiPrefix = QChar();
        m_csiIntermediate = QChar();
        return;
    case ']':
        m_parseState = ParseState::Osc;
        m_oscBuffer.clear();
        return;
    case 'P':
        m_parseState = ParseState::Dcs;
        return;
    case '(':
    case ')':
    case '*':
    case '+':
    case '-':
    case '.':
    case '/':
    case '#':
        m_parseState = ParseState::Charset;
        return;
    case '7':
        saveCursor();
        m_parseState = ParseState::Ground;
        return;
    case '8':
        restoreCursor();
        m_parseState = ParseState::Ground;
        return;
    case 'D':
        index();
        m_parseState = ParseState::Ground;
        return;
    case 'E':
        m_cursor.col = 0;
        index();
        m_parseState = ParseState::Ground;
        return;
    case 'M':
        reverseIndex();
        m_parseState = ParseState::Ground;
        return;
    case 'c':
        reset();
        m_parseState = ParseState::Ground;
        return;
    case 0x1B:
        return;
    default:
        m_parseState = ParseState::Ground;
        return;
    }
}

void TerminalScreen::handleCsi(char32_t codepoint)
{
    if (codepoint == 0x1B) {
        m_parseState = ParseState::Escape;
        return;
    }
    if (codepoint < 0x20) {
        handleControl(codepoint);
        return;
    }
    if (codepoint >= '0' && codepoint <= '9') {
        m_csiCurrent = m_csiCurrent * 10 + static_cast<int>(codepoint - '0');
        m_csiCurrent = qMin(m_csiCurrent, 1000000);
        m_csiHasParam = true;
        return;
    }
    if (codepoint == ';' || codepoint == ':') {
        m_csiParams.append(m_csiHasParam ? m_csiCurrent : 0);
        m_csiCurrent = 0;
        m_csiHasParam = false;
        return;
    }
    if (codepoint >= 0x3C && codepoint <= 0x3F) {
        m_csiPrefix = QChar(static_cast<ushort>(codepoint));
        return;
    }
    if (codepoint >= 0x20 && codepoint <= 0x2F) {
        m_csiIntermediate = QChar(static_cast<ushort>(codepoint));
        return;
    }
    if (codepoint >= 0x40 && codepoint <= 0x7E) {
        if (m_csiHasParam || !m_csiParams.isEmpty())
            m_csiParams.append(m_csiHasParam ? m_csiCurrent : 0);
        dispatchCsi(static_cast<char>(codepoint));
        m_parseState = ParseState::Ground;
    }
}

int TerminalScreen::csiParam(int index, int fallback) const
{
    if (index < 0 || index >= m_csiParams.size())
        return fallback;
    return m_csiParams.at(index);
}

bool TerminalScreen::csiFlag(int index, bool fallback) const
{
    return csiParam(index, fallback ? 1 : 0) != 0;
}

void TerminalScreen::dispatchCsi(char final)
{
    const bool greater = m_csiPrefix == QLatin1Char('>');

    switch (final) {
    case '@':
        insertChars(qMax(1, csiParam(0, 1)));
        break;
    case 'A':
        moveCursor(-qMax(1, csiParam(0, 1)), 0);
        break;
    case 'B':
    case 'e':
        moveCursor(qMax(1, csiParam(0, 1)), 0);
        break;
    case 'C':
    case 'a':
        moveCursor(0, qMax(1, csiParam(0, 1)));
        break;
    case 'D':
        moveCursor(0, -qMax(1, csiParam(0, 1)));
        break;
    case 'E':
        setCursor(m_cursor.row + qMax(1, csiParam(0, 1)), 0);
        break;
    case 'F':
        setCursor(m_cursor.row - qMax(1, csiParam(0, 1)), 0);
        break;
    case 'G':
    case '`':
        setCursor(m_cursor.row, qMax(1, csiParam(0, 1)) - 1);
        break;
    case 'H':
    case 'f': {
        int row = qMax(1, csiParam(0, 1)) - 1;
        const int col = qMax(1, csiParam(1, 1)) - 1;
        if (m_originMode)
            row += m_scrollTop;
        setCursor(row, col);
        break;
    }
    case 'J':
        eraseInDisplay(csiParam(0, 0));
        break;
    case 'K':
        eraseInLine(csiParam(0, 0));
        break;
    case 'L':
        insertLines(qMax(1, csiParam(0, 1)));
        break;
    case 'M':
        deleteLines(qMax(1, csiParam(0, 1)));
        break;
    case 'P':
        deleteChars(qMax(1, csiParam(0, 1)));
        break;
    case 'S':
        scrollUpRegion(m_scrollTop, m_scrollBottom, qMax(1, csiParam(0, 1)));
        break;
    case 'T':
        scrollDownRegion(m_scrollTop, m_scrollBottom, qMax(1, csiParam(0, 1)));
        break;
    case 'X':
        eraseChars(qMax(1, csiParam(0, 1)));
        break;
    case 'Z':
        tabBackward(qMax(1, csiParam(0, 1)));
        break;
    case 'b':
        repeatLastChar(qMax(1, csiParam(0, 1)));
        break;
    case 'c':
        if (!greater)
            appendResponse(QByteArrayLiteral("\x1b[?1;2c"));
        break;
    case 'd': {
        int row = qMax(1, csiParam(0, 1)) - 1;
        if (m_originMode)
            row += m_scrollTop;
        setCursor(row, m_cursor.col);
        break;
    }
    case 'g':
        if (csiParam(0, 0) == 0)
            m_tabStops.remove(m_cursor.col);
        else if (csiParam(0, 0) == 3)
            m_tabStops.clear();
        break;
    case 'h':
    case 'l':
        if (!greater)
            setModes(final == 'h');
        break;
    case 'm':
        parseSgr();
        break;
    case 'n': {
        const int request = csiParam(0, 0);
        if (request == 5) {
            appendResponse(QByteArrayLiteral("\x1b[0n"));
        } else if (request == 6) {
            const int row = m_cursor.row + (m_originMode ? m_scrollTop : 0) + 1;
            const int col = m_cursor.col + 1;
            appendResponse(QStringLiteral("\x1b[%1;%2R").arg(row).arg(col).toLatin1());
        }
        break;
    }
    case 'q':
        // cursor style: DECSCUSR
        break;
    case 'r': {
        int top = qMax(1, csiParam(0, 1)) - 1;
        int bottom = csiParam(1, m_rows) - 1;
        if (bottom <= 0 || bottom >= m_rows)
            bottom = m_rows - 1;
        if (top < bottom) {
            m_scrollTop = top;
            m_scrollBottom = bottom;
            setCursor(m_originMode ? m_scrollTop : 0, 0);
        }
        break;
    }
    case 's':
        saveCursor();
        break;
    case 'u':
        restoreCursor();
        break;
    case 't':
        break;
    default:
        break;
    }
}

void TerminalScreen::setModes(bool enable)
{
    const bool privateMode = m_csiPrefix == QLatin1Char('?');
    if (m_csiParams.isEmpty())
        return;

    for (int param : m_csiParams) {
        if (privateMode) {
            switch (param) {
            case 1:
                m_applicationCursorKeys = enable;
                break;
            case 3:
                break;
            case 6:
                m_originMode = enable;
                setCursor(0, 0);
                break;
            case 7:
                m_autoWrap = enable;
                break;
            case 12:
                m_cursorBlink = enable;
                break;
            case 25:
                m_cursorVisible = enable;
                break;
            case 47:
            case 1047:
                if (enable)
                    enterAltScreen(false, true);
                else
                    exitAltScreen(false);
                break;
            case 1048:
                if (enable)
                    saveCursor();
                else
                    restoreCursor();
                break;
            case 1049:
                if (enable)
                    enterAltScreen(true, true);
                else
                    exitAltScreen(true);
                break;
            case 2004:
                m_bracketedPaste = enable;
                break;
            case 1000:
            case 1002:
            case 1003:
            case 1005:
            case 1006:
            case 1015:
                // Mouse reporting is intentionally not implemented.
                break;
            default:
                break;
            }
        } else {
            switch (param) {
            case 4:
                m_insertMode = enable;
                break;
            case 20:
                m_newlineMode = enable;
                break;
            default:
                break;
            }
        }
    }
    m_dirty = true;
}

void TerminalScreen::parseSgr()
{
    if (m_csiParams.isEmpty()) {
        resetPen();
        return;
    }

    for (int i = 0; i < m_csiParams.size(); ++i) {
        const int value = m_csiParams.at(i);
        if (value == 0) {
            resetPen();
        } else if (value == 1) {
            m_cursor.flags |= Bold;
        } else if (value == 2) {
            m_cursor.flags |= Dim;
        } else if (value == 3) {
            m_cursor.flags |= Italic;
        } else if (value == 4) {
            m_cursor.flags |= Underline;
        } else if (value == 5 || value == 6) {
            m_cursor.flags |= Blink;
        } else if (value == 7) {
            m_cursor.flags |= Inverse;
        } else if (value == 8) {
            m_cursor.flags |= Hidden;
        } else if (value == 9) {
            m_cursor.flags |= Strike;
        } else if (value == 21 || value == 22) {
            if (value == 22) {
                m_cursor.flags &= ~(Bold | Dim);
            } else {
                m_cursor.flags |= Underline;
            }
        } else if (value == 23) {
            m_cursor.flags &= ~Italic;
        } else if (value == 24) {
            m_cursor.flags &= ~Underline;
        } else if (value == 25) {
            m_cursor.flags &= ~Blink;
        } else if (value == 27) {
            m_cursor.flags &= ~Inverse;
        } else if (value == 28) {
            m_cursor.flags &= ~Hidden;
        } else if (value == 29) {
            m_cursor.flags &= ~Strike;
        } else if (value >= 30 && value <= 37) {
            m_cursor.fg = static_cast<quint32>(value - 30);
        } else if (value == 39) {
            m_cursor.fg = DefaultColor;
        } else if (value >= 40 && value <= 47) {
            m_cursor.bg = static_cast<quint32>(value - 40);
        } else if (value == 49) {
            m_cursor.bg = DefaultColor;
        } else if (value >= 90 && value <= 97) {
            m_cursor.fg = static_cast<quint32>(value - 90 + 8);
        } else if (value >= 100 && value <= 107) {
            m_cursor.bg = static_cast<quint32>(value - 100 + 8);
        } else if (value == 38 || value == 48 || value == 58) {
            quint32 color = DefaultColor;
            if (i + 1 < m_csiParams.size() && m_csiParams.at(i + 1) == 5 && i + 2 < m_csiParams.size()) {
                color = static_cast<quint32>(qBound(0, m_csiParams.at(i + 2), 255));
                i += 2;
            } else if (i + 1 < m_csiParams.size() && m_csiParams.at(i + 1) == 2
                       && i + 4 < m_csiParams.size()) {
                const int r = qBound(0, m_csiParams.at(i + 2), 255);
                const int g = qBound(0, m_csiParams.at(i + 3), 255);
                const int b = qBound(0, m_csiParams.at(i + 4), 255);
                color = RgbColorFlag | (static_cast<quint32>(r) << 16) | (static_cast<quint32>(g) << 8)
                        | static_cast<quint32>(b);
                i += 4;
            }
            if (value == 38)
                m_cursor.fg = color;
            else if (value == 48)
                m_cursor.bg = color;
            // 58 (underline colour) is parsed but not stored
        }
    }
    m_dirty = true;
}

void TerminalScreen::handleOsc(char32_t codepoint)
{
    if (codepoint == 0x07) {
        finishOsc();
        m_parseState = ParseState::Ground;
        return;
    }
    if (codepoint == 0x1B) {
        m_parseState = ParseState::OscEscape;
        return;
    }
    if (codepoint >= 0x20 && codepoint != 0x7F) {
        if (m_oscBuffer.size() < 8192)
            m_oscBuffer.append(stringForCodepoint(codepoint));
    }
}

void TerminalScreen::finishOsc()
{
    const QString payload = m_oscBuffer;
    m_oscBuffer.clear();
    const int separator = payload.indexOf(QLatin1Char(';'));
    if (separator < 0)
        return;
    const QString code = payload.left(separator);
    const QString value = payload.mid(separator + 1);
    if (code == QStringLiteral("0") || code == QStringLiteral("2")) {
        if (m_title != value) {
            m_title = value;
            m_titleDirty = true;
            m_dirty = true;
        }
    }
}

bool TerminalScreen::takeTitleChanged(QString *title)
{
    if (!m_titleDirty)
        return false;
    m_titleDirty = false;
    if (title)
        *title = m_title;
    return true;
}

bool TerminalScreen::takeDirty()
{
    const bool dirty = m_dirty;
    m_dirty = false;
    return dirty;
}

bool TerminalScreen::takeBell()
{
    const bool bell = m_bell;
    m_bell = false;
    return bell;
}

QByteArray TerminalScreen::takeResponse()
{
    QByteArray response;
    response.swap(m_response);
    return response;
}

void TerminalScreen::appendResponse(const QByteArray &data)
{
    m_response.append(data);
}

QString TerminalScreen::lineText(int absoluteRow) const
{
    const Line &line = lineAt(absoluteRow);
    QString text;
    text.reserve(line.size());
    for (const Cell &cell : line) {
        if (cell.width == 0)
            continue;
        if (cell.ch == 0)
            continue;
        text.append(stringForCodepoint(cell.ch));
    }
    while (text.endsWith(QLatin1Char(' ')))
        text.chop(1);
    return text;
}

QString TerminalScreen::textRange(int startRow, int startCol, int endRow, int endCol) const
{
    const int total = totalLines();
    const int firstRow = qBound(0, qMin(startRow, endRow), total - 1);
    const int lastRow = qBound(0, qMax(startRow, endRow), total - 1);
    int firstCol = qBound(0, startRow <= endRow ? startCol : endCol, m_columns - 1);
    int lastCol = qBound(0, startRow <= endRow ? endCol : startCol, m_columns - 1);

    QStringList lines;
    for (int row = firstRow; row <= lastRow; ++row) {
        const Line &line = lineAt(row);
        int from = row == firstRow ? firstCol : 0;
        int to = row == lastRow ? lastCol : m_columns - 1;
        if (to < from)
            std::swap(from, to);
        QString text;
        for (int col = from; col <= to && col < line.size(); ++col) {
            const Cell &cell = line.at(col);
            if (cell.width == 0)
                continue;
            if (cell.ch == 0)
                continue;
            text.append(stringForCodepoint(cell.ch));
        }
        while (text.endsWith(QLatin1Char(' ')))
            text.chop(1);
        lines.append(text);
    }
    return lines.join(QLatin1Char('\n'));
}

void TerminalScreen::clearScrollback()
{
    if (m_altActive)
        return;
    while (static_cast<int>(m_primary.size()) > m_rows)
        m_primary.pop_front();
    m_dirty = true;
}

int TerminalScreen::charWidth(char32_t codepoint)
{
    if (codepoint == 0)
        return 0;
    if (isCombining(codepoint))
        return 0;
    return isWide(codepoint) ? 2 : 1;
}

} // namespace nyan::term
