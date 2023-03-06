/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// cmake stuff
#include <config-kwin.h>
#include <kwinconfig.h>
// kwin
#include <kwinglobals.h>
// Qt
#include <QList>
#include <QLoggingCategory>
#include <QMatrix4x4>
#include <QPoint>
#include <QRect>
// system
#include <climits>
Q_DECLARE_LOGGING_CATEGORY(KWIN_CORE)
Q_DECLARE_LOGGING_CATEGORY(KWIN_OPENGL)
Q_DECLARE_LOGGING_CATEGORY(KWIN_QPAINTER)
Q_DECLARE_LOGGING_CATEGORY(KWIN_VIRTUALKEYBOARD)
namespace KWin
{
Q_NAMESPACE

const QPoint invalidPoint(INT_MIN, INT_MIN);

enum Layer {
    UnknownLayer = -1,
    FirstLayer = 0,
    DesktopLayer = FirstLayer,
    BelowLayer,
    NormalLayer,
    DockLayer,
    AboveLayer,
    NotificationLayer, // layer for windows of type notification
    ActiveLayer, // active fullscreen, or active dialog
    PopupLayer, // tooltips, sub- and context menus
    CriticalNotificationLayer, // layer for notifications that should be shown even on top of fullscreen
    OnScreenDisplayLayer, // layer for On Screen Display windows such as volume feedback
    UnmanagedLayer, // layer for override redirect windows.
    NumLayers, // number of layers, must be last
};
Q_ENUM_NS(Layer)

enum StrutArea {
    StrutAreaInvalid = 0, // Null
    StrutAreaTop = 1 << 0,
    StrutAreaRight = 1 << 1,
    StrutAreaBottom = 1 << 2,
    StrutAreaLeft = 1 << 3,
    StrutAreaAll = StrutAreaTop | StrutAreaRight | StrutAreaBottom | StrutAreaLeft,
};
Q_DECLARE_FLAGS(StrutAreas, StrutArea)

class KWIN_EXPORT StrutRect : public QRect
{
public:
    explicit StrutRect(QRect rect = QRect(), StrutArea area = StrutAreaInvalid);
    StrutRect(int x, int y, int width, int height, StrutArea area = StrutAreaInvalid);
    StrutRect(const StrutRect &other);
    StrutRect &operator=(const StrutRect &other);
    inline StrutArea area() const
    {
        return m_area;
    }

private:
    StrutArea m_area;
};
typedef QVector<StrutRect> StrutRects;

enum ShadeMode {
    ShadeNone, // not shaded
    ShadeNormal, // normally shaded - isShade() is true only here
    ShadeHover, // "shaded", but visible due to hover unshade
    ShadeActivated // "shaded", but visible due to alt+tab to the window
};

/**
 * Maximize mode. These values specify how a window is maximized.
 *
 * @note these values are written to session files, don't change the order
 */
enum MaximizeMode {
    MaximizeRestore = 0, ///< The window is not maximized in any direction.
    MaximizeVertical = 1, ///< The window is maximized vertically.
    MaximizeHorizontal = 2, ///< The window is maximized horizontally.
    /// Equal to @p MaximizeVertical | @p MaximizeHorizontal
    MaximizeFull = MaximizeVertical | MaximizeHorizontal,
};

inline MaximizeMode operator^(MaximizeMode m1, MaximizeMode m2)
{
    return MaximizeMode(int(m1) ^ int(m2));
}

// TODO: could this be in Tile itself?
enum class QuickTileFlag {
    None = 0,
    Left = 1 << 0,
    Right = 1 << 1,
    Top = 1 << 2,
    Bottom = 1 << 3,
    Custom = 1 << 4,
    Horizontal = Left | Right,
    Vertical = Top | Bottom,
    Maximize = Left | Right | Top | Bottom,
};
Q_ENUM_NS(QuickTileFlag);
Q_DECLARE_FLAGS(QuickTileMode, QuickTileFlag)

void KWIN_EXPORT grabXServer();
void KWIN_EXPORT ungrabXServer();
bool KWIN_EXPORT grabXKeyboard(xcb_window_t w = XCB_WINDOW_NONE);
void KWIN_EXPORT ungrabXKeyboard();

static inline QRegion mapRegion(const QMatrix4x4 &matrix, const QRegion &region)
{
    QRegion result;
    for (const QRect &rect : region) {
        result += matrix.mapRect(rect);
    }
    return result;
}

/**
 * Small helper class which performs grabXServer in the ctor and
 * ungrabXServer in the dtor. Use this class to ensure that grab and
 * ungrab are matched.
 */
class XServerGrabber
{
public:
    XServerGrabber()
    {
        grabXServer();
    }
    ~XServerGrabber()
    {
        ungrabXServer();
    }
};

// converting between X11 mouse/keyboard state mask and Qt button/keyboard states
Qt::MouseButton x11ToQtMouseButton(int button);
Qt::MouseButton KWIN_EXPORT x11ToQtMouseButton(int button);
Qt::MouseButtons KWIN_EXPORT x11ToQtMouseButtons(int state);
Qt::KeyboardModifiers KWIN_EXPORT x11ToQtKeyboardModifiers(int state);

KWIN_EXPORT QPointF popupOffset(const QRectF &anchorRect, const Qt::Edges anchorEdge, const Qt::Edges gravity, const QSizeF popupSize);
KWIN_EXPORT QRectF gravitateGeometry(const QRectF &rect, const QRectF &bounds, Gravity gravity);

} // namespace

// Must be outside namespace
Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::StrutAreas)
Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::QuickTileMode)
