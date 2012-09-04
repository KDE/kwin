/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

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
#ifndef KWIN_USERACTIONS_H
#define KWIN_USERACTIONS_H

#include <QObject>
#include <QWeakPointer>

class QAction;
class QMenu;
class QRect;

namespace KWin
{
class Client;

/**
 * @brief Menu shown for a Client.
 *
 * The UserActionsMenu implements the Menu which is shown on:
 * @li context-menu event on Window decoration
 * @li window menu button
 * @li Keyboard Shortcut (by default Alt+F3)
 *
 * The menu contains various window management related actions for the Client the menu is opened
 * for, this is normally the active Client.
 *
 * The menu which is shown is tried to be as close as possible to the menu implemented in
 * libtaskmanager, though there are differences as there are some actions only the window manager
 * can provide and on the other hand the libtaskmanager cares also about things like e.g. grouping.
 *
 * Whenever the menu is changed it should be tried to also adjust the menu in libtaskmanager.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 **/
class UserActionsMenu : public QObject
{
    Q_OBJECT
public:
    explicit UserActionsMenu(QObject *parent = 0);
    virtual ~UserActionsMenu();
    /**
     * Discards the constructed menu, so that it gets recreates
     * on next show event.
     * @see show
     **/
    void discard();

    /**
     * @returns Whether the menu is currently visible
     **/
    bool isShown() const;
    /**
     * @returns Whether the menu has a Client set to operate on.
     **/
    bool hasClient() const;
    /**
     * Checks whether the given Client @p c is the Client
     * for which the Menu is shown.
     * @param c The Client to compare
     * @returns Whether the Client is the one related to this Menu
     **/
    bool isMenuClient(const Client *c) const;
    /**
     * Closes the Menu and prepares it for next usage.
     **/
    void close();
    /**
     * @brief Shows the menu at the given @p pos for the given @p client.
     *
     * @param pos The position where the menu should be shown.
     * @param client The Client for which the Menu has to be shown.
     **/
    void show(const QRect &pos, const QWeakPointer<Client> &client);

public slots:
    /**
     * Delayed initialization of the activity menu.
     *
     * The call to retrieve the current list of activities is performed in a thread and this
     * slot is invoked once the list has been fetched. Only task of this method is to decide
     * whether to show the activity menu and to invoke the initialization of it.
     *
     * @see initActivityPopup
     **/
    void showHideActivityMenu();

private slots:
    /**
     * The menu will become visible soon.
     *
     * Adjust the items according to the respective Client.
     **/
    void menuAboutToShow();
    /**
     * Adjusts the add to tab group menu to the current value of the Client.
     **/
    void rebuildTabGroupPopup();
    /**
     * Adjusts the switch to tab menu to the current values of the Client.
     **/
    void rebuildTabListPopup();
    /**
     * Adds the Client as tab to the Client identified by the @p action.
     *
     * @param action The invoked action containing the Client to which the active Client should be tabbed.
     **/
    void entabPopupClient(QAction *action);
    /**
     * Activates the selected tabbed Client.
     *
     * @param action The invoked action containing the tabbed Client which should be activated.
     **/
    void selectPopupClientTab(QAction *action);
    /**
     * Adjusts the desktop popup to the current values and the location of
     * the Client.
     **/
    void desktopPopupAboutToShow();
    /**
     * Adjusts the screen popup to the current values and the location of
     * the Client.
     **/
    void screenPopupAboutToShow();
    /**
     * Adjusts the activity popup to the current values and the location of
     * the Client.
     **/
    void activityPopupAboutToShow();
    /**
     * Sends the client to desktop \a desk
     *
     * @param action Invoked Action containing the Desktop as data element
     **/
    void slotSendToDesktop(QAction *action);
    /**
     * Sends the Client to screen \a screen
     *
     * @param action Invoked Action containing the Screen as data element
     **/
    void slotSendToScreen(QAction *action);
    /**
     * Toggles whether the Client is on the \a activity
     *
     * @param action Invoked Action containing the Id of the Activity to toggle the Client on
     **/
    void slotToggleOnActivity(QAction *action);
    /**
     * Performs a window operation.
     *
     * @param action Invoked Action containing the Window Operation to perform for the Client
     **/
    void slotWindowOperation(QAction *action);
    /**
     * Invokes the kcmshell with the Window Manager related config modules.
     **/
    void configureWM();

private:
    /**
     * Creates the menu if not already created.
     **/
    void init();
    /**
     * Creates the Move To Desktop sub-menu.
     **/
    void initDesktopPopup();
    /**
     * Creates the Move To Screen sub-menu.
     **/
    void initScreenPopup();
    /**
     * Creates activity popup.
     * I'm going with checkable ones instead of "copy to" and "move to" menus; I *think* it's an easier way.
     * Oh, and an 'all' option too of course
     **/
    void initActivityPopup();
    /**
     * Creates the Window Tabbing related menus.
     **/
    void initTabbingPopups();
    /**
     * Shows a helper Dialog to inform the user how to get back in case he triggered
     * an action which hides the window decoration (e.g. NoBorder or Fullscreen).
     * @param message The message type to be shown
     * @param c The Client for which the dialog should be shown.
     **/
    void helperDialog(const QString &message, const QWeakPointer<Client> &c);
    /**
     * The actual main context menu which is show when the UserActionsMenu is invoked.
     **/
    QMenu* m_menu;
    /**
     * The move to desktop sub menu.
     **/
    QMenu* m_desktopMenu;
    /**
     * The move to screen sub menu.
     **/
    QMenu* m_screenMenu;
    /**
     * The activities sub menu.
     **/
    QMenu* m_activityMenu;
    /**
     * Menu to add the group to other group.
     **/
    QMenu* m_addTabsMenu;
    /**
     * Menu to change tab.
     **/
    QMenu* m_switchToTabMenu;
    QAction* m_resizeOperation;
    QAction* m_moveOperation;
    QAction* m_maximizeOperation;
    QAction* m_shadeOperation;
    QAction* m_keepAboveOperation;
    QAction* m_keepBelowOperation;
    QAction* m_fullScreenOperation;
    QAction* m_noBorderOperation;
    QAction* m_minimizeOperation;
    QAction* m_closeOperation;
    /**
     * Remove client from group.
     **/
    QAction* m_removeFromTabGroup;
    /**
     * Close all clients in the group.
     **/
    QAction* m_closeTabGroup;
    /**
     * The Client for which the menu is shown.
     **/
    QWeakPointer<Client> m_client;
};
} // namespace

#endif //  KWIN_USERACTIONS_H
