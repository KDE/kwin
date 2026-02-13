/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"
#include "core/rect.h"
#include "cursor.h"
#include "effect/globals.h"
#include "options.h"
#include "rules.h"
#include "scene/borderradius.h"
#include "utils/common.h"
#include "utils/gravity.h"

#include <functional>
#include <memory>
#include <optional>

#include <QElapsedTimer>
#include <QIcon>
#include <QKeySequence>
#include <QMatrix4x4>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUuid>

#if KWIN_BUILD_X11
#include <xcb/xcb.h>
#endif

class QMouseEvent;

namespace KDecoration3
{
class Decoration;
}

namespace KWin
{
class PlasmaWindowInterface;
class SurfaceInterface;
class Group;
class LogicalOutput;
class ClientMachine;
class EffectWindow;
class Tile;
class Shadow;
class SurfaceItem;
class VirtualDesktop;
class WindowItem;

namespace Decoration
{
class DecoratedWindowImpl;
class DecorationPalette;
}

using ElectricBorderMode = std::variant<QuickTileMode, MaximizeMode>;

/**
 * The PlacementCommand type specifies how a window should be placed in the workspace when it's shown.
 */
using PlacementCommand = std::variant<QPointF, RectF, MaximizeMode>;

/**
 * The DecorationPolicy enum indicates how the decoration mode is determined.
 */
enum class DecorationPolicy {
    /**
     * Force no decoration.
     */
    None,
    /**
     * Follow the preferred decoration mode of the client.
     */
    PreferredByClient,
    /**
     * Force the server side decoration mode.
     */
    Server,
};

/**
 * The DecorationMode enum specifies who draws the window decoration, if at all.
 */
enum class DecorationMode {
    /**
     * No decoration.
     */
    None,
    /**
     * The window decoration is drawn by the client.
     */
    Client,
    /**
     * The window decoration is drawn by the compositor.
     */
    Server,
};

class KWIN_EXPORT Window : public QObject
{
    Q_OBJECT

    /**
     * This property holds the rectangle that the pixmap or buffer of this Window
     * occupies on the screen. This rectangle includes invisible portions of the
     * window, e.g. client-side drop shadows, etc.
     */
    Q_PROPERTY(KWin::RectF bufferGeometry READ bufferGeometry NOTIFY bufferGeometryChanged)

    /**
     * The geometry of the Window without frame borders.
     */
    Q_PROPERTY(KWin::RectF clientGeometry READ clientGeometry NOTIFY clientGeometryChanged)

    /**
     * This property holds the position of the Window's frame geometry.
     */
    Q_PROPERTY(QPointF pos READ pos)

    /**
     * This property holds the size of the Window's frame geometry.
     */
    Q_PROPERTY(QSizeF size READ size)

    /**
     * This property holds the x position of the Window's frame geometry.
     */
    Q_PROPERTY(qreal x READ x NOTIFY frameGeometryChanged)

    /**
     * This property holds the y position of the Window's frame geometry.
     */
    Q_PROPERTY(qreal y READ y NOTIFY frameGeometryChanged)

    /**
     * This property holds the width of the Window's frame geometry.
     */
    Q_PROPERTY(qreal width READ width NOTIFY frameGeometryChanged)

    /**
     * This property holds the height of the Window's frame geometry.
     */
    Q_PROPERTY(qreal height READ height NOTIFY frameGeometryChanged)

    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)

    /**
     * The output where the window center is on
     */
    Q_PROPERTY(KWin::LogicalOutput *output READ output NOTIFY outputChanged)

    Q_PROPERTY(KWin::RectF rect READ rect)
    Q_PROPERTY(QString resourceName READ resourceName NOTIFY windowClassChanged)
    Q_PROPERTY(QString resourceClass READ resourceClass NOTIFY windowClassChanged)
    Q_PROPERTY(QString windowRole READ windowRole NOTIFY windowRoleChanged)

    /**
     * Returns whether the window is a desktop background window (the one with wallpaper).
     * See _NET_WM_WINDOW_TYPE_DESKTOP at https://specifications.freedesktop.org/wm-spec .
     */
    Q_PROPERTY(bool desktopWindow READ isDesktop CONSTANT)

    /**
     * Returns whether the window is a dock (i.e. a panel).
     * See _NET_WM_WINDOW_TYPE_DOCK at https://specifications.freedesktop.org/wm-spec .
     */
    Q_PROPERTY(bool dock READ isDock CONSTANT)

    /**
     * Returns whether the window is a standalone (detached) toolbar window.
     * See _NET_WM_WINDOW_TYPE_TOOLBAR at https://specifications.freedesktop.org/wm-spec .
     */
    Q_PROPERTY(bool toolbar READ isToolbar CONSTANT)

    /**
     * Returns whether the window is a torn-off menu.
     * See _NET_WM_WINDOW_TYPE_MENU at https://specifications.freedesktop.org/wm-spec .
     */
    Q_PROPERTY(bool menu READ isMenu CONSTANT)

    /**
     * Returns whether the window is a "normal" window, i.e. an application or any other window
     * for which none of the specialized window types fit.
     * See _NET_WM_WINDOW_TYPE_NORMAL at https://specifications.freedesktop.org/wm-spec .
     */
    Q_PROPERTY(bool normalWindow READ isNormalWindow CONSTANT)

    /**
     * Returns whether the window is a dialog window.
     * See _NET_WM_WINDOW_TYPE_DIALOG at https://specifications.freedesktop.org/wm-spec .
     */
    Q_PROPERTY(bool dialog READ isDialog CONSTANT)

    /**
     * Returns whether the window is a splashscreen. Note that many (especially older) applications
     * do not support marking their splash windows with this type.
     * See _NET_WM_WINDOW_TYPE_SPLASH at https://specifications.freedesktop.org/wm-spec .
     */
    Q_PROPERTY(bool splash READ isSplash CONSTANT)

    /**
     * Returns whether the window is a utility window, such as a tool window.
     * See _NET_WM_WINDOW_TYPE_UTILITY at https://specifications.freedesktop.org/wm-spec .
     */
    Q_PROPERTY(bool utility READ isUtility CONSTANT)

    /**
     * Returns whether the window is a dropdown menu (i.e. a popup directly or indirectly open
     * from the application's menubar).
     * See _NET_WM_WINDOW_TYPE_DROPDOWN_MENU at https://specifications.freedesktop.org/wm-spec .
     */
    Q_PROPERTY(bool dropdownMenu READ isDropdownMenu CONSTANT)

    /**
     * Returns whether the window is a popup menu (that is not a torn-off or dropdown menu).
     * See _NET_WM_WINDOW_TYPE_POPUP_MENU at https://specifications.freedesktop.org/wm-spec .
     */
    Q_PROPERTY(bool popupMenu READ isPopupMenu CONSTANT)

    /**
     * Returns whether the window is a tooltip.
     * See _NET_WM_WINDOW_TYPE_TOOLTIP at https://specifications.freedesktop.org/wm-spec .
     */
    Q_PROPERTY(bool tooltip READ isTooltip CONSTANT)

    /**
     * Returns whether the window is a window with a notification.
     * See _NET_WM_WINDOW_TYPE_NOTIFICATION at https://specifications.freedesktop.org/wm-spec .
     */
    Q_PROPERTY(bool notification READ isNotification CONSTANT)

    /**
     * Returns whether the window is a window with a critical notification.
     */
    Q_PROPERTY(bool criticalNotification READ isCriticalNotification CONSTANT)

    /**
     * Returns whether the window is an applet popup.
     */
    Q_PROPERTY(bool appletPopup READ isAppletPopup CONSTANT)

    /**
     * Returns whether the window is an On Screen Display.
     */
    Q_PROPERTY(bool onScreenDisplay READ isOnScreenDisplay CONSTANT)

    /**
     * Returns whether the window is a combobox popup.
     * See _NET_WM_WINDOW_TYPE_COMBO at https://specifications.freedesktop.org/wm-spec .
     */
    Q_PROPERTY(bool comboBox READ isComboBox CONSTANT)

    /**
     * Returns whether the window is a Drag&Drop icon.
     * See _NET_WM_WINDOW_TYPE_DND at https://specifications.freedesktop.org/wm-spec .
     */
    Q_PROPERTY(bool dndIcon READ isDNDIcon CONSTANT)

    /**
     * Returns the NETWM window type.
     * See https://specifications.freedesktop.org/wm-spec .
     */
    Q_PROPERTY(WindowType windowType READ windowType CONSTANT)

