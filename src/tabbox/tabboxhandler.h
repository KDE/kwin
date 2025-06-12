/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "tabboxconfig.h"

#include <QModelIndex>
#include <QPixmap>
#include <QString>

/**
 * @file
 * This file contains the classes which hide KWin core from tabbox.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 * @since 4.4
 */

class QKeyEvent;

namespace KWin
{

class Window;

/**
 * The TabBox is a model based view for displaying a list while switching windows.
 * This functionality is mostly referred to as Alt+Tab. TabBox itself does not provide support for
 * switching windows. This has to be done outside of TabBox inside an independent controller.
 *
 * The main entrance point to TabBox is the class TabBoxHandler, which has to be subclassed and implemented.
 *
 * The behavior of the TabBox is defined by the TabBoxConfig and has to be set in the TabBoxHandler.
 *
 * In order to use the TabBox the TabBoxConfig has to be set. The model has to be initialized by calling
 * TabBoxHandler::createModel(), as the
 * model is undefined when the TabBox is not active. The TabBox is activated by TabBoxHandler::show().
 * Depending on the current set TabBoxConfig it is possible that the
 * highlight windows effect activated and that the view is not displayed at all. As already mentioned
 * the TabBox does not handle any updating of the selected item. This has to be done by invoking
 * TabBoxHandler::setCurrentIndex(). Nevertheless the TabBoxHandler provides methods to query for the
 * model index or the next or previous item, for a cursor position or for a given item. By invoking
 * TabBoxHandler::hide() the view, the
 * optional highlight windows effect are removed. The model is invalidated immediately. So if it is
 * necessary to retrieve the last selected item this has to be done before calling the hide method.
 *
 * The layout of the TabBox View and the items is completely customizable. Therefore TabBox provides
 * a widget LayoutConfig which includes a live preview (in kcmkwin/kwintabbox). The layout of items
 * can be defined by an xml document. That way the user is able to define own custom layouts. The view
 * itself is made up of two widgets: one to show the complete list and one to show only the selected
 * item. This way it is possible to have a view which shows for example a list containing only small
 * icons and nevertheless show the title of the currently selected client.
 */
namespace TabBox
{
class ClientModel;
class TabBoxConfig;
class TabBoxHandlerPrivate;

/**
 * This class is a wrapper around KWin Workspace. It is used for accessing the
 * required core methods from inside TabBox and has to be implemented in KWin core.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 * @since 4.4
 */
class TabBoxHandler : public QObject
{
    Q_OBJECT
public:
    TabBoxHandler(QObject *parent);
    ~TabBoxHandler() override;

    /**
     * @return The id of the active screen
     */
    virtual int activeScreen() const = 0;
    /**
     * @return The current active Window or NULL
     * if there is no active client.
     */
    virtual Window *activeClient() const = 0;
    /**
     * @param client The client which is starting point to find the next client
     * @return The next Window in focus chain
     */
    virtual Window *nextClientFocusChain(Window *client) const = 0;
    /**
     * This method is used by the ClientModel to find an entrance into the focus chain in case
     * there is no active Client.
     *
     * @return The first Client of the focus chain
     * @since 4.9.1
     */
    virtual Window *firstClientFocusChain() const = 0;
    /**
     * Checks whether the given @p client is part of the focus chain at all.
     * This is useful to figure out whether the currently active Client can be used
     * as a starting point to construct the recently used list.
     *
     * In case the @p client is not in the focus chain it is recommended to use the
     * Client returned by firstClientFocusChain.
     *
     * The method accepts a @c null Client and in that case @c false is returned.
     * @param client The Client to check whether it is in the Focus Chain
     * @return @c true in case the Client is part of the focus chain, @c false otherwise.
     * @since 4.9.2
     */
    virtual bool isInFocusChain(Window *client) const = 0;
    /**
     * @param client The client whose desktop name should be retrieved
     * @return The desktop name of the given Window. If the client is
     * on all desktops the name of current desktop will be returned.
     */
    virtual QString desktopName(Window *client) const = 0;

    /**
     * Raise a client (w/o activating it)
     */
    virtual void raiseClient(Window *c) const = 0;

    /**
     * @param c The client to be restacked
     * @param under The client the other one will be placed below
     */
    virtual void restack(Window *c, Window *under) = 0;

    /**
     * Toggle between ShadeHover and ShadeNormal - not shaded windows are unaffected
     * @param c The client to be shaded
     * @param b Whether to un- or shade
     */
    virtual void shadeClient(Window *c, bool b) const = 0;

