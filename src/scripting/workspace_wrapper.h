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
#include <qqmlregistration.h>

namespace KWin
{

// forward declarations
class Tile;
class TileManager;
class Window;
class LogicalOutput;
class VirtualDesktop;

/*!
 * \qmlsingletontype Workspace
 * \inqmlmodule org.kde.kwin
 */
class WorkspaceWrapper : public QObject
{
    Q_OBJECT

    /*!
     * \qmlproperty list<VirtualDesktop> Workspace::desktops
     */
    Q_PROPERTY(QList<KWin::VirtualDesktop *> desktops READ desktops NOTIFY desktopsChanged)

    /*!
     * \qmlproperty VirtualDesktop Workspace::currentDesktop
     */
    Q_PROPERTY(KWin::VirtualDesktop *currentDesktop READ currentDesktop WRITE setCurrentDesktop NOTIFY currentDesktopChanged)

    /*!
     * \qmlproperty bool Workspace::virtualDesktopNavigationWrapsAround
     *
     * Whether navigation in the desktop layout wraps around at the borders
     *
     * \since 6.7
     */
    Q_PROPERTY(bool virtualDesktopNavigationWrapsAround READ virtualDesktopNavigationWrapsAround NOTIFY virtualDesktopNavigationWrapsAroundChanged)

    /*!
     * \qmlproperty Window Workspace::activeWindow
     */
    Q_PROPERTY(KWin::Window *activeWindow READ activeWindow WRITE setActiveWindow NOTIFY windowActivated)

    // TODO: write and notify?
    /*!
     * \qmlproperty size Workspace::desktopGridSize
     */
    Q_PROPERTY(QSize desktopGridSize READ desktopGridSize NOTIFY desktopLayoutChanged)

    /*!
     * \qmlproperty int Workspace::desktopGridWidth
     */
    Q_PROPERTY(int desktopGridWidth READ desktopGridWidth NOTIFY desktopLayoutChanged)

    /*!
     * \qmlproperty int Workspace::desktopGridHeight
     */
    Q_PROPERTY(int desktopGridHeight READ desktopGridHeight WRITE setDesktopGridHeight NOTIFY desktopLayoutChanged)

    /*!
     * \qmlproperty int Workspace::workspaceWidth
     */
    Q_PROPERTY(int workspaceWidth READ workspaceWidth)

    /*!
     * \qmlproperty int Workspace::workspaceHeight
     */
    Q_PROPERTY(int workspaceHeight READ workspaceHeight)

    /*!
     * \qmlproperty size Workspace::workspaceSize
     */
    Q_PROPERTY(QSize workspaceSize READ workspaceSize)

    /*!
     * \qmlproperty LogicalOutput Workspace::activeScreen
     */
    Q_PROPERTY(KWin::LogicalOutput *activeScreen READ activeScreen)

    /*!
     * \qmlproperty list<LogicalOutput> Workspace::screens
     */
    Q_PROPERTY(QList<KWin::LogicalOutput *> screens READ screens NOTIFY screensChanged)

    /*!
     * \qmlproperty list<LogicalOutput> Workspace::screenOrder
     */
    Q_PROPERTY(QList<KWin::LogicalOutput *> screenOrder READ screenOrder NOTIFY screenOrderChanged)

    /*!
     * \qmlproperty string Workspace::currentActivity
     */
    Q_PROPERTY(QString currentActivity READ currentActivity WRITE setCurrentActivity NOTIFY currentActivityChanged)

    /*!
     * \qmlproperty list<string> Workspace::activities
     */
    Q_PROPERTY(QStringList activities READ activityList NOTIFY activitiesChanged)

    /*!
     * \qmlproperty size Workspace::virtualScreenSize
     *
     * The bounding size of all screens combined. Overlapping areas
     * are not counted multiple times.
     * \sa virtualScreenGeometry
     */
    Q_PROPERTY(QSize virtualScreenSize READ virtualScreenSize NOTIFY virtualScreenSizeChanged)

    /*!
     * \qmlproperty rect Workspace::virtualScreenGeometry
     *
     * The bounding geometry of all screens combined. Always starts at (0,0) and has
     * virtualScreenSize as it's size.
     * \sa virtualScreenSize
     */
    Q_PROPERTY(QRect virtualScreenGeometry READ virtualScreenGeometry NOTIFY virtualScreenGeometryChanged)