    /**
     * Whether this Window is managed by KWin (it has control over its placement and other
     * aspects, as opposed to override-redirect windows that are entirely handled by the application).
     */
    Q_PROPERTY(bool managed READ isClient CONSTANT)

    /**
     * Whether this Window represents an already deleted window and only kept for the compositor for animations.
     */
    Q_PROPERTY(bool deleted READ isDeleted CONSTANT)

    /**
     * Whether the window does not want to be animated on window close.
     * There are legit reasons for this like a screenshot application which does not want its
     * window being captured.
     */
    Q_PROPERTY(bool skipsCloseAnimation READ skipsCloseAnimation WRITE setSkipCloseAnimation NOTIFY skipCloseAnimationChanged)

    /**
     * Whether the window is a popup.
     */
    Q_PROPERTY(bool popupWindow READ isPopupWindow)

    /**
     * Whether this Window represents the outline.
     *
     * @note It's always @c false if compositing is turned off.
     */
    Q_PROPERTY(bool outline READ isOutline)

    /**
     * This property holds a UUID to uniquely identify this Window.
     */
    Q_PROPERTY(QUuid internalId READ internalId CONSTANT)

    /**
     * The pid of the process owning this window.
     *
     * @since 5.20
     */
    Q_PROPERTY(int pid READ pid CONSTANT)

    /**
     * The position of this window within Workspace's window stack.
     */
    Q_PROPERTY(int stackingOrder READ stackingOrder NOTIFY stackingOrderChanged)

    /**
     * Whether this Window is fullScreen. A Window might either be fullScreen due to the _NET_WM property
     * or through a legacy support hack. The fullScreen state can only be changed if the Window does not
     * use the legacy hack. To be sure whether the state changed, connect to the notify signal.
     */
    Q_PROPERTY(bool fullScreen READ isFullScreen WRITE setFullScreen NOTIFY fullScreenChanged)

    /**
     * Whether the Window can be set to fullScreen. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     */
    Q_PROPERTY(bool fullScreenable READ isFullScreenable)

    /**
     * Whether this Window is active or not. Use Workspace::activateWindow() to activate a Window.
     * @see Workspace::activateWindow
     */
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)

    /**
     * The virtual desktops this client is on. If it's on all desktops, the list is empty.
     */
    Q_PROPERTY(QList<KWin::VirtualDesktop *> desktops READ desktops WRITE setDesktops NOTIFY desktopsChanged)

    /**
     * Whether the Window is on all desktops. That is desktop is -1.
     */
    Q_PROPERTY(bool onAllDesktops READ isOnAllDesktops WRITE setOnAllDesktops NOTIFY desktopsChanged)

    /**
     * The activities this client is on. If it's on all activities the property is empty.
     */
    Q_PROPERTY(QStringList activities READ activities WRITE setOnActivities NOTIFY activitiesChanged)

    /**
     * Indicates that the window should not be included on a taskbar.
     */
    Q_PROPERTY(bool skipTaskbar READ skipTaskbar WRITE setSkipTaskbar NOTIFY skipTaskbarChanged)

    /**
     * Indicates that the window should not be included on a Pager.
     */
    Q_PROPERTY(bool skipPager READ skipPager WRITE setSkipPager NOTIFY skipPagerChanged)

    /**
     * Whether the Window should be excluded from window switching effects.
     */
    Q_PROPERTY(bool skipSwitcher READ skipSwitcher WRITE setSkipSwitcher NOTIFY skipSwitcherChanged)

    /**
     * Whether the window can be closed by the user.
     */
    Q_PROPERTY(bool closeable READ isCloseable NOTIFY closeableChanged)

    Q_PROPERTY(QIcon icon READ icon NOTIFY iconChanged)

    /**
     * Whether the Window is set to be kept above other windows.
     */
    Q_PROPERTY(bool keepAbove READ keepAbove WRITE setKeepAbove NOTIFY keepAboveChanged)

    /**
     * Whether the Window is set to be kept below other windows.
     */
    Q_PROPERTY(bool keepBelow READ keepBelow WRITE setKeepBelow NOTIFY keepBelowChanged)

    /**
     * Whether the Window can be minimized. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     */
    Q_PROPERTY(bool minimizable READ isMinimizable)

    /**
     * Whether the Window is minimized.
     */
    Q_PROPERTY(bool minimized READ isMinimized WRITE setMinimized NOTIFY minimizedChanged)

    /**
     * The optional geometry representing the minimized Window in e.g a taskbar.
     * See _NET_WM_ICON_GEOMETRY at https://specifications.freedesktop.org/wm-spec .
     * The value is evaluated each time the getter is called.
     * Because of that no changed signal is provided.
     */
    Q_PROPERTY(KWin::RectF iconGeometry READ iconGeometry)

    /**
     * Returns whether the window is any of special windows types (desktop, dock, splash, ...),
     * i.e. window types that usually don't have a window frame and the user does not use window
     * management (moving, raising,...) on them.
     * The value is evaluated each time the getter is called.
     * Because of that no changed signal is provided.
     */
    Q_PROPERTY(bool specialWindow READ isSpecialWindow)

    /**
     * Whether window state _NET_WM_STATE_DEMANDS_ATTENTION is set. This state indicates that some
     * action in or with the window happened. For example, it may be set by the Window Manager if
     * the window requested activation but the Window Manager refused it, or the application may set
     * it if it finished some work. This state may be set by both the Window and the Window Manager.
     * It should be unset by the Window Manager when it decides the window got the required attention
     * (usually, that it got activated).
     */
    Q_PROPERTY(bool demandsAttention READ isDemandingAttention WRITE demandAttention NOTIFY demandsAttentionChanged)

    /**
     * The Caption of the Window. Read from WM_NAME property together with a suffix for hostname and shortcut.
     * To read only the caption as provided by WM_NAME, use @c captionNormal.
     */
    Q_PROPERTY(QString caption READ caption NOTIFY captionChanged)

    /**
     * The Caption of the Window. Read from WM_NAME property.
     */
    Q_PROPERTY(QString captionNormal READ captionNormal NOTIFY captionNormalChanged)

    /**
     * Minimum size as specified in WM_NORMAL_HINTS
     */
    Q_PROPERTY(QSizeF minSize READ minSize)

    /**
     * Maximum size as specified in WM_NORMAL_HINTS
     */
    Q_PROPERTY(QSizeF maxSize READ maxSize)

    /**
     * Whether the Window can accept keyboard focus.
     * The value is evaluated each time the getter is called.
     * Because of that no changed signal is provided.
     */
    Q_PROPERTY(bool wantsInput READ wantsInput)

    /**
     * Whether the Window is a transient Window to another Window.
     * @see transientFor
     */
    Q_PROPERTY(bool transient READ isTransient NOTIFY transientChanged)

    /**
     * The Window to which this Window is a transient if any.
     */
    Q_PROPERTY(KWin::Window *transientFor READ transientFor NOTIFY transientChanged)

    /**
     * Whether the Window represents a modal window.
     */
    Q_PROPERTY(bool modal READ isModal NOTIFY modalChanged)

    /**
     * The geometry of this Window. Be aware that depending on resize mode the frameGeometryChanged
     * signal might be emitted at each resize step or only at the end of the resize operation.
     */
    Q_PROPERTY(KWin::RectF frameGeometry READ frameGeometry WRITE moveResize NOTIFY frameGeometryChanged)

    /**
     * Whether the Window is currently being moved by the user.
     * Notify signal is emitted when the Window starts or ends move/resize mode.
     */
    Q_PROPERTY(bool move READ isInteractiveMove NOTIFY moveResizedChanged)

    /**
     * Whether the Window is currently being resized by the user.
     * Notify signal is emitted when the Window starts or ends move/resize mode.
     */
    Q_PROPERTY(bool resize READ isInteractiveResize NOTIFY moveResizedChanged)

    /**
     * Whether the decoration is currently using an alpha channel.
     */
    Q_PROPERTY(bool decorationHasAlpha READ decorationHasAlpha)

    /**
     * Whether the window has a decoration or not.
     * This property is not allowed to be set by applications themselves.
     * The decision whether a window has a border or not belongs to the window manager.
     * If this property gets abused by application developers, it will be removed again.
     */
    Q_PROPERTY(bool noBorder READ noBorder WRITE setNoBorder NOTIFY decorationPolicyChanged)

    /**
     * Whether the Window provides context help. Mostly needed by decorations to decide whether to
     * show the help button or not.
     */
    Q_PROPERTY(bool providesContextHelp READ providesContextHelp CONSTANT)

    /**
     * Whether the Window can be maximized both horizontally and vertically.
     * The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     */
    Q_PROPERTY(bool maximizable READ isMaximizable)

    /**
     * Whether the window is maximized horizontally, vertically or fully.
     * This is read only, in order to maximize from a script use
     * the setMaximize function
     * @see setMaximize
     */
    Q_PROPERTY(KWin::MaximizeMode maximizeMode READ maximizeMode NOTIFY maximizedChanged)

    /**
     * Whether the Window is moveable. Even if it is not moveable, it might be possible to move
     * it to another screen. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     * @see moveableAcrossScreens
     */
    Q_PROPERTY(bool moveable READ isMovable)

    /**
     * Whether the Window can be moved to another screen. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     * @see moveable
     */
    Q_PROPERTY(bool moveableAcrossScreens READ isMovableAcrossScreens)

    /**
     * Whether the Window can be resized. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     */
    Q_PROPERTY(bool resizeable READ isResizable)

    /**
     * The desktop file name of the application this Window belongs to.
     *
     * This is either the base name without full path and without file extension of the
     * desktop file for the window's application (e.g. "org.kde.foo").
     *
     * The application's desktop file name can also be the full path to the desktop file
     * (e.g. "/opt/kde/share/org.kde.foo.desktop") in case it's not in a standard location.
     */
    Q_PROPERTY(QString desktopFileName READ desktopFileName NOTIFY desktopFileNameChanged)

    /**
     * Whether an application menu is available for this Window
     */
    Q_PROPERTY(bool hasApplicationMenu READ hasApplicationMenu NOTIFY hasApplicationMenuChanged)

    /**
     * Whether the application menu for this Window is currently opened
     */
    Q_PROPERTY(bool applicationMenuActive READ applicationMenuActive NOTIFY applicationMenuActiveChanged)

    /**
     * Whether this window is unresponsive.
     *
     * When an application failed to react on a ping request in time, it is
     * considered unresponsive. This usually indicates that the application froze or crashed.
     */
    Q_PROPERTY(bool unresponsive READ unresponsive NOTIFY unresponsiveChanged)

    /**
     * The color scheme set on this window
     * Absolute file path, or name of palette in the user's config directory following KColorSchemes format.
     * An empty string indicates the default palette from kdeglobals is used.
     * @note This indicates the colour scheme requested, which might differ from the theme applied if the colorScheme cannot be found
     */
    Q_PROPERTY(QString colorScheme READ colorScheme NOTIFY colorSchemeChanged)

    Q_PROPERTY(KWin::Layer layer READ layer)

    /**
     * Whether this window is hidden. It's usually the case with auto-hide panels.
     */
    Q_PROPERTY(bool hidden READ isHidden NOTIFY hiddenChanged)

    /**
     * The Tile this window is associated to, if any
     */
    Q_PROPERTY(KWin::Tile *tile READ requestedTile WRITE setTileCompatibility NOTIFY tileChanged)

    /**
     * Returns whether this window is an input method window.
     * This is only used for Wayland.
     */
    Q_PROPERTY(bool inputMethod READ isInputMethod)

    /**
     * A client-provided tag of the window.
     * Not necessarily unique, but can be used to identify similar windows
     * across application restarts
     */
    Q_PROPERTY(QString tag READ tag NOTIFY tagChanged)

    /**
     * A client-provided description of the window
     */
    Q_PROPERTY(QString description READ description NOTIFY descriptionChanged)

    /**
     * Exclude this window from ScreenCast.
     * It will also be applied for all transient windows recursively.
     */
    Q_PROPERTY(bool excludeFromCapture READ excludeFromCapture WRITE setExcludeFromCapture NOTIFY excludeFromCaptureChanged FINAL)