    virtual void highlightWindows(Window *window = nullptr, QWindow *controller = nullptr) = 0;

    /**
     * @return The current stacking order of Windows
     */
    virtual QList<Window *> stackingOrder() const = 0;
    /**
     * Determines if given client will be added to the list:
     * <UL>
     * <LI>if the client wants to have tab focus.</LI>
     * <LI>The client won't be added if it has modal dialogs</LI>
     * <LI>In that case the modal dialog will be returned if it isn't already
     * included</LI>
     * <LI>Won't be added if it isn't on active screen when using separate
     * screen focus</LI>
     * </UL>
     * @param client The client to be checked for inclusion
     * @param allDesktops Add clients from all desktops or only from current
     * @return The client to be included in the list or NULL if it isn't to be included
     */
    virtual Window *clientToAddToList(Window *client) const = 0;
    /**
     * @return The first desktop window in the stacking order.
     */
    virtual Window *desktopClient() const = 0;
    /**
     * Activates the currently selected client and closes the TabBox.
     */
    virtual void activateAndClose() = 0;

    /**
     * @return The currently used TabBoxConfig
     */
    const TabBoxConfig &config() const;
    /**
     * Call this method when you want to change the currently used TabBoxConfig.
     * It fires the signal configChanged.
     * @param config Updates the currently used TabBoxConfig to config
     */
    void setConfig(const TabBoxConfig &config);

    /**
     * Call this method to show the TabBoxView. Depending on current
     * configuration this method might not do anything.
     * If highlight windows effect is to be used it will be activated.
     * @see TabBoxConfig::isShowTabBox
     * @see TabBoxConfig::isHighlightWindows
     */
    void show();
    /**
     * Hides the TabBoxView if shown.
     * Deactivates highlight windows effect if active.
     * @see show
     */
    void hide(bool abort = false);

    /**
     * Sets the current model index in the view and updates
     * highlight windows if active.
     * @param index The current Model index
     */
    void setCurrentIndex(const QModelIndex &index);
    /**
     * @returns the current index
     */
    const QModelIndex &currentIndex() const;

    /**
     * Retrieves the next or previous item of the current item.
     * @param forward next or previous item
     * @return The next or previous item. If there is no matching item
     * the current item will be returned.
     */
    QModelIndex nextPrev(bool forward) const;

    /**
     * Initializes the model based on the current config.
     * This method has to be invoked before showing the TabBox.
     * It can also be invoked when clients are added or removed.
     * In that case partialReset has to be true.
     *
     * @param partialReset Keep the currently selected item or regenerate everything
     */
    void createModel(bool partialReset = false);

    /**
     * Handles additional grabbed key events by the TabBox controller.
     * @param event The key event which has been grabbed
     */
    virtual void grabbedKeyEvent(QKeyEvent *event) const;
    /**
     * @param pos The position to be tested in global coordinates
     * @return True if the view contains the point, otherwise false.
     */
    bool containsPos(const QPoint &pos) const;
    /**
     * @param client The Window whose index should be returned
     * @return Returns the ModelIndex of given Window or an invalid ModelIndex
     * if the model does not contain the given Window.
     * @see ClientModel::index
     */
    QModelIndex index(Window *client) const;
    /**
     * @return Returns the current list of Windows.
     * @see ClientModel::clientList
     */
    QList<Window *> clientList() const;
    /**
     * @param index The index of the client to be returned
     * @return Returns the Window at given model index. If
     * the index is invalid, does not point to a Client or the list
     * is empty, NULL will be returned.
     */
    Window *client(const QModelIndex &index) const;
    /**
     * @return The first model index. That is the model index at position 0, 0.
     * It is valid, as desktop has at least one desktop and if there are no
     * clients an empty item is created.
     */
    QModelIndex first() const;

    bool eventFilter(QObject *watcher, QEvent *event) override;

    /**
     * @returns whether the TabBox operates in a no modifier grab mode.
     * In this mode a click on an item should directly accept and close the tabbox.
     */
    virtual bool noModifierGrab() const = 0;

Q_SIGNALS:
    /**
     * This signal is fired when the TabBoxConfig changes
     * @see setConfig
     */
    void configChanged();
    void selectedIndexChanged();

private Q_SLOTS:
    void initHighlightWindows();

private:
    friend class TabBoxHandlerPrivate;
    TabBoxHandlerPrivate *d;
};

/**
 * Pointer to the global TabBoxHandler object.
 */
extern TabBoxHandler *tabBox;

} // namespace TabBox
} // namespace KWin
