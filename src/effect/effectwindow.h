/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/output.h"
#include "kwin_export.h"

#include "globals.h"

#include <QObject>
#include <QWindow>

class QWindow;

namespace KDecoration3
{
class Decoration;
}

namespace KWin
{

class EffectWindowGroup;
class EffectWindowVisibleRef;
class Group;
class Output;
class SurfaceInterface;
class VirtualDesktop;
class Window;
class WindowItem;

/**
 * @short Representation of a window used by/for Effect classes.
 *
 * The purpose is to hide internal data and also to serve as a single
 *  representation for the case when Client/Unmanaged becomes Deleted.
 */
class KWIN_EXPORT EffectWindow : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QRectF geometry READ frameGeometry)
    Q_PROPERTY(QRectF expandedGeometry READ expandedGeometry)
    Q_PROPERTY(qreal height READ height)
    Q_PROPERTY(qreal opacity READ opacity)
    Q_PROPERTY(QPointF pos READ pos)
    Q_PROPERTY(KWin::Output *screen READ screen)
    Q_PROPERTY(QSizeF size READ size)
    Q_PROPERTY(qreal width READ width)
    Q_PROPERTY(qreal x READ x)
    Q_PROPERTY(qreal y READ y)
    Q_PROPERTY(QList<KWin::VirtualDesktop *> desktops READ desktops)
    Q_PROPERTY(bool onAllDesktops READ isOnAllDesktops)
    Q_PROPERTY(bool onCurrentDesktop READ isOnCurrentDesktop)
    Q_PROPERTY(QRectF rect READ rect)
    Q_PROPERTY(QString windowClass READ windowClass)
    Q_PROPERTY(QString windowRole READ windowRole)
    /**
     * Returns whether the window is a desktop background window (the one with wallpaper).
     * See _NET_WM_WINDOW_TYPE_DESKTOP at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool desktopWindow READ isDesktop)
    /**
     * Returns whether the window is a dock (i.e. a panel).
     * See _NET_WM_WINDOW_TYPE_DOCK at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool dock READ isDock)
    /**
     * Returns whether the window is a standalone (detached) toolbar window.
     * See _NET_WM_WINDOW_TYPE_TOOLBAR at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool toolbar READ isToolbar)
    /**
     * Returns whether the window is a torn-off menu.
     * See _NET_WM_WINDOW_TYPE_MENU at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool menu READ isMenu)
    /**
     * Returns whether the window is a "normal" window, i.e. an application or any other window
     * for which none of the specialized window types fit.
     * See _NET_WM_WINDOW_TYPE_NORMAL at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool normalWindow READ isNormalWindow)
    /**
     * Returns whether the window is a dialog window.
     * See _NET_WM_WINDOW_TYPE_DIALOG at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool dialog READ isDialog)
    /**
     * Returns whether the window is a splashscreen. Note that many (especially older) applications
     * do not support marking their splash windows with this type.
     * See _NET_WM_WINDOW_TYPE_SPLASH at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool splash READ isSplash)
    /**
     * Returns whether the window is a utility window, such as a tool window.
     * See _NET_WM_WINDOW_TYPE_UTILITY at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool utility READ isUtility)
    /**
     * Returns whether the window is a dropdown menu (i.e. a popup directly or indirectly open
     * from the applications menubar).
     * See _NET_WM_WINDOW_TYPE_DROPDOWN_MENU at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool dropdownMenu READ isDropdownMenu)
    /**
     * Returns whether the window is a popup menu (that is not a torn-off or dropdown menu).
     * See _NET_WM_WINDOW_TYPE_POPUP_MENU at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool popupMenu READ isPopupMenu)
    /**
     * Returns whether the window is a tooltip.
     * See _NET_WM_WINDOW_TYPE_TOOLTIP at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool tooltip READ isTooltip)
    /**
     * Returns whether the window is a window with a notification.
     * See _NET_WM_WINDOW_TYPE_NOTIFICATION at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool notification READ isNotification)
    /**
     * Returns whether the window is a window with a critical notification.
     * using the non-standard _KDE_NET_WM_WINDOW_TYPE_CRITICAL_NOTIFICATION
     */
    Q_PROPERTY(bool criticalNotification READ isCriticalNotification)
    /**
     * Returns whether the window is an on screen display window
     * using the non-standard _KDE_NET_WM_WINDOW_TYPE_ON_SCREEN_DISPLAY
     */
    Q_PROPERTY(bool onScreenDisplay READ isOnScreenDisplay)
    /**
     * Returns whether the window is a combobox popup.
     * See _NET_WM_WINDOW_TYPE_COMBO at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool comboBox READ isComboBox)
    /**
     * Returns whether the window is a Drag&Drop icon.
     * See _NET_WM_WINDOW_TYPE_DND at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(bool dndIcon READ isDNDIcon)
    /**
     * Returns the NETWM window type
     * See https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(int windowType READ windowTypeInt)
    /**
     * Whether this EffectWindow is managed by KWin (it has control over its placement and other
     * aspects, as opposed to override-redirect windows that are entirely handled by the application).
     */
    Q_PROPERTY(bool managed READ isManaged)
    /**
     * Whether this EffectWindow represents an already deleted window and only kept for the compositor for animations.
     */
    Q_PROPERTY(bool deleted READ isDeleted)
    /**
     * The Caption of the window. Read from WM_NAME property together with a suffix for hostname and shortcut.
     */
    Q_PROPERTY(QString caption READ caption)
    /**
     * Whether the window is set to be kept above other windows.
     */
    Q_PROPERTY(bool keepAbove READ keepAbove)
    /**
     * Whether the window is set to be kept below other windows.
     */
    Q_PROPERTY(bool keepBelow READ keepBelow)
    /**
     * Whether the window is minimized.
     */
    Q_PROPERTY(bool minimized READ isMinimized WRITE setMinimized)
    /**
     * Whether the window represents a modal window.
     */
    Q_PROPERTY(bool modal READ isModal)
    /**
     * Whether the window is moveable. Even if it is not moveable, it might be possible to move
     * it to another screen.
     * @see moveableAcrossScreens
     */
    Q_PROPERTY(bool moveable READ isMovable)
    /**
     * Whether the window can be moved to another screen.
     * @see moveable
     */
    Q_PROPERTY(bool moveableAcrossScreens READ isMovableAcrossScreens)
    /**
     * By how much the window wishes to grow/shrink at least. Usually QSize(1,1).
     * MAY BE DISOBEYED BY THE WM! It's only for information, do NOT rely on it at all.
     */
    Q_PROPERTY(QSizeF basicUnit READ basicUnit)
    /**
     * Whether the window is currently being moved by the user.
     */
    Q_PROPERTY(bool move READ isUserMove)
    /**
     * Whether the window is currently being resized by the user.
     */
    Q_PROPERTY(bool resize READ isUserResize)
    /**
     * The optional geometry representing the minimized Client in e.g a taskbar.
     * See _NET_WM_ICON_GEOMETRY at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    Q_PROPERTY(QRectF iconGeometry READ iconGeometry)
    /**
     * Returns whether the window is any of special windows types (desktop, dock, splash, ...),
     * i.e. window types that usually don't have a window frame and the user does not use window
     * management (moving, raising,...) on them.
     */
    Q_PROPERTY(bool specialWindow READ isSpecialWindow)
    Q_PROPERTY(QIcon icon READ icon)
    /**
     * Whether the window should be excluded from window switching effects.
     */
    Q_PROPERTY(bool skipSwitcher READ isSkipSwitcher)
    /**
     * Geometry of the actual window contents inside the whole (including decorations) window.
     */
    Q_PROPERTY(QRectF contentsRect READ contentsRect)
    Q_PROPERTY(bool hasDecoration READ hasDecoration)
    Q_PROPERTY(QStringList activities READ activities)
    Q_PROPERTY(bool onCurrentActivity READ isOnCurrentActivity)
    Q_PROPERTY(bool onAllActivities READ isOnAllActivities)
    /**
     * Whether the decoration currently uses an alpha channel.
     * @since 4.10
     */
    Q_PROPERTY(bool decorationHasAlpha READ decorationHasAlpha)
    /**
     * Whether the window is currently visible to the user, that is:
     * <ul>
     * <li>Not minimized</li>
     * <li>On current desktop</li>
     * <li>On current activity</li>
     * </ul>
     * @since 4.11
     */
    Q_PROPERTY(bool visible READ isVisible)
    /**
     * Whether the window does not want to be animated on window close.
     * In case this property is @c true it is not useful to start an animation on window close.
     * The window will not be visible, but the animation hooks are executed.
     * @since 5.0
     */
    Q_PROPERTY(bool skipsCloseAnimation READ skipsCloseAnimation)

    /**
     * Whether the window is fullscreen.
     * @since 5.6
     */
    Q_PROPERTY(bool fullScreen READ isFullScreen)

    /**
     * Whether this client is unresponsive.
     *
     * When an application failed to react on a ping request in time, it is
     * considered unresponsive. This usually indicates that the application froze or crashed.
     *
     * @since 5.10
     */
    Q_PROPERTY(bool unresponsive READ isUnresponsive)

    /**
     * Whether this is a Wayland client.
     * @since 5.15
     */
    Q_PROPERTY(bool waylandClient READ isWaylandClient CONSTANT)

    /**
     * Whether this is an X11 client.
     * @since 5.15
     */
    Q_PROPERTY(bool x11Client READ isX11Client CONSTANT)

    /**
     * Whether the window is a popup.
     *
     * A popup is a window that can be used to implement tooltips, combo box popups,
     * popup menus and other similar user interface concepts.
     *
     * @since 5.15
     */
    Q_PROPERTY(bool popupWindow READ isPopupWindow CONSTANT)

    /**
     * KWin internal window. Specific to Wayland platform.
     *
     * If the EffectWindow does not reference an internal window, this property is @c null.
     * @since 5.16
     */
    Q_PROPERTY(QWindow *internalWindow READ internalWindow CONSTANT)

    /**
     * Whether this EffectWindow represents the outline.
     *
     * When compositing is turned on, the outline is an actual window.
     *
     * @since 5.16
     */
    Q_PROPERTY(bool outline READ isOutline CONSTANT)

    /**
     * The PID of the application this window belongs to.
     *
     * @since 5.18
     */
    Q_PROPERTY(pid_t pid READ pid CONSTANT)

    /**
     * Whether this EffectWindow represents the screenlocker greeter.
     *
     * @since 5.22
     */
    Q_PROPERTY(bool lockScreen READ isLockScreen CONSTANT)

    /**
     * Whether the window is an applet popup.
     *
     * An applet popup is created by an applet to show its
     * fullRepresentation.
     *
     * @since 6.3
     */
    Q_PROPERTY(bool appletPopup READ isAppletPopup CONSTANT)

    /**
     * Whether this EffectWindow is hidden because the show desktop mode is active.
     */
    Q_PROPERTY(bool hiddenByShowDesktop READ isHiddenByShowDesktop)

