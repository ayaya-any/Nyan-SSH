#pragma once

#include <QByteArray>
#include <QSet>
#include <QString>
#include <QVector>

#include <deque>

namespace nyan::term {

inline constexpr quint32 DefaultColor = 0xFFFFFFFFu;
inline constexpr quint32 RgbColorFlag = 0x01000000u;
inline constexpr quint32 PaletteColorMask = 0x000000FFu;

enum CellFlag : quint16 {
    Bold = 0x0001,
    Dim = 0x0002,
    Italic = 0x0004,
    Underline = 0x0008,
    Blink = 0x0010,
    Inverse = 0x0020,
    Strike = 0x0040,
    Hidden = 0x0080,
};

struct Cell {
    char32_t ch = U' ';
    quint32 fg = DefaultColor;
    quint32 bg = DefaultColor;
    quint16 flags = 0;
    quint8 width = 1;
};

/// A VT100/xterm-compatible screen buffer with ANSI parsing, scrollback,
/// alternate screen, 256/true colour SGR and a handful of xterm extensions.
class TerminalScreen {
public:
    explicit TerminalScreen(int columns = 100, int rows = 30);

    void resize(int columns, int rows);
    void reset();
    void setMaxScrollback(int lines);
    int maxScrollback() const { return m_maxScrollback; }

    void feed(const QByteArray &bytes);

    int columns() const { return m_columns; }
    int rows() const { return m_rows; }
    int totalLines() const;
    int screenTop() const;
    int scrollbackLines() const;
    bool isAltScreen() const { return m_altActive; }

    const Cell &cellAt(int absoluteRow, int column) const;
    const QVector<Cell> &lineAt(int absoluteRow) const;
    int absoluteRow(int screenRow) const;

    int cursorRow() const { return m_cursor.row; }
    int cursorCol() const { return m_cursor.col; }
    bool cursorVisible() const { return m_cursorVisible; }
    bool cursorBlink() const { return m_cursorBlink; }
    bool applicationCursorKeys() const { return m_applicationCursorKeys; }
    bool bracketedPaste() const { return m_bracketedPaste; }

    QString title() const { return m_title; }
    bool takeTitleChanged(QString *title = nullptr);
    bool takeDirty();
    bool takeBell();
    QByteArray takeResponse();

    QString lineText(int absoluteRow) const;
    QString textRange(int startRow, int startCol, int endRow, int endCol) const;
    void clearScrollback();

private:
    using Line = QVector<Cell>;

    struct CursorState {
        int row = 0;
        int col = 0;
        quint32 fg = DefaultColor;
        quint32 bg = DefaultColor;
        quint16 flags = 0;
        bool wrapPending = false;
    };

    enum class ParseState {
        Ground,
        Escape,
        Csi,
        Osc,
        OscEscape,
        Dcs,
        DcsEscape,
        Charset,
    };

    // --- buffer helpers -----------------------------------------------------
    Cell blankCell(quint32 background = DefaultColor) const;
    Line blankLine(quint32 background = DefaultColor) const;
    Line &lineRef(int absoluteRow);
    const Line &lineRef(int absoluteRow) const;
    int primaryTop() const;
    int activeTop() const;
    void ensurePrimaryLines();
    Line &screenLineRef(int screenRow);
    void trimScrollback();

    // --- cursor / scrolling -------------------------------------------------
    void setCursor(int row, int col);
    void moveCursor(int deltaRow, int deltaCol);
    void index();
    void reverseIndex();
    void scrollUpRegion(int top, int bottom, int count);
    void scrollDownRegion(int top, int bottom, int count);
    void insertLines(int count);
    void deleteLines(int count);
    void insertChars(int count);
    void deleteChars(int count);
    void eraseChars(int count);
    void eraseInLine(int mode);
    void eraseInDisplay(int mode);
    void tabForward(int count);
    void tabBackward(int count);
    void saveCursor();
    void restoreCursor();
    void enterAltScreen(bool saveCursor, bool clear);
    void exitAltScreen(bool restoreCursor);
    void putChar(char32_t codepoint);
    void putAscii(char ch);
    void repeatLastChar(int count);
    void resetPen();
    void setPenColor(bool foreground, quint32 color);

    // --- parser -------------------------------------------------------------
    void handleChar(char32_t codepoint);
    void handleControl(char32_t codepoint);
    void handleEscape(char32_t codepoint);
    void handleCsi(char32_t codepoint);
    void dispatchCsi(char final);
    void handleOsc(char32_t codepoint);
    void finishOsc();
    void parseSgr();
    void setModes(bool enable);
    int csiParam(int index, int fallback) const;
    bool csiFlag(int index, bool fallback) const;
    static int charWidth(char32_t codepoint);
    void appendResponse(const QByteArray &data);

    // --- state --------------------------------------------------------------
    int m_columns = 100;
    int m_rows = 30;
    int m_maxScrollback = 5000;
    std::deque<Line> m_primary;
    QVector<Line> m_alt;
    bool m_altActive = false;

    CursorState m_cursor;
    CursorState m_savedCursor;
    bool m_cursorVisible = true;
    bool m_cursorBlink = true;
    bool m_autoWrap = true;
    bool m_originMode = false;
    bool m_insertMode = false;
    bool m_newlineMode = false;
    bool m_applicationCursorKeys = false;
    bool m_bracketedPaste = false;
    int m_scrollTop = 0;
    int m_scrollBottom = 0;
    QSet<int> m_tabStops;

    ParseState m_parseState = ParseState::Ground;
    QVector<int> m_csiParams;
    int m_csiCurrent = 0;
    bool m_csiHasParam = false;
    QChar m_csiPrefix;
    QChar m_csiIntermediate;
    QString m_oscBuffer;
    QByteArray m_utf8Pending;
    bool m_lastCharValid = false;
    char32_t m_lastChar = U' ';

    QString m_title;
    bool m_titleDirty = false;
    bool m_dirty = true;
    bool m_bell = false;
    QByteArray m_response;
};

} // namespace nyan::term
