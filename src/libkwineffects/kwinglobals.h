/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

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
KWIN_EXPORT Q_NAMESPACE

    enum CompositingType {
        NoCompositing = 0,
        /**
         * Used as a flag whether OpenGL based compositing is used.
         * The flag is or-ed to the enum values of the specific OpenGL types.
         * The actual Compositors use the or @c OpenGLCompositing
         * flags. If you need to know whether OpenGL is used, either and the flag or
         * use EffectsHandler::isOpenGLCompositing().
         */
        OpenGLCompositing = 1,
        /* XRenderCompositing = 1<<1, */
        QPainterCompositing = 1 << 2,
    };

enum OpenGLPlatformInterface {
    NoOpenGLPlatformInterface = 0,
    GlxPlatformInterface,
    EglPlatformInterface,
};

enum clientAreaOption {
    PlacementArea, // geometry where a window will be initially placed after being mapped
    MovementArea, // ???  window movement snapping area?  ignore struts
    MaximizeArea, // geometry to which a window will be maximized
    MaximizeFullArea, // like MaximizeArea, but ignore struts - used e.g. for topmenu
    FullScreenArea, // area for fullscreen windows
    // these below don't depend on xinerama settings
    WorkArea, // whole workarea (all screens together)
    FullArea, // whole area (all screens together), ignore struts
    ScreenArea, // one whole screen, ignore struts
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
    ElectricNone,
};
Q_ENUM_NS(ElectricBorder)

// TODO: Hardcoding is bad, need to add some way of registering global actions to these.
// When designing the new system we must keep in mind that we have conditional actions
// such as "only when moving windows" desktop switching that the current global action
// system doesn't support.
enum ElectricBorderAction {
    ElectricActionNone, // No special action, not set, desktop switch or an effect
    ElectricActionShowDesktop, // Show desktop or restore
    ElectricActionLockScreen, // Lock screen
    ElectricActionKRunner, // Open KRunner
    ElectricActionActivityManager, // Activity Manager
    ElectricActionApplicationLauncher, // Application Launcher
    ELECTRIC_ACTION_COUNT,
};

// DesktopMode and WindowsMode are based on the order in which the desktop
//  or window were viewed.
// DesktopListMode lists them in the order created.
enum TabBoxMode {
    TabBoxDesktopMode, // Focus chain of desktops
    TabBoxDesktopListMode, // Static desktop order
    TabBoxWindowsMode, // Primary window switching mode
    TabBoxWindowsAlternativeMode, // Secondary window switching mode
    TabBoxCurrentAppWindowsMode, // Same as primary window switching mode but only for windows of current application
    TabBoxCurrentAppWindowsAlternativeMode, // Same as secondary switching mode but only for windows of current application
};

enum KWinOption {
    CloseButtonCorner,
    SwitchDesktopOnScreenEdge,
    SwitchDesktopOnScreenEdgeMovingWindows,
};

/**
 * @brief The direction in which a pointer axis is moved.
 */
enum PointerAxisDirection {
    PointerAxisUp,
    PointerAxisDown,
    PointerAxisLeft,
    PointerAxisRight,
};

/**
 * @brief Directions for swipe gestures
 * @since 5.10
 */
enum class SwipeDirection {
    Invalid,
    Down,
    Left,
    Up,
    Right,
};

enum class PinchDirection {
    Expanding,
    Contracting
};

/**
 * Represents the state of the session running outside kwin
 * Under Plasma this is managed by ksmserver
 */
enum class SessionState {
    Normal,
    Saving,
    Quitting,
};
Q_ENUM_NS(SessionState)

enum class LED {
    NumLock = 1 << 0,
    CapsLock = 1 << 1,
    ScrollLock = 1 << 2
};
Q_DECLARE_FLAGS(LEDs, LED)
Q_FLAG_NS(LEDs)

/**
 * The Gravity enum is used to specify the direction in which geometry changes during resize.
 */
enum class Gravity {
    None,
    Left,
    Right,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

inline KWIN_EXPORT xcb_connection_t *connection()
{
    return reinterpret_cast<xcb_connection_t *>(qApp->property("x11Connection").value<void *>());
}

inline KWIN_EXPORT xcb_window_t rootWindow()
{
    return qApp->property("x11RootWindow").value<quint32>();
}

inline KWIN_EXPORT xcb_timestamp_t xTime()
{
    return qApp->property("x11Time").value<xcb_timestamp_t>();
}

/**
 * Short wrapper for a cursor image provided by the Platform.
 * @since 5.9
 */
class PlatformCursorImage
{
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

    bool isNull() const
    {
        return m_image.isNull();
    }
    QImage image() const
    {
        return m_image;
    }
    QPoint hotSpot() const
    {
        return m_hotSpot;
    }

private:
    QImage m_image;
    QPoint m_hotSpot;
};

/**
 * Infinite region (i.e. a special region type saying that everything needs to be painted).
 */
inline KWIN_EXPORT QRect infiniteRegion()
{
    // INT_MIN / 2 because width/height is used (INT_MIN+INT_MAX==-1)
    return QRect(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
}

} // namespace

Q_DECLARE_METATYPE(std::chrono::nanoseconds)

#define KWIN_SINGLETON_VARIABLE(ClassName, variableName) \
public:                                                  \
    static ClassName *create(QObject *parent = nullptr); \
    static ClassName *self()                             \
    {                                                    \
        return variableName;                             \
    }                                                    \
                                                         \
protected:                                               \
    explicit ClassName(QObject *parent = nullptr);       \
                                                         \
private:                                                 \
    static ClassName *variableName;

#define KWIN_SINGLETON(ClassName) KWIN_SINGLETON_VARIABLE(ClassName, s_self)

#define KWIN_SINGLETON_FACTORY_VARIABLE_FACTORED(ClassName, FactoredClassName, variableName) \
    ClassName *ClassName::variableName = nullptr;                                            \
    ClassName *ClassName::create(QObject *parent)                                            \
    {                                                                                        \
        Q_ASSERT(!variableName);                                                             \
        variableName = new FactoredClassName(parent);                                        \
        return variableName;                                                                 \
    }
#define KWIN_SINGLETON_FACTORY_VARIABLE(ClassName, variableName) KWIN_SINGLETON_FACTORY_VARIABLE_FACTORED(ClassName, ClassName, variableName)
#define KWIN_SINGLETON_FACTORY_FACTORED(ClassName, FactoredClassName) KWIN_SINGLETON_FACTORY_VARIABLE_FACTORED(ClassName, FactoredClassName, s_self)
#define KWIN_SINGLETON_FACTORY(ClassName) KWIN_SINGLETON_FACTORY_VARIABLE(ClassName, s_self)