public:
    /**  Flags explaining why painting should be disabled  */
    enum {
        /**  Window will not be painted  */
        PAINT_DISABLED = 1 << 0,
        /**  Window will not be painted because of which desktop it's on  */
        PAINT_DISABLED_BY_DESKTOP = 1 << 1,
        /**  Window will not be painted because it is minimized  */
        PAINT_DISABLED_BY_MINIMIZE = 1 << 2,
        /**  Window will not be painted because it's not on the current activity  */
        PAINT_DISABLED_BY_ACTIVITY = 1 << 3,
    };

    explicit EffectWindow(WindowItem *windowItem);
    ~EffectWindow() override;

    Q_SCRIPTABLE void addRepaint(const QRect &r);
    Q_SCRIPTABLE void addRepaint(int x, int y, int w, int h);
    Q_SCRIPTABLE void addRepaintFull();
    Q_SCRIPTABLE void addLayerRepaint(const QRect &r);
    Q_SCRIPTABLE void addLayerRepaint(int x, int y, int w, int h);

    void refWindow();
    void unrefWindow();

    bool isDeleted() const;
    bool isHidden() const;
    bool isHiddenByShowDesktop() const;

    bool isMinimized() const;
    double opacity() const;

    bool isOnCurrentActivity() const;
    Q_SCRIPTABLE bool isOnActivity(const QString &id) const;
    bool isOnAllActivities() const;
    QStringList activities() const;

    Q_SCRIPTABLE bool isOnDesktop(KWin::VirtualDesktop *desktop) const;
    bool isOnCurrentDesktop() const;
    bool isOnAllDesktops() const;
    /**
     * All the desktops by number that the window is in. On X11 this list will always have
     * a length of 1, on Wayland can be any subset.
     * If the list is empty it means the window is on all desktops
     */
    QList<KWin::VirtualDesktop *> desktops() const;

    qreal x() const;
    qreal y() const;
    qreal width() const;
    qreal height() const;
    /**
     * By how much the window wishes to grow/shrink at least. Usually QSize(1,1).
     * MAY BE DISOBEYED BY THE WM! It's only for information, do NOT rely on it at all.
     */
    QSizeF basicUnit() const;
    /**
     * Returns the geometry of the window excluding server-side and client-side
     * drop-shadows.
     *
     * @since 5.18
     */
    QRectF frameGeometry() const;
    /**
     * Returns the geometry of the pixmap or buffer attached to this window.
     *
     * For X11 clients, this method returns server-side geometry of the Window.
     *
     * For Wayland clients, this method returns rectangle that the main surface
     * occupies on the screen, in global screen coordinates.
     *
     * @since 5.18
     */
    QRectF bufferGeometry() const;
    QRectF clientGeometry() const;
    /**
     * Geometry of the window including decoration and potentially shadows.
     * May be different from geometry() if the window has a shadow.
     * @since 4.9
     */
    QRectF expandedGeometry() const;
    Output *screen() const;
    QPointF pos() const;
    QSizeF size() const;
    QRectF rect() const;
    bool isMovable() const;
    bool isMovableAcrossScreens() const;
    bool isUserMove() const;
    bool isUserResize() const;
    QRectF iconGeometry() const;

    /**
     * Geometry of the actual window contents inside the whole (including decorations) window.
     */
    QRectF contentsRect() const;
    bool hasDecoration() const;
    bool decorationHasAlpha() const;
    /**
     * Returns the decoration
     * @since 5.25
     */
    KDecoration3::Decoration *decoration() const;
    QByteArray readProperty(long atom, long type, int format) const;
    void deleteProperty(long atom) const;

    QString caption() const;
    QIcon icon() const;
    QString windowClass() const;
    QString windowRole() const;
    const EffectWindowGroup *group() const;

    /**
     * Returns whether the window is a desktop background window (the one with wallpaper).
     * See _NET_WM_WINDOW_TYPE_DESKTOP at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isDesktop() const;
    /**
     * Returns whether the window is a dock (i.e. a panel).
     * See _NET_WM_WINDOW_TYPE_DOCK at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isDock() const;
    /**
     * Returns whether the window is a standalone (detached) toolbar window.
     * See _NET_WM_WINDOW_TYPE_TOOLBAR at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isToolbar() const;
    /**
     * Returns whether the window is a torn-off menu.
     * See _NET_WM_WINDOW_TYPE_MENU at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isMenu() const;
    /**
     * Returns whether the window is a "normal" window, i.e. an application or any other window
     * for which none of the specialized window types fit.
     * See _NET_WM_WINDOW_TYPE_NORMAL at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isNormalWindow() const; // normal as in 'NET::Normal or NET::Unknown non-transient'
    /**
     * Returns whether the window is any of special windows types (desktop, dock, splash, ...),
     * i.e. window types that usually don't have a window frame and the user does not use window
     * management (moving, raising,...) on them.
     */
    bool isSpecialWindow() const;
    /**
     * Returns whether the window is a dialog window.
     * See _NET_WM_WINDOW_TYPE_DIALOG at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isDialog() const;
    /**
     * Returns whether the window is a splashscreen. Note that many (especially older) applications
     * do not support marking their splash windows with this type.
     * See _NET_WM_WINDOW_TYPE_SPLASH at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isSplash() const;
    /**
     * Returns whether the window is a utility window, such as a tool window.
     * See _NET_WM_WINDOW_TYPE_UTILITY at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isUtility() const;
    /**
     * Returns whether the window is a dropdown menu (i.e. a popup directly or indirectly open
     * from the applications menubar).
     * See _NET_WM_WINDOW_TYPE_DROPDOWN_MENU at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isDropdownMenu() const;
    /**
     * Returns whether the window is a popup menu (that is not a torn-off or dropdown menu).
     * See _NET_WM_WINDOW_TYPE_POPUP_MENU at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isPopupMenu() const; // a context popup, not dropdown, not torn-off
    /**
     * Returns whether the window is a tooltip.
     * See _NET_WM_WINDOW_TYPE_TOOLTIP at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isTooltip() const;
    /**
     * Returns whether the window is a window with a notification.
     * See _NET_WM_WINDOW_TYPE_NOTIFICATION at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isNotification() const;
    /**
     * Returns whether the window is a window with a critical notification.
     * using the non-standard _KDE_NET_WM_WINDOW_TYPE_CRITICAL_NOTIFICATION
     */
    bool isCriticalNotification() const;
    /**
     * Returns whether the window is a window used for applet popups.
     */
    bool isAppletPopup() const;
    /**
     * Returns whether the window is an on screen display window
     * using the non-standard _KDE_NET_WM_WINDOW_TYPE_ON_SCREEN_DISPLAY
     */
    bool isOnScreenDisplay() const;
    /**
     * Returns whether the window is a combobox popup.
     * See _NET_WM_WINDOW_TYPE_COMBO at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isComboBox() const;
    /**
     * Returns whether the window is a Drag&Drop icon.
     * See _NET_WM_WINDOW_TYPE_DND at https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    bool isDNDIcon() const;
    /**
     * Returns the NETWM window type
     * See https://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     */
    WindowType windowType() const;
    int windowTypeInt() const
    {
        return int(windowType());
    }
    /**
     * Returns whether the window is managed by KWin (it has control over its placement and other
     * aspects, as opposed to override-redirect windows that are entirely handled by the application).
     */
    bool isManaged() const; // whether it's managed or override-redirect
    /**
     * Returns whether or not the window can accept keyboard focus.
     */
    bool acceptsFocus() const;
    /**
     * Returns whether or not the window is kept above all other windows.
     */
    bool keepAbove() const;
    /**
     * Returns whether the window is kept below all other windows.
     */
    bool keepBelow() const;

    bool isModal() const;
    Q_SCRIPTABLE KWin::EffectWindow *findModal();
    Q_SCRIPTABLE KWin::EffectWindow *transientFor();
    Q_SCRIPTABLE QList<KWin::EffectWindow *> mainWindows() const;

    /**
     * Returns whether the window should be excluded from window switching effects.
     * @since 4.5
     */
    bool isSkipSwitcher() const;

    void setMinimized(bool minimize);
    void minimize();
    void unminimize();
    Q_SCRIPTABLE void closeWindow();

    /**
     * @since 4.11
     */
    bool isVisible() const;

    /**
     * @since 5.0
     */
    bool skipsCloseAnimation() const;

    /**
     * @since 5.5
     */
    SurfaceInterface *surface() const;

    /**
     * @since 5.6
     */
    bool isFullScreen() const;

    /**
     * @since 5.10
     */
    bool isUnresponsive() const;

    /**
     * @since 5.15
     */
    bool isWaylandClient() const;

    /**
     * @since 5.15
     */
    bool isX11Client() const;

    /**
     * @since 5.15
     */
    bool isPopupWindow() const;

    /**
     * @since 5.16
     */
    QWindow *internalWindow() const;

    /**
     * @since 5.16
     */
    bool isOutline() const;

    /**
     * @since 5.22
     */
    bool isLockScreen() const;

    /**
     * @since 5.18
     */
    pid_t pid() const;

    /**
     * @since 5.21
     */
    qlonglong windowId() const;
    /**
     * Returns the internal id of the window that uniquely identifies it. The main difference
     * between internalId() and windowId() is that the latter one works as expected only on X11,
     * while the former is unique regardless of the window system.
     *
     * Note that the internaId() has special meaning only to kwin.
     * @since 5.24
     */
    QUuid internalId() const;

    /**
     * @since 6.0
     */
    bool isInputMethod() const;

    /**
     * Can be used to by effects to store arbitrary data in the EffectWindow.
     *
     * Invoking this method will emit the signal EffectsHandler::windowDataChanged.
     * @see EffectsHandler::windowDataChanged
     */
    Q_SCRIPTABLE void setData(int role, const QVariant &data);
    Q_SCRIPTABLE QVariant data(int role) const;

    Window *window() const;
    WindowItem *windowItem() const;
    void elevate(bool elevate);