public:
    ~Window() override;

    void ref();
    void unref();

    /**
     * Returns the last requested geometry. The returned value indicates the bounding
     * geometry, meaning that the client can commit smaller window geometry if the window
     * is resized.
     *
     * The main difference between the frame geometry and the move-resize geometry is
     * that the former specifies the current geometry while the latter specifies the next
     * geometry.
     */
    RectF moveResizeGeometry() const;

    /**
     * Returns the output where the last move or resize operation has occurred. The
     * window is expected to land on this output after the move/resize operation completes.
     */
    LogicalOutput *moveResizeOutput() const;
    void setMoveResizeOutput(LogicalOutput *output);

    /**
     * Returns the geometry of the pixmap or buffer attached to this Window.
     *
     * For X11 windows, this method returns server-side geometry of the Window.
     *
     * For Wayland windows, this method returns rectangle that the main surface
     * occupies on the screen, in global screen coordinates.
     */
    RectF bufferGeometry() const;
    /**
     * Returns the geometry of the Window, excluding invisible portions, e.g.
     * server-side and client-side drop shadows, etc.
     */
    RectF frameGeometry() const;
    /**
     * Returns the geometry of the client window, in global screen coordinates.
     */
    RectF clientGeometry() const;
    /**
     * Returns the extents of the server-side decoration.
     *
     * Note that the returned margins object will have all margins set to 0 if
     * the window doesn't have a server-side decoration.
     */
    QMargins frameMargins() const;

    BorderRadius borderRadius() const;
    void setBorderRadius(const BorderRadius &radius);

    virtual QSizeF minSize() const;
    virtual QSizeF maxSize() const;
    QSizeF size() const;
    QPointF pos() const;
    RectF rect() const;
    qreal x() const;
    qreal y() const;
    qreal width() const;
    qreal height() const;
    bool isOnOutput(LogicalOutput *output) const;
    bool isOnActiveOutput() const;
    LogicalOutput *output() const;
    void setOutput(LogicalOutput *output);
    QSizeF clientSize() const;
    /**
     * Returns a rectangle that the window occupies on the screen, including drop-shadows.
     */
    RectF visibleGeometry() const;

    /**
     * Maps the specified @a point from the global screen coordinates to the frame coordinates.
     */
    QPointF mapToFrame(const QPointF &point) const;
    /**
     * Maps the specified @a point from the global screen coordinates to the surface-local
     * coordinates of the main surface. For X11 windows, this function maps the specified point
     * from the global screen coordinates to the buffer-local coordinates.
     */
    QPointF mapToLocal(const QPointF &point) const;
    QPointF mapFromLocal(const QPointF &point) const;

    /**
     * Calculates the matching client position for the given frame position @p point.
     */
    virtual QPointF framePosToClientPos(const QPointF &point) const;
    virtual QPointF nextFramePosToClientPos(const QPointF &point) const;
    /**
     * Calculates the matching frame position for the given client position @p point.
     */
    virtual QPointF clientPosToFramePos(const QPointF &point) const;
    virtual QPointF nextClientPosToFramePos(const QPointF &point) const;
    /**
     * Calculates the matching client size for the given frame size @p size.
     *
     * Notice that size constraints won't be applied.
     *
     * Default implementation returns the frame size with frame margins being excluded.
     */
    virtual QSizeF frameSizeToClientSize(const QSizeF &size) const;
    virtual QSizeF nextFrameSizeToClientSize(const QSizeF &size) const;
    /**
     * Calculates the matching frame size for the given client size @p size.
     *
     * Notice that size constraints won't be applied.
     *
     * Default implementation returns the client size with frame margins being included.
     */
    virtual QSizeF clientSizeToFrameSize(const QSizeF &size) const;
    virtual QSizeF nextClientSizeToFrameSize(const QSizeF &size) const;
    /**
     * Calculates the matching client rect for the given frame rect @p rect.
     *
     * Notice that size constraints won't be applied.
     */
    RectF frameRectToClientRect(const RectF &rect) const;
    RectF nextFrameRectToClientRect(const RectF &rect) const;
    /**
     * Calculates the matching frame rect for the given client rect @p rect.
     *
     * Notice that size constraints won't be applied.
     */
    RectF clientRectToFrameRect(const RectF &rect) const;
    RectF nextClientRectToFrameRect(const RectF &rect) const;

    /**
     * How to resize the window in order to obey constraints (mainly aspect ratios).
     */
    enum SizeMode {
        SizeModeAny,
        SizeModeFixedW, ///< Try not to affect width
        SizeModeFixedH, ///< Try not to affect height
        SizeModeMax ///< Try not to make it larger in either direction
    };

    virtual QSizeF constrainClientSize(const QSizeF &size, SizeMode mode = SizeModeAny) const;
    QSizeF constrainFrameSize(const QSizeF &size, SizeMode mode = SizeModeAny) const;

    /**
     * Moves the window so that the new topLeft corner of the frame is @p topLeft.
     */
    void move(const QPointF &topLeft);

    /**
     * Resizes the window to have a new @p size but stay with the top-left corner in the same position.
     */
    void resize(const QSizeF &size);

    /**
     * Requests a new @p geometry for the window that the implementation will need to adopt
     * within its possibilities.
     */
    void moveResize(const RectF &geometry);

    /**
     * Returns @c true if the window has already been placed in the workspace; otherwise returns @c false.
     */
    bool isPlaced() const;

    /**
     * Places the window in the workspace as specified by the @a placement command.
     */
    void place(const PlacementCommand &placement);

    void growHorizontal();
    void shrinkHorizontal();
    void growVertical();
    void shrinkVertical();

    virtual RectF resizeWithChecks(const RectF &geometry, const QSizeF &s) const = 0;
    RectF keepInArea(RectF geometry, RectF area, bool partial = false) const;

    // prefer isXXX() instead
    virtual WindowType windowType() const = 0;
    bool hasNETSupport() const;
    bool isDesktop() const;
    bool isDock() const;
    bool isToolbar() const;
    bool isMenu() const;
    bool isNormalWindow() const; // normal as in 'NET::Normal or NET::Unknown non-transient'
    bool isDialog() const;
    bool isSplash() const;
    bool isUtility() const;
    bool isDropdownMenu() const;
    bool isPopupMenu() const; // a context popup, not dropdown, not torn-off
    bool isTooltip() const;
    bool isNotification() const;
    bool isCriticalNotification() const;
    bool isAppletPopup() const;
    bool isOnScreenDisplay() const;
    bool isComboBox() const;
    bool isDNDIcon() const;

    virtual bool isLockScreen() const;
    virtual bool isInputMethod() const;
    virtual bool isOutline() const;
    virtual bool isInternal() const;
    virtual bool isPopupWindow() const;

    virtual bool isClient() const;
    bool isDeleted() const;
    virtual bool isUnmanaged() const;
    virtual bool isPictureInPicture() const;

    bool isLockScreenOverlay() const;
    void setLockScreenOverlay(bool allowed);

    QStringList desktopIds() const;
    QList<VirtualDesktop *> desktops() const;
    void setDesktops(QList<VirtualDesktop *> desktops);
    void enterDesktop(VirtualDesktop *desktop);
    void leaveDesktop(VirtualDesktop *desktop);
    bool isOnDesktop(VirtualDesktop *desktop) const;
    bool isOnCurrentDesktop() const;
    bool isOnAllDesktops() const;
    void setOnAllDesktops(bool set);

    virtual QStringList activities() const;
    bool isOnActivity(const QString &activity) const;
    bool isOnCurrentActivity() const;
    bool isOnAllActivities() const;
    void setOnActivity(const QString &activity, bool enable);
    void setOnActivities(const QStringList &newActivitiesList);
    void setOnAllActivities(bool all);
    virtual void updateActivities(bool includeTransients);
    void blockActivityUpdates(bool b = true);

    /**
     * Refresh Window's cache of activities
     * Called when activity daemon status changes
     */
    virtual void checkActivities(){};

    virtual QString windowRole() const;
    QString resourceName() const;
    QString resourceClass() const;
    QString wmClientMachine(bool use_localhost) const;
    ClientMachine *clientMachine() const;
    virtual bool isLocalhost() const;
    virtual pid_t pid() const;

    bool readyForPainting() const; // true if the window has been already painted its contents
    void setOpacity(qreal opacity);
    qreal opacity() const;
    void setupItem();
    EffectWindow *effectWindow();
    const EffectWindow *effectWindow() const;
    SurfaceItem *surfaceItem() const;
    WindowItem *windowItem() const;

    /**
     * Returns the Shadow associated with this Window or @c null if it has no shadow.
     */
    Shadow *shadow() const;
    /**
     * Updates the Shadow associated with this Window from X11 Property.
     * Call this method when the Property changes or Compositing is started.
     */
    void updateShadow();
    /**
     * Whether the Window currently wants the shadow to be rendered.
     */
    bool wantsShadowToBeRendered() const;

    bool skipsCloseAnimation() const;
    void setSkipCloseAnimation(bool set);

    SurfaceInterface *surface() const;
    void setSurface(SurfaceInterface *surface);

    /**
     * @returns Transformation to map from global to window coordinates.
     */
    QMatrix4x4 inputTransformation() const;

    /**
     * Returns @c true if the window can accept input at the specified position @a point.
     */
    virtual bool hitTest(const QPointF &point) const;

    /**
     * The window has a popup grab. This means that when it got mapped the
     * parent window had an implicit (pointer) grab.
     *
     * Normally this is only relevant for transient windows.
     *
     * Once the popup grab ends (e.g. pointer press outside of any Window of
     * the client), the method popupDone should be invoked.
     *
     * The default implementation returns @c false.
     * @see popupDone
     * @since 5.10
     */
    virtual bool hasPopupGrab() const
    {
        return false;
    }
    /**
     * This method should be invoked for windows with a popup grab when
     * the grab ends.
     *
     * The default implementation does nothing.
     * @see hasPopupGrab
     * @since 5.10
     */
    virtual void popupDone(){};

    /**
     * @brief Finds the Window matching the condition expressed in @p func in @p list.
     *
     * The method is templated to operate on either a list of windows or on a list of
     * a subclass type of Window.
     * @param list The list to search in
     * @param func The condition function (compare std::find_if)
     * @return T* The found Window or @c null if there is no matching Window
     */
    template<class T, class U>
    static T *findInList(const QList<T *> &list, std::function<bool(const U *)> func);

    /**
     * A UUID to uniquely identify this Window independent of windowing system.
     */
    QUuid internalId() const
    {
        return m_internalId;
    }

    int stackingOrder() const;
    void setStackingOrder(int order); ///< @internal

    bool skipSwitcher() const
    {
        return m_skipSwitcher;
    }
    void setSkipSwitcher(bool set);

    bool skipTaskbar() const
    {
        return m_skipTaskbar;
    }
    void setSkipTaskbar(bool set);
    void setOriginalSkipTaskbar(bool set);
    bool originalSkipTaskbar() const
    {
        return m_originalSkipTaskbar;
    }

    bool skipPager() const
    {
        return m_skipPager;
    }
    void setSkipPager(bool set);

    const QIcon &icon() const
    {
        return m_icon;
    }

    bool isActive() const
    {
        return m_active;
    }
    /**
     * Sets the window's active state to \a act.
     *
     * This function does only change the visual appearance of the window,
     * it does not change the focus setting. Use
     * Workspace::activateClient() or Workspace::requestFocus() instead.
     *
     * If a window receives or looses the focus, it calls setActive() on
     * its own.
     */
    void setActive(bool);

    bool keepAbove() const
    {
        return m_keepAbove;
    }
    void setKeepAbove(bool);
    bool keepBelow() const
    {
        return m_keepBelow;
    }
    void setKeepBelow(bool);

    void demandAttention(bool set = true);
    bool isDemandingAttention() const
    {
        return m_demandsAttention;
    }

    void cancelAutoRaise();

    /**
     * @returns The caption consisting of captionNormal and captionSuffix
     * @see captionNormal
     * @see captionSuffix
     */
    QString caption() const;
    /**
     * @returns The caption as set by the Window without any suffix.
     * @see caption
     * @see captionSuffix
     */
    virtual QString captionNormal() const = 0;
    /**
     * @returns The suffix added to the caption (e.g. shortcut, machine name, etc.)
     * @see caption
     * @see captionNormal
     */
    virtual QString captionSuffix() const = 0;
    virtual bool isPlaceable() const;
    virtual bool isCloseable() const = 0;
    bool isShown() const;
    bool isHidden() const;
    void setHidden(bool hidden);
    bool isHiddenByShowDesktop() const;
    void setHiddenByShowDesktop(bool hidden);
    Window *findModal() const;
    virtual bool isTransient() const;
    const Window *transientFor() const;
    Window *transientFor();
    void setTransientFor(Window *transientFor);
    /**
     * @returns @c true if transient is the transient_for window for this window,
     *  or recursively the transient_for window
     * @todo: remove boolean trap
     */
    virtual bool hasTransient(const Window *transient, bool indirect) const;
    const QList<Window *> &transients() const; // Is not indirect
    virtual void addTransient(Window *transient);
    virtual void removeTransient(Window *transient);
    void removeTransientFromList(Window *cl);
    virtual QList<Window *> mainWindows() const; // Call once before loop , is not indirect
    QList<Window *> allMainWindows() const; // Call once before loop , is indirect
    /**
     * Returns true for "special" windows and false for windows which are "normal"
     * (normal=window which has a border, can be moved by the user, can be closed, etc.)
     * true for Desktop, Dock, Splash, Override and TopMenu (and Toolbar??? - for now)
     * false for Normal, Dialog, Utility and Menu (and Toolbar??? - not yet) TODO
     */
    bool isSpecialWindow() const;
    void sendToOutput(LogicalOutput *output);
    const QKeySequence &shortcut() const
    {
        return _shortcut;
    }
    void setShortcut(const QString &cut);

    RectF iconGeometry() const;

    void setMinimized(bool set);
    bool isMinimized() const
    {
        return m_minimized;
    }
    virtual bool isMinimizable() const;

    bool isSuspended() const;
    void setSuspended(bool suspended);

    RectF fullscreenGeometryRestore() const;
    void setFullscreenGeometryRestore(const RectF &geom);
    virtual bool isFullScreenable() const;
    virtual bool isFullScreen() const;
    virtual bool isRequestedFullScreen() const;
    virtual void setFullScreen(bool set);

    bool wantsAdaptiveSync() const;
    bool wantsTearing(bool tearingRequested) const;

    RectF geometryRestore() const;
    void setGeometryRestore(const RectF &rect);
    virtual bool isMaximizable() const;
    virtual MaximizeMode maximizeMode() const;
    virtual MaximizeMode requestedMaximizeMode() const;
    virtual void maximize(MaximizeMode mode, const RectF &restore = RectF());
    /**
     * Sets the maximization according to @p vertically and @p horizontally.
     */
    Q_INVOKABLE void setMaximize(bool vertically, bool horizontally, const RectF &restore = RectF());

    QPalette palette();
    const Decoration::DecorationPalette *decorationPalette();

    /**
     * Returns whether the window is resizable or has a fixed size.
     */
    virtual bool isResizable() const = 0;
    /**
     * Returns whether the window is moveable or has a fixed position.
     */
    virtual bool isMovable() const = 0;
    /**
     * Returns whether the window can be moved to another screen.
     */
    virtual bool isMovableAcrossScreens() const = 0;

    const WindowRules *rules() const
    {
        return &m_rules;
    }
    void removeRule(Rules *r);
    void setupWindowRules();
    void finishWindowRules();
    void evaluateWindowRules();
    virtual void updateWindowRules(Rules::Types selection);
    virtual void applyWindowRules();
    virtual bool supportsWindowRules() const;

    bool wantsTabFocus() const;
    virtual void takeFocus();
    virtual bool wantsInput() const = 0;
    void checkWorkspacePosition(RectF oldGeometry = RectF(), const VirtualDesktop *oldDesktop = nullptr, LogicalOutput *oldOutput = nullptr);