    /*!
     * \qmlproperty list<Window> Workspace::stackingOrder
     *
     * List of Clients currently managed by KWin, ordered by
     * their visibility (later ones cover earlier ones).
     */
    Q_PROPERTY(QList<KWin::Window *> stackingOrder READ stackingOrder)

    /*!
     * \qmlproperty point Workspace::cursorPos
     *
     * The current position of the cursor.
     */
    Q_PROPERTY(QPoint cursorPos READ cursorPos NOTIFY cursorPosChanged)

private:
    Q_DISABLE_COPY(WorkspaceWrapper)

Q_SIGNALS:
    /*!
     * \qmlsignal Workspace::windowAdded(Window window)
     */
    void windowAdded(KWin::Window *window);

    /*!
     * \qmlsignal Workspace::windowRemoved(Window window)
     */
    void windowRemoved(KWin::Window *window);

    /*!
     * \qmlsignal Workspace::windowActivated(Window window)
     */
    void windowActivated(KWin::Window *window);

    /*!
     * \qmlsignal Workspace::desktopsChanged()
     *
     * This signal is emitted when a virtual desktop is added or removed.
     */
    void desktopsChanged();

    /*!
     * \qmlsignal Workspace::desktopLayoutChanged()
     *
     * Signal emitted whenever the layout of virtual desktops changed.
     *
     * That is desktopGrid(Size/Width/Height) will have new values.
     * \since 4.11
     */
    void desktopLayoutChanged();

    /*!
     * \qmlsignal Workspace::screensChanged()
     *
     * Emitted when the output list changes, e.g. an output is connected or removed.
     */
    void screensChanged();

    /*!
     * \qmlsignal Workspace::screenOrderChanged()
     *
     * Emitted when the output order list changes, e.g. the primary output changes.
     */
    void screenOrderChanged();

    /*!
     * \qmlsignal Workspace::currentActivityChanged(string id)
     *
     * Signal emitted whenever the current activity changed.
     *
     * \a id id of the new activity
     */
    void currentActivityChanged(const QString &id);

    /*!
     * \qmlsignal Workspace::activitiesChanged(string id)
     *
     * Signal emitted whenever the list of activities changed.
     *
     * \a id id of the new activity
     */
    void activitiesChanged(const QString &id);

    /*!
     * \qmlsignal Workspace::activityAdded(string id)
     *
     * This signal is emitted when a new activity is added
     *
     * \a id id of the new activity
     */
    void activityAdded(const QString &id);

    /*!
     * \qmlsignal Workspace::activityRemoved(string id)
     *
     * This signal is emitted when the activity
     * is removed
     *
     * \a id id of the removed activity
     */
    void activityRemoved(const QString &id);

    /*!
     * \qmlsignal Workspace::virtualScreenSizeChanged()
     *
     * Emitted whenever the virtualScreenSize changes.
     *
     * \sa virtualScreenSize()
     * \since 5.0
     */
    void virtualScreenSizeChanged();

    /*!
     * \qmlsignal Workspace::virtualScreenGeometryChanged()
     *
     * Emitted whenever the virtualScreenGeometry changes.
     * \sa virtualScreenGeometry()
     * \since 5.0
     */
    void virtualScreenGeometryChanged();

    /*!
     * This signal is emitted when the current virtual desktop changes.
     *
     * \a output The screen where the desktop was changed.
     */
    void currentDesktopChanged(KWin::VirtualDesktop *previous, KWin::VirtualDesktop *current, KWin::LogicalOutput *output);

    /*!
     * Emitted whenever the setting on whether navigation in the desktop layout
     * wraps around at the borders changes.
     *
     * \since 6.7
     */
    void virtualDesktopNavigationWrapsAroundChanged();

    /*!
     * This signal is emitted when the cursor position changes.
     * \sa cursorPos()
     */
    void cursorPosChanged();

public:
    //------------------------------------------------------------------
    // enums copy&pasted from kwinglobals.h because qtscript is evil

    /*!
     * \value PlacementArea geometry where a window will be initially placed after being mapped
     * \value MovementArea window movement snapping area?  ignore struts
     * \value MaximizeArea geometry to which a window will be maximized
     * \value MaximizeFullArea like MaximizeArea, but ignore struts - used e.g. for topmenu
     * \value FullScreenArea area for fullscreen windows
     * \value WorkArea whole workarea (all screens together)
     * \value FullArea whole area (all screens together), ignore struts
     * \value ScreenArea one whole screen, ignore struts
     */
    enum ClientAreaOption {
        PlacementArea,
        MovementArea,
        MaximizeArea,
        MaximizeFullArea,
        FullScreenArea,
        WorkArea,
        FullArea,
        ScreenArea,
    };
    Q_ENUM(ClientAreaOption)