Q_SIGNALS:
    /**
     * Signal emitted when a user begins a window move or resize operation.
     * To figure out whether the user resizes or moves the window use
     * isUserMove or isUserResize.
     * Whenever the geometry is updated the signal @ref windowStepUserMovedResized
     * is emitted with the current geometry.
     * The move/resize operation ends with the signal @ref windowFinishUserMovedResized.
     * Only one window can be moved/resized by the user at the same time!
     * @param w The window which is being moved/resized
     * @see windowStepUserMovedResized
     * @see windowFinishUserMovedResized
     * @see EffectWindow::isUserMove
     * @see EffectWindow::isUserResize
     */
    void windowStartUserMovedResized(KWin::EffectWindow *w);
    /**
     * Signal emitted during a move/resize operation when the user changed the geometry.
     * Please note: KWin supports two operation modes. In one mode all changes are applied
     * instantly. This means the window's geometry matches the passed in @p geometry. In the
     * other mode the geometry is changed after the user ended the move/resize mode.
     * The @p geometry differs from the window's geometry. Also the window's pixmap still has
     * the same size as before. Depending what the effect wants to do it would be recommended
     * to scale/translate the window.
     * @param w The window which is being moved/resized
     * @param geometry The geometry of the window in the current move/resize step.
     * @see windowStartUserMovedResized
     * @see windowFinishUserMovedResized
     * @see EffectWindow::isUserMove
     * @see EffectWindow::isUserResize
     */
    void windowStepUserMovedResized(KWin::EffectWindow *w, const QRectF &geometry);
    /**
     * Signal emitted when the user finishes move/resize of window @p w.
     * @param w The window which has been moved/resized
     * @see windowStartUserMovedResized
     * @see windowFinishUserMovedResized
     */
    void windowFinishUserMovedResized(KWin::EffectWindow *w);

    /**
     * Signal emitted when the maximized state of the window @p w changed.
     * A window can be in one of four states:
     * @li restored: both @p horizontal and @p vertical are @c false
     * @li horizontally maximized: @p horizontal is @c true and @p vertical is @c false
     * @li vertically maximized: @p horizontal is @c false and @p vertical is @c true
     * @li completely maximized: both @p horizontal and @p vertical are @c true
     * @param w The window whose maximized state changed
     * @param horizontal If @c true maximized horizontally
     * @param vertical If @c true maximized vertically
     */
    void windowMaximizedStateChanged(KWin::EffectWindow *w, bool horizontal, bool vertical);

    /**
     * Signal emitted when the maximized state of the window @p w is about to change,
     * but before windowMaximizedStateChanged is emitted or any geometry change.
     * Useful for OffscreenEffect to grab a window image before any actual change happens
     *
     * A window can be in one of four states:
     * @li restored: both @p horizontal and @p vertical are @c false
     * @li horizontally maximized: @p horizontal is @c true and @p vertical is @c false
     * @li vertically maximized: @p horizontal is @c false and @p vertical is @c true
     * @li completely maximized: both @p horizontal and @p vertical are @c true
     * @param w The window whose maximized state changed
     * @param horizontal If @c true maximized horizontally
     * @param vertical If @c true maximized vertically
     */
    void windowMaximizedStateAboutToChange(KWin::EffectWindow *w, bool horizontal, bool vertical);

    /**
     * This signal is emitted when the frame geometry of a window changed.
     * @param window The window whose geometry changed
     * @param oldGeometry The previous geometry
     */
    void windowFrameGeometryChanged(KWin::EffectWindow *window, const QRectF &oldGeometry);

    /**
     * This signal is emitted when the frame geometry is about to change, the new one is not known yet.
     * Useful for OffscreenEffect to grab a window image before any actual change happens.
     *
     * @param window The window whose geometry is about to change
     */
    void windowFrameGeometryAboutToChange(KWin::EffectWindow *window);

    /**
     * Signal emitted when the windows opacity is changed.
     * @param w The window whose opacity level is changed.
     * @param oldOpacity The previous opacity level
     * @param newOpacity The new opacity level
     */
    void windowOpacityChanged(KWin::EffectWindow *w, qreal oldOpacity, qreal newOpacity);
    /**
     * Signal emitted when a window is minimized or unminimized.
     * @param w The window whose minimized state has changed
     */
    void minimizedChanged(KWin::EffectWindow *w);
    /**
     * Signal emitted when a window either becomes modal (ie. blocking for its main client) or looses that state.
     * @param w The window which was unminimized
     */
    void windowModalityChanged(KWin::EffectWindow *w);
    /**
     * Signal emitted when a window either became unresponsive (eg. app froze or crashed)
     * or respoonsive
     * @param w The window that became (un)responsive
     * @param unresponsive Whether the window is responsive or unresponsive
     */
    void windowUnresponsiveChanged(KWin::EffectWindow *w, bool unresponsive);
    /**
     * Signal emitted when an area of a window is scheduled for repainting.
     * Use this signal in an effect if another area needs to be synced as well.
     * @param w The window which is scheduled for repainting
     */
    void windowDamaged(KWin::EffectWindow *w);

    /**
     * This signal is emitted when the keep above state of @p w was changed.
     *
     * @param w The window whose the keep above state was changed.
     */
    void windowKeepAboveChanged(KWin::EffectWindow *w);

    /**
     * This signal is emitted when the keep below state of @p was changed.
     *
     * @param w The window whose the keep below state was changed.
     */
    void windowKeepBelowChanged(KWin::EffectWindow *w);

    /**
     * This signal is emitted when the full screen state of @p w was changed.
     *
     * @param w The window whose the full screen state was changed.
     */
    void windowFullScreenChanged(KWin::EffectWindow *w);

    /**
     * This signal is emitted when decoration of @p was changed.
     *
     * @param w The window for which decoration changed
     */
    void windowDecorationChanged(KWin::EffectWindow *window);

    /**
     * This signal is emitted when the visible geometry of a window changed.
     */
    void windowExpandedGeometryChanged(KWin::EffectWindow *window);

    /**
     * This signal is emitted when a window enters or leaves a virtual desktop.
     */
    void windowDesktopsChanged(KWin::EffectWindow *window);

    /**
     * This signal is emitted when a window is hidden or shown.
     */
    void windowHiddenChanged(KWin::EffectWindow *window);