#if KWIN_BUILD_X11
    virtual xcb_timestamp_t userTime() const;
#endif

    void keyPressEvent(QKeyCombination key_code);

    virtual void pointerEnterEvent(const QPointF &globalPos);
    virtual void pointerLeaveEvent();

    // a helper for the workspace window packing. tests for screen validity and updates since in maximization case as with normal moving
    void packTo(qreal left, qreal top);

    Tile *tile() const;
    void commitTile(Tile *tile);
    Tile *requestedTile() const;
    void requestTile(Tile *tile);
    void forgetTile(Tile *tile);
    void setTileCompatibility(Tile *tile);

    void handleQuickTileShortcut(QuickTileMode mode);
    void setQuickTileModeAtCurrentPosition(QuickTileMode mode);
    void setQuickTileMode(QuickTileMode mode, const QPointF &tileAtPoint);
    QuickTileMode quickTileMode() const;
    QuickTileMode requestedQuickTileMode() const;

    void handleCustomQuickTileShortcut(QuickTileMode mode);

    Layer layer() const;
    void updateLayer();

    /**
     * Returns @c true if the Client is being interactively moved; otherwise @c false.
     */
    bool isInteractiveMove() const
    {
        return isInteractiveMoveResize() && interactiveMoveResizeGravity() == Gravity::None;
    }
    /**
     * Returns @c true if the Client is being interactively resized; otherwise @c false.
     */
    bool isInteractiveResize() const
    {
        return isInteractiveMoveResize() && interactiveMoveResizeGravity() != Gravity::None;
    }
    Gravity interactiveMoveResizeGravity() const
    {
        return m_interactiveMoveResize.gravity;
    }
    QPointF interactiveMoveResizeAnchor() const
    {
        return m_interactiveMoveResize.anchor;
    }
    /**
     * Cursor shape for move/resize mode.
     */
    CursorShape cursor() const
    {
        return m_interactiveMoveResize.cursor;
    }
    uint32_t interactiveMoveResizeCount() const;

    void updateInteractiveMoveResize(const QPointF &global, Qt::KeyboardModifiers modifiers);
    /**
     * Ends move resize when all pointer buttons are up again.
     */
    void endInteractiveMoveResize();
    void cancelInteractiveMoveResize();

    virtual StrutRect strutRect(StrutArea area) const;
    StrutRects strutRects() const;
    virtual bool hasStrut() const;

    void setModal(bool modal);
    bool isModal() const;

    /**
     * Determines the mouse command for the given @p button in the current state.
     */
    std::optional<Options::MouseCommand> getMousePressCommand(Qt::MouseButton button) const;
    std::optional<Options::MouseCommand> getMouseReleaseCommand(Qt::MouseButton button) const;
    std::optional<Options::MouseCommand> getWheelCommand(Qt::Orientation orientation) const;
    bool mousePressCommandConsumesEvent(Options::MouseCommand command) const;
    /**
     * @returns whether or not the command consumes the event that triggered it
     */
    bool performMousePressCommand(Options::MouseCommand, const QPointF &globalPos);
    void performMouseReleaseCommand(Options::MouseCommand, const QPointF &globalPos);

    // decoration related
    Qt::Edge titlebarPosition() const;
    bool titlebarPositionUnderMouse() const;
    KDecoration3::Decoration *decoration() const
    {
        return m_decoration.decoration.get();
    }
    virtual KDecoration3::Decoration *nextDecoration() const;
    bool isDecorated() const
    {
        return m_decoration.decoration != nullptr;
    }
    Decoration::DecoratedWindowImpl *decoratedWindow() const;
    void setDecoratedWindow(Decoration::DecoratedWindowImpl *client);
    bool decorationHasAlpha() const;
    void triggerDecorationRepaint();
    void layoutDecorationRects(RectF &left, RectF &top, RectF &right, RectF &bottom) const;
    void processDecorationMove(const QPointF &localPos, const QPointF &globalPos);
    bool processDecorationButtonPress(const QPointF &localPos, const QPointF &globalPos, Qt::MouseButton button, bool ignoreMenu = false);
    void processDecorationButtonRelease(Qt::MouseButton button);

    virtual void invalidateDecoration();

    bool noBorder() const;
    void setNoBorder(bool set);
    bool userCanSetNoBorder() const;

    virtual DecorationPolicy decorationPolicy() const;
    virtual void setDecorationPolicy(DecorationPolicy policy);

    /**
     * Returns whether the window provides context help or not. If it does,
     * you should show a help menu item or a help button like '?' and call
     * contextHelp() if this is invoked.
     *
     * Default implementation returns @c false.
     * @see showContextHelp;
     */
    virtual bool providesContextHelp() const;

    /**
     * Invokes context help on the window. Only works if the window
     * actually provides context help.
     *
     * Default implementation does nothing.
     *
     * @see providesContextHelp()
     */
    virtual void showContextHelp();

    /**
     * @returns the geometry of the virtual keyboard
     * This geometry is in global coordinates
     */
    RectF virtualKeyboardGeometry() const;

    /**
     * Sets the geometry of the virtual keyboard, The window may resize itself in order to make space for the keyboard
     * This geometry is in global coordinates
     */
    virtual void setVirtualKeyboardGeometry(const RectF &geo);

    /**
     * Restores the Window after it had been hidden due to show on screen edge functionality.
     * The Window also gets raised (e.g. Panel mode windows can cover) and the Window
     * gets informed in a window specific way that it is shown and raised again.
     */
    virtual void showOnScreenEdge();

    QString desktopFileName() const
    {
        return m_desktopFileName;
    }

    /**
     * Helper function to compute the icon out of an application id defined by @p fileName
     *
     * @returns an icon name that can be used with QIcon::fromTheme()
     */
    static QString iconFromDesktopFile(const QString &fileName);

    static QString findDesktopFile(const QString &fileName);

    /**
     * Tries to terminate the process of this Window.
     *
     * Implementing subclasses can perform a windowing system solution for terminating.
     */
    virtual void killWindow() = 0;
    virtual void destroyWindow() = 0;

    enum class SameApplicationCheck {
        RelaxedForActive = 1 << 0,
        AllowCrossProcesses = 1 << 1
    };
    Q_DECLARE_FLAGS(SameApplicationChecks, SameApplicationCheck)
    static bool belongToSameApplication(const Window *c1, const Window *c2, SameApplicationChecks checks = SameApplicationChecks());
    virtual bool belongsToDesktop() const;

    bool hasApplicationMenu() const;
    bool applicationMenuActive() const
    {
        return m_applicationMenuActive;
    }
    void setApplicationMenuActive(bool applicationMenuActive);

    QString applicationMenuServiceName() const
    {
        return m_applicationMenuServiceName;
    }
    QString applicationMenuObjectPath() const
    {
        return m_applicationMenuObjectPath;
    }

    /**
     * Request showing the application menu bar
     * @param actionId The DBus menu ID of the action that should be highlighted, 0 for the root menu
     */
    void showApplicationMenu(int actionId);

    virtual QString preferredColorScheme() const;
    QString colorScheme() const;
    void setColorScheme(const QString &colorScheme);

    bool unresponsive() const;

    /**
     * Default implementation returns @c null.
     * Mostly intended for X11 clients, from EWMH:
     * @verbatim
     * If the WM_TRANSIENT_FOR property is set to None or Root window, the window should be
     * treated as a transient for all other windows in the same group. It has been noted that this
     * is a slight ICCCM violation, but as this behavior is pretty standard for many toolkits and
     * window managers, and is extremely unlikely to break anything, it seems reasonable to document
     * it as standard.
     * @endverbatim
     */
    virtual bool groupTransient() const;
    /**
     * Default implementation returns @c null.
     *
     * Mostly for X11 clients, holds the client group
     */
    virtual const Group *group() const;
    /**
     * Default implementation returns @c null.
     *
     * Mostly for X11 clients, holds the client group
     */
    virtual Group *group();

    /**
     * Return window management interface
     */
    PlasmaWindowInterface *windowManagementInterface() const
    {
        return m_windowManagementInterface;
    }

    /**
     * Sets the last user usage serial of the surface as @p serial
     * TODO make the QPA responsible for this, it's only needed
     * for KWindowSystem / for internal windows
     */
    void setLastUsageSerial(quint32 serial);
    quint32 lastUsageSerial() const;

    void refOffscreenRendering();
    void unrefOffscreenRendering();
    bool isOffscreenRendering() const;

    qreal targetScale() const;
    qreal nextTargetScale() const;
    void setNextTargetScale(qreal scale);

    OutputTransform preferredBufferTransform() const;
    void setPreferredBufferTransform(OutputTransform transform);

    const std::shared_ptr<ColorDescription> &preferredColorDescription() const;
    void setPreferredColorDescription(const std::shared_ptr<ColorDescription> &description);

    QString tag() const;
    QString description() const;

    void setActivationToken(const QString &token);
    QString activationToken() const;

    bool excludeFromCapture() const;
    void setExcludeFromCapture(bool newExcludeFromCapture);

