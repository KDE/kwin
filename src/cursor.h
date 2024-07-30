/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
// kwin
#include "effect/globals.h"
// Qt
#include <QHash>
#include <QObject>
#include <QPoint>
// KF
#include <KSharedConfig>
// xcb
#include <xcb/xcb.h>

class QTimer;

namespace KWin
{

class CursorSource;
class Output;

namespace ExtendedCursor
{
/**
 * Extension of Qt::CursorShape with values not currently present there
 */
enum Shape {
    SizeNorthWest = 0x100 + 0,
    SizeNorth = 0x100 + 1,
    SizeNorthEast = 0x100 + 2,
    SizeEast = 0x100 + 3,
    SizeWest = 0x100 + 4,
    SizeSouthEast = 0x100 + 5,
    SizeSouth = 0x100 + 6,
    SizeSouthWest = 0x100 + 7
};
}

/**
 * @brief Wrapper round Qt::CursorShape with extensions enums into a single entity
 */
class KWIN_EXPORT CursorShape
{
public:
    CursorShape() = default;
    CursorShape(Qt::CursorShape qtShape)
    {
        m_shape = qtShape;
    }
    CursorShape(KWin::ExtendedCursor::Shape kwinShape)
    {
        m_shape = kwinShape;
    }
    bool operator==(const CursorShape &o) const
    {
        return m_shape == o.m_shape;
    }
    operator int() const
    {
        return m_shape;
    }
    /**
     * @brief The name of a cursor shape in the theme.
     */
    QByteArray name() const;

    /**
     * Returns the list of alternative shape names for a shape with the specified @a name.
     */
    static QList<QByteArray> alternatives(const QByteArray &name);

private:
    int m_shape = Qt::ArrowCursor;
};

/**
 * @short Replacement for QCursor.
 *
 * This class provides a similar API to QCursor and should be preferred inside KWin. It allows to
 * get the position and warp the mouse cursor with static methods just like QCursor. It also provides
 * the possibility to get an X11 cursor for a Qt::CursorShape - a functionality lost in Qt 5's QCursor
 * implementation.
 *
 * In addition the class provides a mouse polling facility as required by e.g. Effects and ScreenEdges
 * and emits signals when the mouse position changes. In opposite to QCursor this class is a QObject
 * and cannot be constructed. Instead it provides a singleton getter, though the most important
 * methods are wrapped in a static method, just like QCursor.
 *
 * The actual implementation is split into two parts: a system independent interface and a windowing
 * system specific subclass. So far only an X11 backend is implemented which uses query pointer to
 * fetch the position and warp pointer to set the position. It uses a timer based mouse polling and
 * can provide X11 cursors through the XCursor library.
 */
class KWIN_EXPORT Cursor : public QObject
{
    Q_OBJECT
public:
    Cursor();
    ~Cursor() override;

    /**
     * @brief The name of the currently used Cursor theme.
     *
     * @return const QString&
     */
    const QString &themeName() const;
    /**
     * @brief The size of the currently used Cursor theme.
     *
     * @return int
     */
    int themeSize() const;
    /**
     * Returns the default Xcursor theme name.
     */
    static QString defaultThemeName();
    /**
     * Returns the default Xcursor theme size.
     */
    static int defaultThemeSize();
    /**
     * Returns the fallback Xcursor theme name.
     */
    static QString fallbackThemeName();

    /**
     * Returns the current cursor position. This method does an update of the mouse position if
     * needed. It's save to call it multiple times.
     *
     * Implementing subclasses should prefer to use currentPos which is not performing a check
     * for update.
     */
    QPointF pos();
    /**
     * Warps the mouse cursor to new @p pos.
     */
    void setPos(const QPointF &pos);
    xcb_cursor_t x11Cursor(CursorShape shape);
    /**
     * Notice: if available always use the CursorShape variant to avoid cache duplicates for
     * ambiguous cursor names in the non existing cursor name specification
     */
    xcb_cursor_t x11Cursor(const QByteArray &name);

    QPointF hotspot() const;
    QRectF geometry() const;
    QRectF rect() const;

    CursorSource *source() const;
    void setSource(CursorSource *source);

    /**
     * Returns @c true if the cursor is visible on the given output; otherwise returns @c false.
     */
    bool isOnOutput(Output *output) const;

Q_SIGNALS:
    void posChanged(const QPointF &pos);
    void mouseChanged(const QPointF &pos, const QPointF &oldpos,
                      Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                      Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
    /**
     * @brief Signal emitted when the cursor image changes.
     *
     * To enable these signals use startCursorTracking.
     *
     * @see startCursorTracking
     * @see stopCursorTracking
     */
    void cursorChanged();
    void themeChanged();

protected:
    /**
     * Performs the actual warping of the cursor.
     */
    virtual void doSetPos();
    /**
     * Called from @ref pos() to allow syncing the internal position with the underlying
     * system's cursor position.
     */
    virtual void doGetPos();
    /**
     * Provides the actual internal cursor position to inheriting classes. If an inheriting class needs
     * access to the cursor position this method should be used instead of the static @ref pos, as
     * the static method syncs with the underlying system's cursor.
     */
    const QPointF &currentPos() const;
    /**
     * Updates the internal position to @p pos without warping the pointer as
     * setPos does.
     */
    void updatePos(const QPointF &pos);

private Q_SLOTS:
    void loadThemeSettings();
    void slotKGlobalSettingsNotifyChange(int type, int arg);

private:
    void updateTheme(const QString &name, int size);
    void loadThemeFromKConfig();
    CursorSource *m_source = nullptr;
    QHash<QByteArray, xcb_cursor_t> m_cursors;
    QPointF m_pos;
    QString m_themeName;
    int m_themeSize;
};

class KWIN_EXPORT Cursors : public QObject
{
    Q_OBJECT
public:
    Cursor *mouse() const
    {
        return m_mouse;
    }

    void setMouse(Cursor *mouse)
    {
        if (m_mouse != mouse) {
            m_mouse = mouse;

            addCursor(m_mouse);
            setCurrentCursor(m_mouse);
        }
    }

    void addCursor(Cursor *cursor);
    void removeCursor(Cursor *cursor);

    ///@returns the last cursor that moved
    Cursor *currentCursor() const
    {
        return m_currentCursor;
    }

    void hideCursor();
    void showCursor();
    bool isCursorHidden() const;

    static Cursors *self();

Q_SIGNALS:
    void currentCursorChanged(Cursor *cursor);
    void hiddenChanged();
    void positionChanged(Cursor *cursor, const QPointF &position);

private:
    void emitCurrentCursorChanged();
    void setCurrentCursor(Cursor *cursor);

    static Cursors *s_self;
    Cursor *m_currentCursor = nullptr;
    Cursor *m_mouse = nullptr;
    QList<Cursor *> m_cursors;
    int m_cursorHideCounter = 0;
};

inline const QPointF &Cursor::currentPos() const
{
    return m_pos;
}

inline const QString &Cursor::themeName() const
{
    return m_themeName;
}

inline int Cursor::themeSize() const
{
    return m_themeSize;
}
}

Q_DECLARE_METATYPE(KWin::CursorShape)