protected:
    friend EffectWindowVisibleRef;
    void refVisible(const EffectWindowVisibleRef *holder);
    void unrefVisible(const EffectWindowVisibleRef *holder);

private:
    class Private;
    std::unique_ptr<Private> d;
};

/**
 * The EffectWindowDeletedRef provides a convenient way to prevent deleting a closed
 * window until an effect has finished animating it.
 */
class KWIN_EXPORT EffectWindowDeletedRef
{
public:
    EffectWindowDeletedRef()
        : m_window(nullptr)
    {
    }

    explicit EffectWindowDeletedRef(EffectWindow *window)
        : m_window(window)
    {
        m_window->refWindow();
    }

    EffectWindowDeletedRef(const EffectWindowDeletedRef &other)
        : m_window(other.m_window)
    {
        if (m_window) {
            m_window->refWindow();
        }
    }

    ~EffectWindowDeletedRef()
    {
        if (m_window) {
            m_window->unrefWindow();
        }
    }

    EffectWindowDeletedRef &operator=(const EffectWindowDeletedRef &other)
    {
        if (other.m_window) {
            other.m_window->refWindow();
        }
        if (m_window) {
            m_window->unrefWindow();
        }
        m_window = other.m_window;
        return *this;
    }

    bool isNull() const
    {
        return m_window == nullptr;
    }

private:
    EffectWindow *m_window;
};

