/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kwin.h"

#include <QCoreApplication>
#include <QImage>
#include <QPoint>
#include <QVariant>
#include <QVector2D>

#include <kwin_export.h>

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
Q_ENUM_NS(MaximizeMode)

inline MaximizeMode operator^(MaximizeMode m1, MaximizeMode m2)
{
    return MaximizeMode(int(m1) ^ int(m2));
}

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

enum TabBoxMode {
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
    ScrollLock = 1 << 2,
    Compose = 1 << 3,
    Kana = 1 << 4,
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

enum Layer {
    UnknownLayer = -1,
    FirstLayer = 0,
    DesktopLayer = FirstLayer,
    BelowLayer,
    NormalLayer,
    AboveLayer,
    NotificationLayer, // layer for windows of type notification
    ActiveLayer, // active fullscreen, or active dialog
    PopupLayer, // tooltips, sub- and context menus
    CriticalNotificationLayer, // layer for notifications that should be shown even on top of fullscreen
    OnScreenDisplayLayer, // layer for On Screen Display windows such as volume feedback
    OverlayLayer,
    NumLayers, // number of layers, must be last
};
Q_ENUM_NS(Layer)

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
};
Q_ENUM_NS(QuickTileFlag)
Q_DECLARE_FLAGS(QuickTileMode, QuickTileFlag)

inline QuickTileMode operator~(QuickTileFlag flag)
{
    return QuickTileMode(~int(flag));
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
    explicit PlatformCursorImage(const QImage &image, const QPointF &hotSpot)
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
    QPointF hotSpot() const
    {
        return m_hotSpot;
    }

private:
    QImage m_image;
    QPointF m_hotSpot;
};

/**
 * Infinite region (i.e. a special region type saying that everything needs to be painted).
 */
inline KWIN_EXPORT QRect infiniteRegion()
{
    // INT_MIN / 2 because width/height is used (INT_MIN+INT_MAX==-1)
    return QRect(INT_MIN / 2, INT_MIN / 2, INT_MAX, INT_MAX);
}

/**
 * Scale a rect by a scalar.
 */
KWIN_EXPORT inline QRectF scaledRect(const QRectF &rect, qreal scale)
{
    return QRectF{rect.x() * scale, rect.y() * scale, rect.width() * scale, rect.height() * scale};
}

/**
 * Round a vector to nearest integer.
 */
KWIN_EXPORT inline QVector2D roundVector(const QVector2D &input)
{
    return QVector2D(std::round(input.x()), std::round(input.y()));
}

/**
 * Convert a QPointF to a QPoint by flooring instead of rounding.
 *
 * By default, QPointF::toPoint() rounds which can cause problems in certain
 * cases.
 */
KWIN_EXPORT inline QPoint flooredPoint(const QPointF &point)
{
    return QPoint(std::floor(point.x()), std::floor(point.y()));
}

/**
 * @returns if @a point is contained in @a rect, including the left and top borders
 * but excluding the right and bottom borders
 */
static inline bool exclusiveContains(const QRectF &rect, const QPointF &point)
{
    return point.x() >= rect.x() && point.y() >= rect.y() && point.x() < (rect.x() + rect.width()) && point.y() < (rect.y() + rect.height());
}

enum class PresentationMode {
    VSync,
    AdaptiveSync,
    Async,
    AdaptiveAsync,
};
Q_ENUM_NS(PresentationMode);

enum class ContentType {
    None = 0,
    Photo = 1,
    Video = 2,
    Game = 3,
};
Q_ENUM_NS(ContentType);

enum class VrrPolicy {
    Never = 0,
    Always = 1,
    Automatic = 2,
};
Q_ENUM_NS(VrrPolicy);

enum class PresentationModeHint {
    VSync,
    Async
};
Q_ENUM_NS(PresentationModeHint);

