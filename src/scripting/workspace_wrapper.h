/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Rohan Prabhu <rohan@rohanprabhu.com>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/globals.h"
#include <QObject>
#include <QQmlListProperty>
#include <QRect>
#include <QSize>
#include <QStringList>

namespace KWin
{
// forward declarations
class TileManager;
class Window;
class Output;
class VirtualDesktop;

class WorkspaceWrapper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QList<KWin::VirtualDesktop *> desktops READ desktops NOTIFY desktopsChanged)
    Q_PROPERTY(KWin::VirtualDesktop *currentDesktop READ currentDesktop WRITE setCurrentDesktop NOTIFY currentDesktopChanged)
    Q_PROPERTY(KWin::Window *activeWindow READ activeWindow WRITE setActiveWindow NOTIFY windowActivated)
    // TODO: write and notify?
    Q_PROPERTY(QSize desktopGridSize READ desktopGridSize NOTIFY desktopLayoutChanged)
    Q_PROPERTY(int desktopGridWidth READ desktopGridWidth NOTIFY desktopLayoutChanged)
    Q_PROPERTY(int desktopGridHeight READ desktopGridHeight NOTIFY desktopLayoutChanged)
    Q_PROPERTY(int workspaceWidth READ workspaceWidth)
    Q_PROPERTY(int workspaceHeight READ workspaceHeight)
    Q_PROPERTY(QSize workspaceSize READ workspaceSize)
    Q_PROPERTY(KWin::Output *activeScreen READ activeScreen)
    Q_PROPERTY(QList<KWin::Output *> screens READ screens NOTIFY screensChanged)
    Q_PROPERTY(QList<KWin::Output *> screenOrder READ screenOrder NOTIFY screenOrderChanged)
    Q_PROPERTY(QString currentActivity READ currentActivity WRITE setCurrentActivity NOTIFY currentActivityChanged)
    Q_PROPERTY(QStringList activities READ activityList NOTIFY activitiesChanged)

    /**
     * The bounding size of all screens combined. Overlapping areas
     * are not counted multiple times.
     * @see virtualScreenGeometry
     */
    Q_PROPERTY(QSize virtualScreenSize READ virtualScreenSize NOTIFY virtualScreenSizeChanged)

    /**
     * The bounding geometry of all screens combined. Always starts at (0,0) and has
     * virtualScreenSize as it's size.
     * @see virtualScreenSize
     */
    Q_PROPERTY(QRect virtualScreenGeometry READ virtualScreenGeometry NOTIFY virtualScreenGeometryChanged)

    /**
     * List of Clients currently managed by KWin, orderd by
     * their visibility (later ones cover earlier ones).
     */
    Q_PROPERTY(QList<KWin::Window *> stackingOrder READ stackingOrder)

    /**
     * The current position of the cursor.
     */
    Q_PROPERTY(QPoint cursorPos READ cursorPos NOTIFY cursorPosChanged)

private:
    Q_DISABLE_COPY(WorkspaceWrapper)

Q_SIGNALS:
    void windowAdded(KWin::Window *window);
    void windowRemoved(KWin::Window *window);
    void windowActivated(KWin::Window *window);

    /**
     * This signal is emitted when a virtual desktop is added or removed.
     */
    void desktopsChanged();

    /**
     * Signal emitted whenever the layout of virtual desktops changed.
     * That is desktopGrid(Size/Width/Height) will have new values.
     * @since 4.11
     */
    void desktopLayoutChanged();

    /**
     * Emitted when the output list changes, e.g. an output is connected or removed.
     */
    void screensChanged();

    /**
     * Emitted when the output order list changes, e.g. the primary output changes.
     */
    void screenOrderChanged();

    /**
     * Signal emitted whenever the current activity changed.
     * @param id id of the new activity
     */
    void currentActivityChanged(const QString &id);

    /**
     * Signal emitted whenever the list of activities changed.
     * @param id id of the new activity
     */
    void activitiesChanged(const QString &id);

    /**
     * This signal is emitted when a new activity is added
     * @param id id of the new activity
     */
    void activityAdded(const QString &id);

    /**
     * This signal is emitted when the activity
     * is removed
     * @param id id of the removed activity
     */
    void activityRemoved(const QString &id);

    /**
     * Emitted whenever the virtualScreenSize changes.
     * @see virtualScreenSize()
     * @since 5.0
     */
    void virtualScreenSizeChanged();

    /**
     * Emitted whenever the virtualScreenGeometry changes.
     * @see virtualScreenGeometry()
     * @since 5.0
     */
    void virtualScreenGeometryChanged();

    /**
     * This signal is emitted when the current virtual desktop changes.
     */
    void currentDesktopChanged(KWin::VirtualDesktop *previous);

    /**
     * This signal is emitted when the cursor position changes.
     * @see cursorPos()
     */
    void cursorPosChanged();