/**
 * The EffectWindowVisibleRef provides a convenient way to force the visible status of a
 * window until an effect is finished animating it.
 */
class KWIN_EXPORT EffectWindowVisibleRef
{
public:
    EffectWindowVisibleRef()
        : m_window(nullptr)
        , m_reason(0)
    {
    }

    explicit EffectWindowVisibleRef(EffectWindow *window, int reason)
        : m_window(window)
        , m_reason(reason)
    {
        m_window->refVisible(this);
    }

    EffectWindowVisibleRef(const EffectWindowVisibleRef &other)
        : m_window(other.m_window)
        , m_reason(other.m_reason)
    {
        if (m_window) {
            m_window->refVisible(this);
        }
    }

    ~EffectWindowVisibleRef()
    {
        if (m_window) {
            m_window->unrefVisible(this);
        }
    }

    int reason() const
    {
        return m_reason;
    }

    EffectWindowVisibleRef &operator=(const EffectWindowVisibleRef &other)
    {
        if (other.m_window) {
            other.m_window->refVisible(&other);
        }
        if (m_window) {
            m_window->unrefVisible(this);
        }
        m_window = other.m_window;
        m_reason = other.m_reason;
        return *this;
    }

    bool isNull() const
    {
        return m_window == nullptr;
    }

private:
    EffectWindow *m_window;
    int m_reason;
};

class KWIN_EXPORT EffectWindowGroup
{
public:
    explicit EffectWindowGroup(Group *group);
    virtual ~EffectWindowGroup();

    QList<EffectWindow *> members() const;

private:
    Group *m_group;
};

inline void EffectWindow::addRepaint(int x, int y, int w, int h)
{
    addRepaint(QRect(x, y, w, h));
}

inline void EffectWindow::addLayerRepaint(int x, int y, int w, int h)
{
    addLayerRepaint(QRect(x, y, w, h));
}

} // namespace KWin