public Q_SLOTS:
    virtual void closeWindow() = 0;

protected Q_SLOTS:
    void setReadyForPainting();

Q_SIGNALS:
    void stackingOrderChanged();
    void opacityChanged(KWin::Window *window, qreal oldOpacity);
    void damaged(KWin::Window *window);
    void inputTransformationChanged();
    void closed();
    /**
     * Emitted whenever the Window's screen changes. This can happen either in consequence to
     * a screen being removed/added or if the Window's geometry changes.
     * @since 4.11
     */
    void outputChanged();
    void skipCloseAnimationChanged();
    /**
     * Emitted whenever the window role of the window changes.
     * @since 5.0
     */
    void windowRoleChanged();
    /**
     * Emitted whenever the window class name or resource name of the window changes.
     * @since 5.0
     */
    void windowClassChanged();

    /**
     * Emitted whenever the Surface for this Window changes.
     */
    void surfaceChanged();

    /**
     * Emitted whenever the window's shadow changes.
     * @since 5.15
     */
    void shadowChanged();

    /**
     * This signal is emitted when the Window's buffer geometry changes.
     */
    void bufferGeometryChanged(const KWin::RectF &oldGeometry);
    /**
     * This signal is emitted when the Window's frame geometry changes.
     */
    void frameGeometryChanged(const KWin::RectF &oldGeometry);
    /**
     * This signal is emitted when the Window's client geometry has changed.
     */
    void clientGeometryChanged(const KWin::RectF &oldGeometry);

    /**
     * This signal is emitted when the frame geometry is about to change. The new geometry is not known yet
     */
    void frameGeometryAboutToChange();

    /**
     * This signal is emitted when the visible geometry has changed.
     */
    void visibleGeometryChanged();

    /**
     * This signal is emitted when associated tile has changed, including from and to none
     */
    void tileChanged(KWin::Tile *tile);
    void requestedTileChanged();

    void fullScreenChanged();
    void skipTaskbarChanged();
    void skipPagerChanged();
    void skipSwitcherChanged();
    void iconChanged();
    void activeChanged();
    void keepAboveChanged(bool);
    void keepBelowChanged(bool);
    /**
     * Emitted whenever the demands attention state changes.
     */
    void demandsAttentionChanged();
    void desktopsChanged();
    void activitiesChanged();
    void minimizedChanged();
    void paletteChanged(const QPalette &p);
    void colorSchemeChanged();
    void captionChanged();
    void captionNormalChanged();
    void maximizedAboutToChange(MaximizeMode mode);
    void maximizedChanged();
    void transientChanged();
    void modalChanged();
    void quickTileModeChanged();
    void moveResizedChanged();
    void moveResizeCursorChanged(CursorShape);
    void interactiveMoveResizeStarted();
    void interactiveMoveResizeStepped(const KWin::RectF &geometry);
    void interactiveMoveResizeFinished();
    void closeableChanged(bool);
    void minimizeableChanged(bool);
    void maximizeableChanged(bool);
    void desktopFileNameChanged();
    void applicationMenuChanged();
    void hasApplicationMenuChanged(bool);
    void applicationMenuActiveChanged(bool);
    void unresponsiveChanged(bool);
    void decorationChanged();
    void hiddenChanged();
    void hiddenByShowDesktopChanged();
    void lockScreenOverlayChanged();
    void readyForPaintingChanged();
    void maximizeGeometryRestoreChanged();
    void fullscreenGeometryRestoreChanged();
    void offscreenRenderingChanged();
    void targetScaleChanged();
    void nextTargetScaleChanged();
    void tagChanged();
    void descriptionChanged();
    void borderRadiusChanged();
    void excludeFromCaptureChanged();
    void decorationPolicyChanged();

