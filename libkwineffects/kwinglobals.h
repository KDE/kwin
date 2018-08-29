/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWIN_LIB_KWINGLOBALS_H
#define KWIN_LIB_KWINGLOBALS_H

#include <QCoreApplication>
#include <QImage>
#include <QPoint>
#include <QVariant>

#include <kwin_export.h>

#include <xcb/xcb.h>

#include <kwinconfig.h>

#define KWIN_QT5_PORTING 0

namespace KWin
{


enum CompositingType {
    NoCompositing = 0,
    /**
     * Used as a flag whether OpenGL based compositing is used.
     * The flag is or-ed to the enum values of the specific OpenGL types.
     * The actual Compositors use the or @c OpenGL2Compositing
     * flags. If you need to know whether OpenGL is used, either and the flag or
     * use EffectsHandler::isOpenGLCompositing().
     **/
    OpenGLCompositing = 1,
    XRenderCompositing = 1<<1,
    QPainterCompositing = 1<< 2,
    OpenGL2Compositing = 1<<3 | OpenGLCompositing
};

enum OpenGLPlatformInterface {
    NoOpenGLPlatformInterface = 0,
    GlxPlatformInterface,
    EglPlatformInterface
};

enum clientAreaOption {
    PlacementArea,         // geometry where a window will be initially placed after being mapped
    MovementArea,          // ???  window movement snapping area?  ignore struts
    MaximizeArea,          // geometry to which a window will be maximized
    MaximizeFullArea,      // like MaximizeArea, but ignore struts - used e.g. for topmenu
    FullScreenArea,        // area for fullscreen windows
    // these below don't depend on xinerama settings
    WorkArea,              // whole workarea (all screens together)
    FullArea,              // whole area (all screens together), ignore struts
    ScreenArea             // one whole screen, ignore struts
};

enum ElectricBorder {
    ElectricTop,
    ElectricTopRight,
    ElectricRight,
    ElectricBottomRight,
    ElectricBottom,
    ElectricBottomLeft,
    ElectricLeft,
    ElectricTopLeft,
    ELECTRIC_COUNT,
    ElectricNone
};

// TODO: Hardcoding is bad, need to add some way of registering global actions to these.
// When designing the new system we must keep in mind that we have conditional actions
// such as "only when moving windows" desktop switching that the current global action
// system doesn't support.
enum ElectricBorderAction {
    ElectricActionNone,          // No special action, not set, desktop switch or an effect
    ElectricActionShowDesktop,   // Show desktop or restore
    ElectricActionLockScreen,   // Lock screen
    ElectricActionKRunner,       // Open KRunner
    ElectricActionActivityManager, // Activity Manager
    ElectricActionApplicationLauncher, // Application Launcher
    ELECTRIC_ACTION_COUNT
};

// DesktopMode and WindowsMode are based on the order in which the desktop
//  or window were viewed.
// DesktopListMode lists them in the order created.
enum TabBoxMode {
    TabBoxDesktopMode,                      // Focus chain of desktops
    TabBoxDesktopListMode,                  // Static desktop order
    TabBoxWindowsMode,                      // Primary window switching mode
    TabBoxWindowsAlternativeMode,           // Secondary window switching mode
    TabBoxCurrentAppWindowsMode,            // Same as primary window switching mode but only for windows of current application
    TabBoxCurrentAppWindowsAlternativeMode  // Same as secondary switching mode but only for windows of current application
};

enum KWinOption {
    CloseButtonCorner,
    SwitchDesktopOnScreenEdge,
    SwitchDesktopOnScreenEdgeMovingWindows
};

/**
 * @brief The direction in which a pointer axis is moved.
 *
 */
enum PointerAxisDirection {
    PointerAxisUp,
    PointerAxisDown,
    PointerAxisLeft,
    PointerAxisRight
};

/**
 * @brief Directions for swipe gestures
 * @since 5.10
 **/
enum class SwipeDirection {
    Invalid,
    Down,
    Left,
    Up,
    Right
};

inline
KWIN_EXPORT xcb_connection_t *connection()
{
    static xcb_connection_t *s_con = nullptr;
    if (!s_con) {
        s_con = reinterpret_cast<xcb_connection_t*>(qApp->property("x11Connection").value<void*>());
    }
    Q_ASSERT(qApp);
    return s_con;
}

inline
KWIN_EXPORT xcb_window_t rootWindow()
{
    static xcb_window_t s_rootWindow = XCB_WINDOW_NONE;
    if (s_rootWindow == XCB_WINDOW_NONE) {
        s_rootWindow = qApp->property("x11RootWindow").value<quint32>();
    }
    return s_rootWindow;
}

inline
KWIN_EXPORT xcb_timestamp_t xTime()
{
    return qApp->property("x11Time").value<xcb_timestamp_t>();
}

inline
KWIN_EXPORT xcb_screen_t *defaultScreen()
{
    static xcb_screen_t *s_screen = nullptr;
    if (s_screen) {
        return s_screen;
    }
    int screen = qApp->property("x11ScreenNumber").toInt();
    for (xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(connection()));
            it.rem;
            --screen, xcb_screen_next(&it)) {
        if (screen == 0) {
            s_screen = it.data;
        }
    }
    return s_screen;
}

inline
KWIN_DEPRECATED_EXPORT int displayWidth()
{
    xcb_screen_t *screen = defaultScreen();
    return screen ? screen->width_in_pixels : 0;
}

inline
KWIN_DEPRECATED_EXPORT int displayHeight()
{
    xcb_screen_t *screen = defaultScreen();
    return screen ? screen->height_in_pixels : 0;
}

/**
 * Short wrapper for a cursor image provided by the Platform.
 * @since 5.9
 **/
class PlatformCursorImage {
public:
    explicit PlatformCursorImage()
        : m_image()
        , m_hotSpot()
    {
    }
    explicit PlatformCursorImage(const QImage &image, const QPoint &hotSpot)
        : m_image(image)
        , m_hotSpot(hotSpot)
    {
    }
    virtual ~PlatformCursorImage() = default;

    QImage image() const {
        return m_image;
    }
    QPoint hotSpot() const {
        return m_hotSpot;
    }

private:
    QImage m_image;
    QPoint m_hotSpot;
};

} // namespace

#define KWIN_SINGLETON_VARIABLE(ClassName, variableName) \
public: \
    static ClassName *create(QObject *parent = nullptr);\
    static ClassName *self() { return variableName; }\
protected: \
    explicit ClassName(QObject *parent = nullptr); \
private: \
    static ClassName *variableName;

#define KWIN_SINGLETON(ClassName) KWIN_SINGLETON_VARIABLE(ClassName, s_self)

#define KWIN_SINGLETON_FACTORY_VARIABLE_FACTORED(ClassName, FactoredClassName, variableName) \
ClassName *ClassName::variableName = nullptr; \
ClassName *ClassName::create(QObject *parent) \
{ \
    Q_ASSERT(!variableName); \
    variableName = new FactoredClassName(parent); \
    return variableName; \
}
#define KWIN_SINGLETON_FACTORY_VARIABLE(ClassName, variableName) KWIN_SINGLETON_FACTORY_VARIABLE_FACTORED(ClassName, ClassName, variableName)
#define KWIN_SINGLETON_FACTORY_FACTORED(ClassName, FactoredClassName) KWIN_SINGLETON_FACTORY_VARIABLE_FACTORED(ClassName, FactoredClassName, s_self)
#define KWIN_SINGLETON_FACTORY(ClassName) KWIN_SINGLETON_FACTORY_VARIABLE(ClassName, s_self)

#endif