    /*!
     * \value ElectricTop
     * \value ElectricTopRight
     * \value ElectricRight
     * \value ElectricBottomRight
     * \value ElectricBottom
     * \value ElectricBottomLeft
     * \value ElectricLeft
     * \value ElectricTopLeft
     * \omitvalue ELECTRIC_COUNT
     * \value ElectricNone
     */
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
    void setDesktopGridHeight(int height);

    int workspaceWidth() const;
    int workspaceHeight() const;
    QSize workspaceSize() const;
    KWin::LogicalOutput *activeScreen() const;
    QList<KWin::LogicalOutput *> screens() const;
    QList<KWin::LogicalOutput *> screenOrder() const;
    QStringList activityList() const;
    QSize virtualScreenSize() const;
    QRect virtualScreenGeometry() const;
    QPoint cursorPos() const;

    QList<VirtualDesktop *> desktops() const;
    VirtualDesktop *currentDesktop() const;
    void setCurrentDesktop(VirtualDesktop *desktop);

    bool virtualDesktopNavigationWrapsAround() const;

    /*!
     * \qmlmethod VirtualDesktop Workspace::currentDesktopForScreen(LogicalOutput output)
     *
     * Returns current desktop on screen given by \a output.
     */
    Q_INVOKABLE KWin::VirtualDesktop *currentDesktopForScreen(KWin::LogicalOutput *output) const;

    /*!
     * \qmlmethod void Workspace::setCurrentDesktopForScreen(VirtualDesktop desktop, LogicalOutput output)
     *
     * Sets current desktop on \a output.
     */
    Q_INVOKABLE void setCurrentDesktopForScreen(KWin::VirtualDesktop *desktop, KWin::LogicalOutput *output);

    /*!
     * \qmlmethod LogicalOutput Workspace::screenAt(point pos)
     */
    Q_INVOKABLE KWin::LogicalOutput *screenAt(const QPointF &pos) const;

    /*!
     * \qmlmethod TileManager Workspace::tilingForScreen(string screenName)
     * \deprecated[6.3]
     * Use rootTile()
     */
    Q_INVOKABLE KWin::TileManager *tilingForScreen(const QString &screenName) const;

    /*!
     * \qmlmethod TileManager Workspace::tilingForScreen(LogicalOutput output)
     * \deprecated[6.3]
     * Use rootTile()
     */
    Q_INVOKABLE KWin::TileManager *tilingForScreen(KWin::LogicalOutput *output) const;

    /*!
     * \qmlmethod Tile Workspace::rootTile(LogicalOutput output, VirtualDesktop desktop)
     *
     * Returns the root tile for the given @a output and @a desktop.
     */
    Q_INVOKABLE KWin::Tile *rootTile(KWin::LogicalOutput *output, KWin::VirtualDesktop *desktop) const;

    /*!
     * \qmlmethod rect Workspace::clientArea(enumeration option, LogicalOutput output, VirtualDesktop desktop)
     *
     * Returns the geometry a Client can use with the specified option.
     *
     * This method should be preferred over other methods providing screen sizes as the
     * various options take constraints such as struts set on panels into account.
     * This method is also multi screen aware, but there are also options to get full areas.
     *
     * \a option The type of area which should be considered
     *
     * \a screen The screen for which the area should be considered
     *
     * \a desktop The desktop for which the area should be considered, in general there should not be a difference
     */
    Q_SCRIPTABLE QRectF clientArea(ClientAreaOption option, KWin::LogicalOutput *output, KWin::VirtualDesktop *desktop) const;

    /*!
     * \qmlmethod rect Workspace::clientArea(enumeration option, Window client)
     *
     * Overloaded method for convenience.
     *
     * \a client The Client for which the area should be retrieved
     */
    Q_SCRIPTABLE QRectF clientArea(ClientAreaOption option, KWin::Window *client) const;
    Q_SCRIPTABLE QRectF clientArea(ClientAreaOption option, const KWin::Window *client) const;