protected:
    Window();

    virtual std::unique_ptr<WindowItem> createItem(Item *parentItem) = 0;

    void setResourceClass(const QString &name, const QString &className = QString());
    void setIcon(const QIcon &icon);
    void startAutoRaise();
    void autoRaise();
    bool isMostRecentlyRaised() const;
    void markAsDeleted();
    void markAsPlaced();
    /**
     * Whether the window accepts focus.
     * The difference to wantsInput is that the implementation should not check rules and return
     * what the window effectively supports.
     */
    virtual bool acceptsFocus() const = 0;
    /**
     * Called from setActive once the active value got updated, but before the changed signal
     * is emitted.
     *
     * Default implementation does nothing.
     */
    virtual void doSetActive();
    /**
     * Called from setKeepAbove once the keepBelow value got updated, but before the changed signal
     * is emitted.
     *
     * Default implementation does nothing.
     */
    virtual void doSetKeepAbove();
    /**
     * Called from setKeepBelow once the keepBelow value got updated, but before the changed signal
     * is emitted.
     *
     * Default implementation does nothing.
     */
    virtual void doSetKeepBelow();
    /**
     * Called from setDeskop once the desktop value got updated, but before the changed signal
     * is emitted.
     *
     * Default implementation does nothing.
     */
    virtual void doSetDesktop();
    /**
     * Called from @ref setOnActivities just after the activity list member has been updated, but before
     * @ref updateActivities is called.
     *
     * @param activityList the new list of activities set on that window
     *
     * Default implementation does nothing
     */
    virtual void doSetOnActivities(const QStringList &activityList);
    /**
     * Called from @ref minimize and @ref unminimize once the minimized value got updated, but before the
     * changed signal is emitted.
     *
     * Default implementation does nothig.
     */
    virtual void doMinimize();
    virtual bool belongsToSameApplication(const Window *other, SameApplicationChecks checks) const = 0;

    virtual void doSetSkipTaskbar();
    virtual void doSetSkipPager();
    virtual void doSetSkipSwitcher();
    virtual void doSetDemandsAttention();
    virtual void doSetQuickTileMode();
    virtual void doSetHidden();
    virtual void doSetHiddenByShowDesktop();
    virtual void doSetSuspended();
    virtual void doSetModal();
    virtual void doSetNextTargetScale();
    virtual void doSetPreferredBufferTransform();
    virtual void doSetPreferredColorDescription();

    void setupWindowManagementInterface();
    void destroyWindowManagementInterface();
    void updateColorScheme();
    void ensurePalette();
    void handlePaletteChange();

    virtual Layer belongsToLayer() const;
    bool isActiveFullScreen() const;
    void performDelayedRaise();

    // electric border / quick tiling
    void setElectricBorderMode(std::optional<ElectricBorderMode> mode);
    std::optional<ElectricBorderMode> electricBorderMode() const
    {
        return m_electricMode;
    }
    void setElectricBorderMaximizing(bool maximizing);
    bool isElectricBorderMaximizing() const
    {
        return m_electricMaximizing;
    }
    void updateElectricGeometryRestore();
    RectF quickTileGeometryRestore() const;
    RectF quickTileGeometry(QuickTileMode mode, const QPointF &pos) const;
    void exitQuickTileMode();

    // geometry handling
    void checkOffscreenPosition(RectF *geom, const RectF &screenArea);
    qreal borderLeft() const;
    qreal borderRight() const;
    qreal borderTop() const;
    qreal borderBottom() const;

    enum class MoveResizeMode : uint {
        None,
        Move = 0x1,
        Resize = 0x2,
        MoveResize = Move | Resize,
    };
    virtual void moveResizeInternal(const RectF &rect, MoveResizeMode mode) = 0;

    /**
     * @returns whether the Window is currently in move resize mode
     */
    bool isInteractiveMoveResize() const
    {
        return m_interactiveMoveResize.enabled;
    }
    /**
     * Sets whether the Window is in move resize mode to @p enabled.
     */
    void setInteractiveMoveResize(bool enabled)
    {
        m_interactiveMoveResize.enabled = enabled;
    }
    void setInteractiveMoveResizeAnchor(const QPointF &anchor)
    {
        m_interactiveMoveResize.anchor = anchor;
    }
    void setInteractiveMoveResizeModifiers(Qt::KeyboardModifiers modifiers)
    {
        m_interactiveMoveResize.modifiers = modifiers;
    }
    /**
     * @returns whether the move resize mode is unrestricted.
     */
    bool isUnrestrictedInteractiveMoveResize() const
    {
        return m_interactiveMoveResize.unrestricted;
    }
    /**
     * Sets whether move resize mode is unrestricted to @p set.
     */
    void setUnrestrictedInteractiveMoveResize(bool set)
    {
        m_interactiveMoveResize.unrestricted = set;
    }
    /**
     * Normalized position of the move resize anchor point relative to the top-left window
     * corner when the move resize operation started.
     *
     * QPointF(0, 0) corresponds to the top left window corner, QPointF(1, 1) corresponds to
     * the bottom right window corner.
     */
    QPointF interactiveMoveOffset() const
    {
        return m_interactiveMoveResize.offset;
    }
    void setInteractiveMoveOffset(const QPointF &offset)
    {
        m_interactiveMoveResize.offset = offset;
    }
    RectF initialInteractiveMoveResizeGeometry() const
    {
        return m_interactiveMoveResize.initialGeometry;
    }
    void setMoveResizeGeometry(const RectF &geo);
    void setInteractiveMoveResizeGravity(Gravity gravity)
    {
        m_interactiveMoveResize.gravity = gravity;
    }
    bool isInteractiveMoveResizePointerButtonDown() const
    {
        return m_interactiveMoveResize.buttonDown;
    }
    void setInteractiveMoveResizePointerButtonDown(bool down)
    {
        m_interactiveMoveResize.buttonDown = down;
    }
    void checkUnrestrictedInteractiveMoveResize();
    /**
     * Sets an appropriate cursor shape for the logical mouse position.
     */
    void updateCursor();
    void startDelayedInteractiveMoveResize();
    void stopDelayedInteractiveMoveResize();
    bool startInteractiveMoveResize();
    /**
     * Called from startMoveResize.
     *
     * Implementing classes should return @c false if starting move resize should
     * get aborted. In that case startMoveResize will also return @c false.
     *
     * Base implementation returns @c true.
     */
    virtual bool doStartInteractiveMoveResize();
    virtual void doFinishInteractiveMoveResize();
    void finishInteractiveMoveResize(bool cancel);
    /**
     * Leaves the move resize mode.
     *
     * Inheriting classes must invoke the base implementation which
     * ensures that the internal mode is properly ended.
     */
    virtual void leaveInteractiveMoveResize();
    /*
     * Checks if the mouse cursor is near the edge of the screen and if so
     * activates quick tiling or maximization
     */
    void checkQuickTilingMaximizationZones(int xroot, int yroot);
    void resetQuickTilingMaximizationZones();
    /**
     * Whether a sync request is still pending.
     * Default implementation returns @c false.
     */
    virtual bool isWaitingForInteractiveResizeSync() const;
    /**
     * Called during handling a resize. Implementing subclasses can use this
     * method to perform windowing system specific syncing.
     */
    virtual void doInteractiveResizeSync(const RectF &rect);
    qreal titlebarThickness() const;
    RectF nextInteractiveMoveGeometry(const QPointF &global) const;
    RectF nextInteractiveResizeGeometry(const QPointF &global) const;
    void dontInteractiveMoveResize();

    virtual QSizeF resizeIncrements() const;

    /**
     * Returns the interactive move resize gravity depending on the Decoration's section
     * under mouse. If no decoration it returns Gravity::None.
     */
    Gravity mouseGravity() const;

    void setDecoration(std::shared_ptr<KDecoration3::Decoration> decoration);
    void startDecorationDoubleClickTimer();
    void invalidateDecorationDoubleClickTimer();
    void updateDecorationInputShape();
    void updateDecorationBorderRadius();

    void setDesktopFileName(const QString &name);
    QString iconFromDesktopFile() const;

    void updateApplicationMenuServiceName(const QString &serviceName);
    void updateApplicationMenuObjectPath(const QString &objectPath);

    void setUnresponsive(bool unresponsive);

    virtual void setShortcutInternal();
    QString shortcutCaptionSuffix() const;
    virtual void updateCaption() = 0;

    // The geometry that the window should be restored when the virtual keyboard closes
    RectF keyboardGeometryRestore() const;
    void setKeyboardGeometryRestore(const RectF &geom);

    RectF moveToArea(const RectF &geometry, const RectF &oldArea, const RectF &newArea);
    RectF ensureSpecialStateGeometry(const RectF &geometry);

    void cleanTabBox();
    void maybeSendFrameCallback();

    void updateNextTargetScale();
    void updatePreferredBufferTransform();
    void updatePreferredColorDescription();
    void setTargetScale(qreal scale);

    void setDescription(const QString &description);

    LogicalOutput *m_output = nullptr;
    RectF m_frameGeometry;
    RectF m_clientGeometry;
    RectF m_bufferGeometry;
    bool ready_for_painting;
    bool m_hidden = false;
    bool m_hiddenByShowDesktop = false;

    BorderRadius m_borderRadius;

    qreal m_nextTargetScale = 1;
    qreal m_targetScale = 1;
    OutputTransform m_preferredBufferTransform = OutputTransform::Normal;
    std::shared_ptr<ColorDescription> m_preferredColorDescription = ColorDescription::sRGB;

    int m_refCount = 1;
    QUuid m_internalId;
    std::unique_ptr<WindowItem> m_windowItem;
    std::unique_ptr<Shadow> m_shadow;
    QString resource_name;
    QString resource_class;
    ClientMachine *m_clientMachine;
    bool m_skipCloseAnimation;
    QPointer<SurfaceInterface> m_surface;
    qreal m_opacity = 1.0;
    int m_stackingOrder = 0;

    bool m_skipTaskbar = false;
    /**
     * Unaffected by KWin
     */
    bool m_originalSkipTaskbar = false;
    bool m_skipPager = false;
    bool m_skipSwitcher = false;
    QIcon m_icon;
    bool m_active = false;
    bool m_deleted = false;
    bool m_placed = false;
    bool m_keepAbove = false;
    bool m_keepBelow = false;
    bool m_demandsAttention = false;
    bool m_minimized = false;
    bool m_suspended = false;
    bool m_excludeFromCapture = false;
    QTimer *m_autoRaiseTimer = nullptr;
    QList<VirtualDesktop *> m_desktops;

    QStringList m_activityList;
    int m_activityUpdatesBlocked = 0;
    bool m_blockedActivityUpdatesRequireTransients = false;

    QString m_colorScheme;
    std::shared_ptr<Decoration::DecorationPalette> m_palette;
    static QHash<QString, std::weak_ptr<Decoration::DecorationPalette>> s_palettes;
    static std::shared_ptr<Decoration::DecorationPalette> s_defaultPalette;

    PlasmaWindowInterface *m_windowManagementInterface = nullptr;

    Window *m_transientFor = nullptr;
    QList<Window *> m_transients;
    bool m_modal = false;
    Layer m_layer = UnknownLayer;
    bool m_delayedRaise = false;
    QPointer<Tile> m_requestedTile;
    QPointer<Tile> m_tile;

    // electric border/quick tiling
    std::optional<ElectricBorderMode> m_electricMode = std::nullopt;
    RectF m_electricGeometryRestore;
    bool m_electricMaximizing = false;
    QTimer *m_electricMaximizingDelay = nullptr;

    // geometry
    LogicalOutput *m_moveResizeOutput = nullptr;
    RectF m_moveResizeGeometry;
    RectF m_keyboardGeometryRestore;
    RectF m_maximizeGeometryRestore;
    RectF m_fullscreenGeometryRestore;
    RectF m_virtualKeyboardGeometry;

    struct
    {
        bool enabled = false;
        bool unrestricted = false;
        QPointF anchor;
        Qt::KeyboardModifiers modifiers;
        QPointF offset;
        RectF initialGeometry;
        RectF initialGeometryRestore;
        Gravity gravity = Gravity::None;
        bool buttonDown = false;
        CursorShape cursor = Qt::ArrowCursor;
        QString initialOutputId;
        RectF initialScreenArea;
        QTimer *delayedTimer = nullptr;
        uint32_t counter = 0;
        MaximizeMode initialMaximizeMode;
        QuickTileMode initialQuickTileMode;
    } m_interactiveMoveResize;

    struct
    {
        std::shared_ptr<KDecoration3::Decoration> decoration;
        QPointer<Decoration::DecoratedWindowImpl> client;
        QElapsedTimer doubleClickTimer;
        Region inputRegion;
    } m_decoration;
    QString m_desktopFileName;

    bool m_applicationMenuActive = false;
    QString m_applicationMenuServiceName;
    QString m_applicationMenuObjectPath;

    bool m_unresponsive = false;

    QKeySequence _shortcut;

    WindowRules m_rules;
    quint32 m_lastUsageSerial = 0;
    bool m_lockScreenOverlay = false;
    uint32_t m_offscreenRenderCount = 0;
    QTimer m_offscreenFramecallbackTimer;

    QString m_tag;
    QString m_description;

    QString m_activationToken;
};