// For now, keep in sync with NETWM::WindowType from KWindowSystem
enum class WindowType {
    /**
     * intermediate value, do not use
     */
    Undefined = -2,
    /**
     * indicates that the window did not define a window type.
     */
    Unknown = -1,
    /**
     * indicates that this is a normal, top-level window
     */
    Normal = 0,
    /**
     * indicates a desktop feature. This can include a single window
     * containing desktop icons with the same dimensions as the screen, allowing
     * the desktop environment to have full control of the desktop, without the
     * need for proxying root window clicks.
     */
    Desktop = 1,
    /**
     * indicates a dock or panel feature
     */
    Dock = 2,
    /**
     * indicates a toolbar window
     */
    Toolbar = 3,
    /**
     * indicates a pinnable (torn-off) menu window
     */
    Menu = 4,
    /**
     * indicates that this is a dialog window
     */
    Dialog = 5,
    // cannot deprecate to compiler: used both by clients & manager, later needs to keep supporting it for now
    // KF6: remove
    /**
     * @deprecated has unclear meaning and is KDE-only
     */
    Override = 6, // NON STANDARD
    /**
     * indicates a toplevel menu (AKA macmenu). This is a KDE extension to the
     * _NET_WM_WINDOW_TYPE mechanism.
     */
    TopMenu = 7, // NON STANDARD
    /**
     * indicates a utility window
     */
    Utility = 8,
    /**
     * indicates that this window is a splash screen window.
     */
    Splash = 9,
    /**
     * indicates a dropdown menu (from a menubar typically)
     */
    DropdownMenu = 10,
    /**
     * indicates a popup menu (a context menu typically)
     */
    PopupMenu = 11,
    /**
     * indicates a tooltip window
     */
    Tooltip = 12,
    /**
     * indicates a notification window
     */
    Notification = 13,
    /**
     * indicates that the window is a list for a combobox
     */
    ComboBox = 14,
    /**
     * indicates a window that represents the dragged object during DND operation
     */
    DNDIcon = 15,
    /**
     * indicates an On Screen Display window (such as volume feedback)
     */
    OnScreenDisplay = 16, // NON STANDARD
    /**
     * indicates a critical notification (such as battery is running out)
     */
    CriticalNotification = 17, // NON STANDARD
    /**
     * indicates that this window is an applet.
     */
    AppletPopup = 18, // NON STANDARD
};
Q_ENUM_NS(WindowType);

/**
 * Values for WindowType when they should be OR'ed together, e.g.
 * for the properties argument of the NETRootInfo constructor.
 * @see WindowTypes
 */
enum WindowTypeMask {
    NormalMask = 1u << 0, ///< @see Normal
    DesktopMask = 1u << 1, ///< @see Desktop
    DockMask = 1u << 2, ///< @see Dock
    ToolbarMask = 1u << 3, ///< @see Toolbar
    MenuMask = 1u << 4, ///< @see Menu
    DialogMask = 1u << 5, ///< @see Dialog
    OverrideMask = 1u << 6, ///< @see Override
    TopMenuMask = 1u << 7, ///< @see TopMenu
    UtilityMask = 1u << 8, ///< @see Utility
    SplashMask = 1u << 9, ///< @see Splash
    DropdownMenuMask = 1u << 10, ///< @see DropdownMenu
    PopupMenuMask = 1u << 11, ///< @see PopupMenu
    TooltipMask = 1u << 12, ///< @see Tooltip
    NotificationMask = 1u << 13, ///< @see Notification
    ComboBoxMask = 1u << 14, ///< @see ComboBox
    DNDIconMask = 1u << 15, ///< @see DNDIcon
    OnScreenDisplayMask = 1u << 16, ///< NON STANDARD @see OnScreenDisplay @since 5.6
    CriticalNotificationMask = 1u << 17, ///< NON STANDARD @see CriticalNotification @since 5.58
    AppletPopupMask = 1u << 18, ///< NON STANDARD @see AppletPopup
    AllTypesMask = 0U - 1, ///< All window types.
};
Q_DECLARE_FLAGS(WindowTypes, WindowTypeMask)

enum class OutputConfigurationError {
    None,
    Unknown,
    TooManyEnabledOutputs,
};

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
