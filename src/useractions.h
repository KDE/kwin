/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
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
class Window;

/**
 * @brief Menu shown for a Window.
 *
 * The UserActionsMenu implements the Menu which is shown on:
 * @li context-menu event on Window decoration
 * @li window menu button
 * @li Keyboard Shortcut (by default Alt+F3)
 *
 * The menu contains various window management related actions for the Window the menu is opened
 * for, this is normally the active Window.
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
     * @returns Whether the menu has a Window set to operate on.
     */
    bool hasWindow() const;
    /**
     * Checks whether the given Window @p window is the Window
     * for which the Menu is shown.
     * @param c The Window to compare
     * @returns Whether the Window is the one related to this Menu
     */
    bool isMenuWindow(const Window *window) const;
    /**
     * Closes the Menu and prepares it for next usage.
     */
    void close();
    /**
     * @brief Shows the menu at the given @p pos for the given @p window.
     *
     * @param pos The position where the menu should be shown.
     * @param window The Window for which the Menu has to be shown.
     */
    void show(const QRect &pos, Window *window);

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
     * Adjust the items according to the respective Window.
     */
    void menuAboutToShow();

    /**
     * The menu is about to close, all actions have been handled
     *
     * Perform cleanup
     */
    void menuAboutToHide();

    /**
     * Adjusts the desktop popup to the current values and the location of
     * the Window.
     */
    void desktopPopupAboutToShow();
    /**
     * Adjusts the multipleDesktopsMenu popup to the current values and the location of
     * the Window, Wayland only.
     */
    void multipleDesktopsPopupAboutToShow();
    /**
     * Adjusts the screen popup to the current values and the location of
     * the Window.
     */
    void screenPopupAboutToShow();
    /**
     * Adjusts the activity popup to the current values and the location of
     * the Window.
     */
    void activityPopupAboutToShow();
    /**
     * Performs a window operation.
     *
     * @param action Invoked Action containing the Window Operation to perform for the Window
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
     * @param window The Window for which the dialog should be shown.
     */
    void helperDialog(const QString &message, Window *window);
    /**
     * The actual main context menu which is show when the UserActionsMenu is invoked.
     */
    QMenu *m_menu;
    /**
     * The move to desktop sub menu.
     */
    QMenu *m_desktopMenu;
    /**
     * The move to desktop sub menu, with the Wayland protocol.
     */
    QMenu *m_multipleDesktopsMenu;
    /**
     * The move to screen sub menu.
     */
    QMenu *m_screenMenu;
    /**
     * The activities sub menu.
     */
    QMenu *m_activityMenu;
    /**
     * Menu for further entries added by scripts.
     */
    QMenu *m_scriptsMenu;
    QAction *m_resizeOperation;
    QAction *m_moveOperation;
    QAction *m_maximizeOperation;
    QAction *m_shadeOperation;
    QAction *m_keepAboveOperation;
    QAction *m_keepBelowOperation;
    QAction *m_fullScreenOperation;
    QAction *m_noBorderOperation;
    QAction *m_minimizeOperation;
    QAction *m_closeOperation;
    QAction *m_shortcutOperation;
    /**
     * The window for which the menu is shown.
     */
    QPointer<Window> m_window;
    QAction *m_rulesOperation = nullptr;
    QAction *m_applicationRulesOperation = nullptr;
};

class ShortcutDialog
    : public QDialog
{
    Q_OBJECT
public:
    explicit ShortcutDialog(const QKeySequence &cut);
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