inline RectF Window::bufferGeometry() const
{
    return m_bufferGeometry;
}

inline RectF Window::clientGeometry() const
{
    return m_clientGeometry;
}

inline QSizeF Window::clientSize() const
{
    return m_clientGeometry.size();
}

inline RectF Window::frameGeometry() const
{
    return m_frameGeometry;
}

inline QSizeF Window::size() const
{
    return m_frameGeometry.size();
}

inline QPointF Window::pos() const
{
    return m_frameGeometry.topLeft();
}

inline qreal Window::x() const
{
    return m_frameGeometry.x();
}

inline qreal Window::y() const
{
    return m_frameGeometry.y();
}

inline qreal Window::width() const
{
    return m_frameGeometry.width();
}

inline qreal Window::height() const
{
    return m_frameGeometry.height();
}

inline RectF Window::rect() const
{
    return RectF(0, 0, width(), height());
}

inline bool Window::readyForPainting() const
{
    return ready_for_painting;
}

inline bool Window::isDesktop() const
{
    return windowType() == WindowType::Desktop;
}

inline bool Window::isDock() const
{
    return windowType() == WindowType::Dock;
}

inline bool Window::isMenu() const
{
    return windowType() == WindowType::Menu;
}

inline bool Window::isToolbar() const
{
    return windowType() == WindowType::Toolbar;
}

