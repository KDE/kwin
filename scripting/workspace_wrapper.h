/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Rohan Prabhu <rohan@rohanprabhu.com>
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef KWIN_SCRIPTING_WORKSPACE_WRAPPER_H
#define KWIN_SCRIPTING_WORKSPACE_WRAPPER_H

#include <QtCore/QObject>
#include <QtCore/QSize>
#include <kwinglobals.h>

namespace KWin
{
// forward declarations
class Client;

class WorkspaceWrapper : public QObject
{
    Q_OBJECT
    Q_ENUMS(ClientAreaOption)
    Q_PROPERTY(int currentDesktop READ currentDesktop WRITE setCurrentDesktop NOTIFY currentDesktopChanged)
    Q_PROPERTY(KWin::Client *activeClient READ activeClient WRITE setActiveClient NOTIFY clientActivated)
    // TODO: write and notify?
    Q_PROPERTY(QSize desktopGridSize READ desktopGridSize)
    Q_PROPERTY(int desktopGridWidth READ desktopGridWidth)
    Q_PROPERTY(int desktopGridHeight READ desktopGridHeight)
    Q_PROPERTY(int workspaceWidth READ workspaceWidth)
    Q_PROPERTY(int workspaceHeight READ workspaceHeight)
    Q_PROPERTY(QSize workspaceSize READ workspaceSize)
    /**
     * The number of desktops currently used. Minimum number of desktops is 1, maximum 20.
     **/
    Q_PROPERTY(int desktops READ numberOfDesktops WRITE setNumberOfDesktops NOTIFY numberDesktopsChanged)
    /**
     * The same of the display, that is all screens.
     **/
    Q_PROPERTY(QSize displaySize READ displaySize)
    /**
     * The width of the display, that is width of all combined screens.
     **/
    Q_PROPERTY(int displayWidth READ displayWidth)
    /**
     * The height of the display, that is height of all combined screens.
     **/
    Q_PROPERTY(int displayHeight READ displayHeight)
    Q_PROPERTY(int activeScreen READ activeScreen)
    Q_PROPERTY(int numScreens READ numScreens)

private:
    Q_DISABLE_COPY(WorkspaceWrapper)

signals:
    void desktopPresenceChanged(KWin::Client*, int);
    void currentDesktopChanged(int);
    void clientAdded(KWin::Client*);
    void clientRemoved(KWin::Client*);
    void clientManaging(KWin::Client*);
    void clientMinimized(KWin::Client*);
    void clientUnminimized(KWin::Client*);
    void clientRestored(KWin::Client*);
    void clientMaximizeSet(KWin::Client*, bool, bool);
    void killWindowCalled(KWin::Client*);
    void clientActivated(KWin::Client*);
    void clientFullScreenSet(KWin::Client*, bool, bool);
    void clientSetKeepAbove(KWin::Client*, bool);
    /**
     * Signal emitted whenever the number of desktops changed.
     * To get the current number of desktops use the property desktops.
     * @param oldNumberOfDesktops The previous number of desktops.
     **/
    void numberDesktopsChanged(int oldNumberOfDesktops);

public:
//------------------------------------------------------------------
//enums copy&pasted from kwinglobals.h because qtscript is evil

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

    WorkspaceWrapper(QObject* parent = 0);
#define GETTERSETTERDEF( rettype, getter, setter ) \
rettype getter() const; \
void setter( rettype val );
    GETTERSETTERDEF(int, numberOfDesktops, setNumberOfDesktops)
    GETTERSETTERDEF(int, currentDesktop, setCurrentDesktop)
    GETTERSETTERDEF(KWin::Client*, activeClient, setActiveClient)
#undef GETTERSETTERDEF
    QSize desktopGridSize() const;
    int desktopGridWidth() const;
    int desktopGridHeight() const;
    int workspaceWidth() const;
    int workspaceHeight() const;
    QSize workspaceSize() const;
    int displayWidth() const;
    int displayHeight() const;
    QSize displaySize() const;
    int activeScreen() const;
    int numScreens() const;

    Q_INVOKABLE QList< KWin::Client* > clientList() const;
    /**
     * Returns the geometry a Client can use with the specified option.
     * This method should be preferred over other methods providing screen sizes as the
     * various options take constraints such as struts set on panels into account.
     * This method is also multi screen aware, but there are also options to get full areas.
     * @param option The type of area which should be considered
     * @param screen The screen for which the area should be considered
     * @param desktop The desktop for which the area should be considered, in general there should not be a difference
     * @returns The specified screen geometry
     **/
    Q_SCRIPTABLE QRect clientArea(ClientAreaOption option, int screen, int desktop) const;
    /**
     * Overloaded method for convenience.
     * @param option The type of area which should be considered
     * @param point The coordinates which have to be included in the area
     * @param desktop The desktop for which the area should be considered, in general there should not be a difference
     * @returns The specified screen geometry
     **/
    Q_SCRIPTABLE QRect clientArea(ClientAreaOption option, const QPoint& point, int desktop) const;
    /**
     * Overloaded method for convenience.
     * @param client The Client for which the area should be retrieved
     * @returns The specified screen geometry
     **/
    Q_SCRIPTABLE QRect clientArea(ClientAreaOption option, const Client* client) const;
    /**
     * Returns the name for the given @p desktop.
     **/
    Q_SCRIPTABLE QString desktopName(int desktop) const;
    /**
     * Provides support information about the currently running KWin instance.
     **/
    Q_SCRIPTABLE QString supportInformation() const;

public Q_SLOTS:
    // all the available key bindings
    void slotSwitchDesktopNext();
    void slotSwitchDesktopPrevious();
    void slotSwitchDesktopRight();
    void slotSwitchDesktopLeft();
    void slotSwitchDesktopUp();
    void slotSwitchDesktopDown();

    void slotSwitchToNextScreen();
    void slotWindowToNextScreen();
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
    void slotWindowPackLeft();
    void slotWindowPackRight();
    void slotWindowPackUp();
    void slotWindowPackDown();
    void slotWindowGrowHorizontal();
    void slotWindowGrowVertical();
    void slotWindowShrinkHorizontal();
    void slotWindowShrinkVertical();
    void slotWindowQuickTileLeft();
    void slotWindowQuickTileRight();
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

private Q_SLOTS:
    void setupClientConnections(KWin::Client* client);
};

}

#endif