public:
    //------------------------------------------------------------------
    // enums copy&pasted from kwinglobals.h because qtscript is evil

    enum ClientAreaOption {
        ///< geometry where a window will be initially placed after being mapped
        PlacementArea,
        ///< window movement snapping area?  ignore struts
        MovementArea,
        ///< geometry to which a window will be maximized
        MaximizeArea,
        ///< like MaximizeArea, but ignore struts - used e.g. for topmenu
        MaximizeFullArea,
        ///< area for fullscreen windows
        FullScreenArea,
        ///< whole workarea (all screens together)
        WorkArea,
        ///< whole area (all screens together), ignore struts
        FullArea,
        ///< one whole screen, ignore struts
        ScreenArea
    };
    Q_ENUM(ClientAreaOption)
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
    Q_ENUM(ElectricBorder)

protected:
    explicit WorkspaceWrapper(QObject *parent = nullptr);

public:
    Window *activeWindow() const;
    void setActiveWindow(Window *window);

    QString currentActivity() const;
    void setCurrentActivity(const QString &activity);

    QSize desktopGridSize() const;
    int desktopGridWidth() const;
    int desktopGridHeight() const;
    int workspaceWidth() const;
    int workspaceHeight() const;
    QSize workspaceSize() const;
    KWin::Output *activeScreen() const;
    QList<KWin::Output *> screens() const;
    QList<KWin::Output *> screenOrder() const;
    QStringList activityList() const;
    QSize virtualScreenSize() const;
    QRect virtualScreenGeometry() const;
    QPoint cursorPos() const;

    QList<VirtualDesktop *> desktops() const;
    VirtualDesktop *currentDesktop() const;
    void setCurrentDesktop(VirtualDesktop *desktop);

    Q_INVOKABLE KWin::Output *screenAt(const QPointF &pos) const;

    Q_INVOKABLE KWin::TileManager *tilingForScreen(const QString &screenName) const;
    Q_INVOKABLE KWin::TileManager *tilingForScreen(KWin::Output *output) const;

    /**
     * Returns the geometry a Client can use with the specified option.
     * This method should be preferred over other methods providing screen sizes as the
     * various options take constraints such as struts set on panels into account.
     * This method is also multi screen aware, but there are also options to get full areas.
     * @param option The type of area which should be considered
     * @param screen The screen for which the area should be considered
     * @param desktop The desktop for which the area should be considered, in general there should not be a difference
     * @returns The specified screen geometry
     */
    Q_SCRIPTABLE QRectF clientArea(ClientAreaOption option, KWin::Output *output, KWin::VirtualDesktop *desktop) const;

    /**
     * Overloaded method for convenience.
     * @param client The Client for which the area should be retrieved
     * @returns The specified screen geometry
     */
    Q_SCRIPTABLE QRectF clientArea(ClientAreaOption option, KWin::Window *client) const;
    Q_SCRIPTABLE QRectF clientArea(ClientAreaOption option, const KWin::Window *client) const;

    /**
     * Create a new virtual desktop at the requested position.
     * @param position The position of the desktop. It should be in range [0, count].
     * @param name The name for the new desktop, if empty the default name will be used.
     */
    Q_SCRIPTABLE void createDesktop(int position, const QString &name) const;

    /**
     * Removes the specified virtual desktop.
     */
    Q_SCRIPTABLE void removeDesktop(KWin::VirtualDesktop *desktop) const;

    /**
     * Provides support information about the currently running KWin instance.
     */
    Q_SCRIPTABLE QString supportInformation() const;

    /**
     * List of Clients currently managed by KWin, orderd by
     * their visibility (later ones cover earlier ones).
     */
    QList<KWin::Window *> stackingOrder() const;

    /**
     * Raises a Window  above all others on the screen.
     * @param window The Window to raise
     */
    Q_INVOKABLE void raiseWindow(KWin::Window *window);

#if KWIN_BUILD_X11
    /**
     * Finds the Client with the given @p windowId.
     * @param windowId The window Id of the Client
     * @return The found Client or @c null
     */
    Q_SCRIPTABLE KWin::Window *getClient(qulonglong windowId);
