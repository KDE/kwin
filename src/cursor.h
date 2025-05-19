/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

// Qt
#include <QHash>
#include <QObject>
#include <QPoint>

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
 * The Cursor type represents a pointer or a tablet cursor on the screen.
 */
class KWIN_EXPORT Cursor : public QObject
{
    Q_OBJECT

public:
    Cursor();

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

    QPointF pos();
    void setPos(const QPointF &pos);

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
    void cursorChanged();
    void themeChanged();

private Q_SLOTS:
    void loadThemeSettings();
    void slotKGlobalSettingsNotifyChange(int type, int arg);

private:
    void updateTheme(const QString &name, int size);
    void loadThemeFromKConfig();
    CursorSource *m_source = nullptr;
    QPointF m_pos;
    QString m_themeName;
    int m_themeSize;
};

class KWIN_EXPORT Cursors : public QObject
{
    Q_OBJECT
public:
    Cursors();

    Cursor *mouse() const
    {
        return m_mouse.get();
    }

    void addCursor(Cursor *cursor);

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
    std::unique_ptr<Cursor> m_mouse;
    Cursor *m_currentCursor = nullptr;
    QList<Cursor *> m_cursors;
    int m_cursorHideCounter = 0;
};

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