    /*!
     * \qmlmethod void Workspace::clientArea(int position, string name)
     *
     * Create a new virtual desktop at the requested position.
     *
     * \a position The position of the desktop. It should be in range [0, count].
     *
     * \a name The name for the new desktop, if empty the default name will be used.
     */
    Q_SCRIPTABLE void createDesktop(int position, const QString &name) const;

    /*!
     * \qmlmethod void Workspace::removeDesktop(VirtualDesktop desktop)
     *
     * Removes the specified virtual desktop.
     */
    Q_SCRIPTABLE void removeDesktop(KWin::VirtualDesktop *desktop) const;

    /*!
     * \qmlmethod void Workspace::moveDesktop(VirtualDesktop desktop, int position)
     *
     * Moves the \a desktop to the specified \a position.
     */
    Q_SCRIPTABLE void moveDesktop(KWin::VirtualDesktop *desktop, int position);

    /*!
     * \qmlmethod string Workspace::supportInformation()
     *
     * Provides support information about the currently running KWin instance.
     */
    Q_SCRIPTABLE QString supportInformation() const;

    QList<KWin::Window *> stackingOrder() const;

    /*!
     * \qmlmethod void Workspace::raiseWindow(Window window)
     *
     * Raises a Window  above all others on the screen.
     *
     * \a window The Window to raise
     */
    Q_INVOKABLE void raiseWindow(KWin::Window *window);

#if KWIN_BUILD_X11
    /*!
     * \qmlmethod Window Workspace::getClient(int windowId)
     *
     * Returns the Client with the given \a windowId.
     */
    Q_SCRIPTABLE KWin::Window *getClient(qulonglong windowId);
#endif

    /*!
     * \qmlmethod list<Window> Workspace::windowAt(point pos, int count = 1)
     *
     * Finds up to count windows at a particular location,
     * prioritizing the topmost one first.  A negative count
     * returns all matching clients.
     *
     * \a pos The location to look for
     *
     * \a count The number of clients to return
     *
     * Returns a list of Client objects
     */
    Q_INVOKABLE QList<KWin::Window *> windowAt(const QPointF &pos, int count = 1) const;

    /*!
     * \qmlmethod bool Workspace::isEffectActive(string pluginId)
     *
     * Checks if a specific effect is currently active.
     *
     * \a pluginId The plugin Id of the effect to check.
     *
     * Returns \c true if the effect is loaded and currently active, \c false otherwise.
     * \since 6.0
     */
    Q_INVOKABLE bool isEffectActive(const QString &pluginId) const;

    /*!
     * \qmlmethod void Workspace::constrain(Window below, Window above)
     *
     * Defines that a window needs to remain under another
     *
     * \a below the window that will be underneath
     *
     * \a above the window that will be over
     *
     * \since 6.5
     */
    Q_INVOKABLE void constrain(KWin::Window *below, KWin::Window *above);

    /*!
     * \qmlmethod Window Workspace::unconstrain(Window below, Window above)
     *
     * Breaks the constraint where a window is to remain under another
     *
     * \a below the window that was set to be underneath
     *
     * \a above the window that was set to be over
     *
     * \since 6.5
     */
    Q_INVOKABLE void unconstrain(KWin::Window *below, KWin::Window *above);

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
    void slotWindowExcludeFromCapture();

    void slotWindowToNextDesktop();
    void slotWindowToPreviousDesktop();
    void slotWindowToDesktopRight();
    void slotWindowToDesktopLeft();
    void slotWindowToDesktopUp();
    void slotWindowToDesktopDown();

    /*!
     * Sends the Window to the given \a output.
     */
    void sendClientToScreen(KWin::Window *client, KWin::LogicalOutput *output);

    /*!
     * Shows an outline at the specified \a geometry.
     * If an outline is already shown the outline is moved to the new position.
     * Use hideOutline to remove the outline again.
     */
    void showOutline(const QRect &geometry);

    /*!
     * Overloaded method for convenience.
     */
    void showOutline(int x, int y, int width, int height);

    /*!
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
    QML_NAMED_ELEMENT(Workspace)
    QML_SINGLETON

    Q_PROPERTY(QQmlListProperty<KWin::Window> windows READ windows)

public:
    QQmlListProperty<KWin::Window> windows();
    static qsizetype countWindowList(QQmlListProperty<KWin::Window> *window);
    static KWin::Window *atWindowList(QQmlListProperty<KWin::Window> *windows, qsizetype index);

    explicit DeclarativeScriptWorkspaceWrapper(QObject *parent = nullptr);
};

}