#endif

    /**
     * Finds up to count windows at a particular location,
     * prioritizing the topmost one first.  A negative count
     * returns all matching clients.
     * @param pos The location to look for
     * @param count The number of clients to return
     * @return A list of Client objects
     */
    Q_INVOKABLE QList<KWin::Window *> windowAt(const QPointF &pos, int count = 1) const;

    /**
     * Checks if a specific effect is currently active.
     * @param pluginId The plugin Id of the effect to check.
     * @return @c true if the effect is loaded and currently active, @c false otherwise.
     * @since 6.0
     */
    Q_INVOKABLE bool isEffectActive(const QString &pluginId) const;

public Q_SLOTS:
    // all the available key bindings
    void slotSwitchDesktopNext();
    void slotSwitchDesktopPrevious();
    void slotSwitchDesktopRight();
    void slotSwitchDesktopLeft();
    void slotSwitchDesktopUp();
    void slotSwitchDesktopDown();

    void slotSwitchToNextScreen();
    void slotSwitchToPrevScreen();
    void slotSwitchToRightScreen();
    void slotSwitchToLeftScreen();
    void slotSwitchToAboveScreen();
    void slotSwitchToBelowScreen();
    void slotWindowToNextScreen();
    void slotWindowToPrevScreen();
    void slotWindowToRightScreen();
    void slotWindowToLeftScreen();
    void slotWindowToAboveScreen();
    void slotWindowToBelowScreen();

    void slotToggleShowDesktop();

    void slotWindowMaximize();
    void slotWindowMaximizeVertical();
    void slotWindowMaximizeHorizontal();
    void slotWindowMinimize();
    void slotWindowShade();
    void slotWindowRaise();
    void slotWindowLower();
    void slotWindowRaiseOrLower();
    void slotActivateAttentionWindow();

    void slotWindowMoveLeft();
    void slotWindowMoveRight();
    void slotWindowMoveUp();
    void slotWindowMoveDown();
    void slotWindowExpandHorizontal();
    void slotWindowExpandVertical();
    void slotWindowShrinkHorizontal();
    void slotWindowShrinkVertical();
    void slotWindowQuickTileLeft();
    void slotWindowQuickTileRight();
    void slotWindowQuickTileTop();
    void slotWindowQuickTileBottom();
    void slotWindowQuickTileTopLeft();
    void slotWindowQuickTileTopRight();
    void slotWindowQuickTileBottomLeft();
    void slotWindowQuickTileBottomRight();

    void slotSwitchWindowUp();
    void slotSwitchWindowDown();
    void slotSwitchWindowRight();
    void slotSwitchWindowLeft();

    void slotIncreaseWindowOpacity();
    void slotLowerWindowOpacity();

    void slotWindowOperations();
    void slotWindowClose();
    void slotWindowMove();
    void slotWindowResize();
    void slotWindowAbove();
    void slotWindowBelow();
    void slotWindowOnAllDesktops();
    void slotWindowFullScreen();
    void slotWindowNoBorder();

    void slotWindowToNextDesktop();
    void slotWindowToPreviousDesktop();
    void slotWindowToDesktopRight();
    void slotWindowToDesktopLeft();
    void slotWindowToDesktopUp();
    void slotWindowToDesktopDown();

    /**
     * Sends the Window to the given @p output.
     */
    void sendClientToScreen(KWin::Window *client, KWin::Output *output);

    /**
     * Shows an outline at the specified @p geometry.
     * If an outline is already shown the outline is moved to the new position.
     * Use hideOutline to remove the outline again.
     */
    void showOutline(const QRect &geometry);

    /**
     * Overloaded method for convenience.
     */
    void showOutline(int x, int y, int width, int height);

    /**
     * Hides the outline previously shown by showOutline.
     */
    void hideOutline();
};

class QtScriptWorkspaceWrapper : public WorkspaceWrapper
{
    Q_OBJECT
public:
    /**
     * List of windows currently managed by KWin.
     */
    Q_INVOKABLE QList<KWin::Window *> windowList() const;

    explicit QtScriptWorkspaceWrapper(QObject *parent = nullptr);
};

class DeclarativeScriptWorkspaceWrapper : public WorkspaceWrapper
{
    Q_OBJECT

    Q_PROPERTY(QQmlListProperty<KWin::Window> windows READ windows)
public:
    QQmlListProperty<KWin::Window> windows();
    static qsizetype countWindowList(QQmlListProperty<KWin::Window> *window);
    static KWin::Window *atWindowList(QQmlListProperty<KWin::Window> *windows, qsizetype index);

    explicit DeclarativeScriptWorkspaceWrapper(QObject *parent = nullptr);
};

}