inline bool Window::isSplash() const
{
    return windowType() == WindowType::Splash;
}

inline bool Window::isUtility() const
{
    return windowType() == WindowType::Utility;
}

inline bool Window::isDialog() const
{
    return windowType() == WindowType::Dialog;
}

inline bool Window::isNormalWindow() const
{
    return windowType() == WindowType::Normal;
}

inline bool Window::isDropdownMenu() const
{
    return windowType() == WindowType::DropdownMenu;
}

inline bool Window::isPopupMenu() const
{
    return windowType() == WindowType::PopupMenu;
}

inline bool Window::isTooltip() const
{
    return windowType() == WindowType::Tooltip;
}

inline bool Window::isNotification() const
{
    return windowType() == WindowType::Notification;
}

inline bool Window::isCriticalNotification() const
{
    return windowType() == WindowType::CriticalNotification;
}

inline bool Window::isAppletPopup() const
{
    return windowType() == WindowType::AppletPopup;
}

inline bool Window::isOnScreenDisplay() const
{
    return windowType() == WindowType::OnScreenDisplay;
}

inline bool Window::isComboBox() const
{
    return windowType() == WindowType::ComboBox;
}

inline bool Window::isDNDIcon() const
{
    return windowType() == WindowType::DNDIcon;
}

inline bool Window::isLockScreen() const
{
    return false;
}

inline bool Window::isInputMethod() const
{
    return false;
}

inline bool Window::isOutline() const
{
    return false;
}

inline bool Window::isInternal() const
{
    return false;
}

inline bool Window::isPictureInPicture() const
{
    return false;
}

inline WindowItem *Window::windowItem() const
{
    return m_windowItem.get();
}

inline bool Window::isOnAllDesktops() const
{
    return desktops().isEmpty();
}

inline bool Window::isOnAllActivities() const
{
    return activities().isEmpty();
}

inline bool Window::isOnActivity(const QString &activity) const
{
    return activities().isEmpty() || activities().contains(activity);
}

inline QString Window::resourceName() const
{
    return resource_name; // it is always lowercase
}

inline QString Window::resourceClass() const
{
    return resource_class; // it is always lowercase
}

inline ClientMachine *Window::clientMachine() const
{
    return m_clientMachine;
}

template<class T, class U>
inline T *Window::findInList(const QList<T *> &list, std::function<bool(const U *)> func)
{
    static_assert(std::is_base_of<U, T>::value,
                  "U must be derived from T");
    const auto it = std::find_if(list.begin(), list.end(), func);
    if (it == list.end()) {
        return nullptr;
    }
    return *it;
}

inline bool Window::isPopupWindow() const
{
    switch (windowType()) {
    case WindowType::ComboBox:
    case WindowType::DropdownMenu:
    case WindowType::PopupMenu:
    case WindowType::Tooltip:
        return true;

    default:
        return false;
    }
}

inline const QList<Window *> &Window::transients() const
{
    return m_transients;
}

KWIN_EXPORT QDebug operator<<(QDebug debug, const Window *window);

} // namespace KWin

Q_DECLARE_METATYPE(KWin::Window *)
Q_DECLARE_METATYPE(QList<KWin::Window *>)
Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::Window::SameApplicationChecks)
