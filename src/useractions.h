/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_USERACTIONS_H
#define KWIN_USERACTIONS_H
#include "ui_shortcutdialog.h"

#include <kwinglobals.h>

// Qt
#include <QDialog>
#include <QObject>
#include <QPointer>

class QAction;
class QRect;

namespace KWin
{
class AbstractClient;

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
 */
class KWIN_EXPORT UserActionsMenu : public QObject
{
    Q_OBJECT
public:
    explicit UserActionsMenu(QObject *parent = nullptr);
    ~UserActionsMenu() override;
    /**
     * Discards the constructed menu, so that it gets recreates
     * on next show event.
     * @see show
     */
    void discard();

    /**
     * @returns Whether the menu is currently visible
     */
    bool isShown() const;

    /**
     * grabs keyboard and mouse, workaround(?) for bug #351112
     */
    void grabInput();

    /**
     * @returns Whether the menu has a Client set to operate on.
     */
    bool hasClient() const;
    /**
     * Checks whether the given Client @p c is the Client
     * for which the Menu is shown.
     * @param c The Client to compare
     * @returns Whether the Client is the one related to this Menu
     */
    bool isMenuClient(const AbstractClient *c) const;
    /**
     * Closes the Menu and prepares it for next usage.
     */
    void close();
    /**
     * @brief Shows the menu at the given @p pos for the given @p client.
     *
     * @param pos The position where the menu should be shown.
     * @param client The Client for which the Menu has to be shown.
     */
    void show(const QRect &pos, AbstractClient *client);

public Q_SLOTS:
    /**
     * Delayed initialization of the activity menu.
     *
     * The call to retrieve the current list of activities is performed in a thread and this
     * slot is invoked once the list has been fetched. Only task of this method is to decide
     * whether to show the activity menu and to invoke the initialization of it.
     *
     * @see initActivityPopup
     */
    void showHideActivityMenu();

private Q_SLOTS:
    /**
     * The menu will become visible soon.
     *
     * Adjust the items according to the respective Client.
     */
    void menuAboutToShow();
    /**
     * Adjusts the desktop popup to the current values and the location of
     * the Client.
     */
    void desktopPopupAboutToShow();
    /**
     * Adjusts the multipleDesktopsMenu popup to the current values and the location of
     * the Client, Wayland only.
     */
    void multipleDesktopsPopupAboutToShow();
    /**
     * Adjusts the screen popup to the current values and the location of
     * the Client.
     */
    void screenPopupAboutToShow();
    /**
     * Adjusts the activity popup to the current values and the location of
     * the Client.
     */
    void activityPopupAboutToShow();
    /**
     * Sends the client to desktop \a desk
     *
     * @param action Invoked Action containing the Desktop as data element
     */
    void slotSendToDesktop(QAction *action);
    /**
     * Toggle whether the Client is on a desktop (Wayland only)
     *
     * @param action Invoked Action containing the Desktop as data element
     */
    void slotToggleOnVirtualDesktop(QAction *action);
    /**
     * Sends the Client to screen \a screen
     *
     * @param action Invoked Action containing the Screen as data element
     */
    void slotSendToScreen(QAction *action);
    /**
     * Toggles whether the Client is on the \a activity
     *
     * @param action Invoked Action containing the Id of the Activity to toggle the Client on
     */
    void slotToggleOnActivity(QAction *action);
    /**
     * Performs a window operation.
     *
     * @param action Invoked Action containing the Window Operation to perform for the Client
     */
    void slotWindowOperation(QAction *action);

private:
    /**
     * Creates the menu if not already created.
     */
    void init();
    /**
     * Creates the Move to Desktop sub-menu.
     */
    void initDesktopPopup();
    /**
     * Creates the Move to Screen sub-menu.
     */
    void initScreenPopup();
    /**
     * Creates activity popup.
     * I'm going with checkable ones instead of "copy to" and "move to" menus; I *think* it's an easier way.
     * Oh, and an 'all' option too of course
     */
    void initActivityPopup();
    /**
     * Shows a helper Dialog to inform the user how to get back in case he triggered
     * an action which hides the window decoration (e.g. NoBorder or Fullscreen).
     * @param message The message type to be shown
     * @param c The Client for which the dialog should be shown.
     */
    void helperDialog(const QString &message, AbstractClient *c);
    /**
     * The actual main context menu which is show when the UserActionsMenu is invoked.
     */
    QMenu* m_menu;
    /**
     * The move to desktop sub menu.
     */
    QMenu* m_desktopMenu;
    /**
     * The move to desktop sub menu, with the Wayland protocol.
     */
    QMenu* m_multipleDesktopsMenu;
    /**
     * The move to screen sub menu.
     */
    QMenu* m_screenMenu;
    /**
     * The activities sub menu.
     */
    QMenu* m_activityMenu;
    /**
     * Menu for further entries added by scripts.
     */
    QMenu* m_scriptsMenu;
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
    QAction* m_shortcutOperation;
    /**
     * The Client for which the menu is shown.
     */
    QPointer<AbstractClient> m_client;
    QAction *m_rulesOperation = nullptr;
    QAction *m_applicationRulesOperation = nullptr;
};

class ShortcutDialog
    : public QDialog
{
    Q_OBJECT
public:
    explicit ShortcutDialog(const QKeySequence& cut);
    void accept() override;
    QKeySequence shortcut() const;
public Q_SLOTS:
    void keySequenceChanged();
Q_SIGNALS:
    void dialogDone(bool ok);
protected:
    void done(int r) override;
private:
    Ui::ShortcutDialog m_ui;
    QKeySequence _shortcut;
};

} // namespace

#endif //  KWIN_USERACTIONS_H
