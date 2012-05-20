/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

#ifndef TABBOXHANDLER_H
#define TABBOXHANDLER_H

#include "tabboxconfig.h"

#include <QModelIndex>
#include <QPixmap>
#include <QString>
#include <X11/Xlib.h>
#include <fixx11h.h>

/**
* @file
* This file contains the classes which hide KWin core from tabbox.
* It defines the pure virtual classes TabBoxHandler and TabBoxClient.
* The classes have to be implemented in KWin Core.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/

class QKeyEvent;

namespace KWin
{
/**
* The TabBox is a model based view for displaying a list while switching windows or desktops.
* This functionality is mostly referred as Alt+Tab. TabBox itself does not provide support for
* switching windows or desktops. This has to be done outside of TabBox inside an independent controller.
*
* The main entrance point to TabBox is the class TabBoxHandler, which has to be subclassed and implemented.
* The class TabBoxClient, which represents a window client inside TabBox, has to be implemented as well.
*
* The behavior of the TabBox is defined by the TabBoxConfig and has to be set in the TabBoxHandler.
* If the TabBox should be used to switch desktops as well as clients it is sufficient to just provide
* different TabBoxConfig objects instead of creating an own handler for each mode.
*
* In order to use the TabBox the TabBoxConfig has to be set. This defines if the model for desktops or for
* clients will be used. The model has to be initialized by calling TabBoxHandler::createModel(), as the
* model is undefined when the TabBox is not active. The TabBox is activated by TabBoxHandler::show().
* Depending on the current set TabBoxConfig it is possible that an additional outline is shown, the
* highlight windows effect activated and that the view is not displayed at all. As already mentioned
* the TabBox does not handle any updating of the selected item. This has to be done by invoking
* TabBoxHandler::setCurrentIndex(). Nevertheless the TabBoxHandler provides methods to query for the
* model index or the next or previous item, for a cursor position or for a given item (that is
* TabBoxClient or desktop). By invoking TabBoxHandler::hide() the view, the optional outline and the
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
class DesktopModel;
class ClientModel;
class TabBoxConfig;
class TabBoxClient;
class TabBoxHandlerPrivate;
typedef QList< QWeakPointer< TabBoxClient > > TabBoxClientList;

/**
* This class is a wrapper around KWin Workspace. It is used for accessing the
* required core methods from inside TabBox and has to be implemented in KWin core.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/
class TabBoxHandler : public QObject
{
    Q_OBJECT
public:
    TabBoxHandler();
    virtual ~TabBoxHandler();

    /**
    * @return The id of the active screen
    */
    virtual int activeScreen() const = 0;
    /**
    * @return The current active TabBoxClient or NULL
    * if there is no active client.
    */
    virtual QWeakPointer<TabBoxClient> activeClient() const = 0;
    /**
    * @param client The client which is starting point to find the next client
    * @return The next TabBoxClient in focus chain
    */
    virtual QWeakPointer<TabBoxClient> nextClientFocusChain(TabBoxClient* client) const = 0;
    /**
    * @param client The client whose desktop name should be retrieved
    * @return The desktop name of the given TabBoxClient. If the client is
    * on all desktops the name of current desktop will be returned.
    */
    virtual QString desktopName(TabBoxClient* client) const = 0;
    /**
    * @param desktop The desktop whose name should be retrieved
    * @return The desktop name of given desktop
    */
    virtual QString desktopName(int desktop) const = 0;
    /**
    * @return The number of current desktop
    */
    virtual int currentDesktop() const = 0;
    /**
    * @return The number of virtual desktops
    */
    virtual int numberOfDesktops() const = 0;
    /**
    * @param desktop The desktop which is the starting point to find the next desktop
    * @return The next desktop in the current focus chain.
    */
    virtual int nextDesktopFocusChain(int desktop) const = 0;

    /**
    * De-/Elevate a client using the compositor (if enabled)
    */
    virtual void elevateClient(TabBoxClient* c, WId tabbox, bool elevate) const = 0;

    /**
    * Raise a client (w/o activating it)
    */
    virtual void raiseClient(TabBoxClient* c) const = 0;

    /**
     * @param c The client to be restacked
     * @param under The client the other one will be placed below
     */
    virtual void restack(TabBoxClient *c, TabBoxClient *under) = 0;

    /**
    * @return The current stacking order of TabBoxClients
    */
    virtual TabBoxClientList stackingOrder() const = 0;
    /**
    * Determines if given client will be added to the list:
    * <UL>
    * <LI>Depends on desktop</LI>
    * <LI>if the client wants to have tab focus.</LI>
    * <LI>The client won't be added if it has modal dialogs</LI>
    * <LI>In that case the modal dialog will be returned if it isn't already
    * included</LI>
    * <LI>Won't be added if it isn't on active screen when using separate
    * screen focus</LI>
    * </UL>
    * @param client The client to be checked for inclusion
    * @param desktop The desktop the client should be on. This is irrelevant if allDesktops is set
    * @param allDesktops Add clients from all desktops or only from current
    * @return The client to be included in the list or NULL if it isn't to be included
    */
    virtual QWeakPointer<TabBoxClient> clientToAddToList(TabBoxClient* client, int desktop) const = 0;
    /**
    * @return The first desktop window in the stacking order.
    */
    virtual QWeakPointer<TabBoxClient> desktopClient() const = 0;
    /**
     * Activates the currently selected client and closes the TabBox.
     **/
    virtual void activateAndClose() = 0;

    /**
    * @return The currently used TabBoxConfig
    */
    const TabBoxConfig& config() const;
    /**
    * Call this method when you want to change the currently used TabBoxConfig.
    * It fires the signal configChanged.
    * @param config Updates the currently used TabBoxConfig to config
    */
    void setConfig(const TabBoxConfig& config);

    /**
    * Call this method to show the TabBoxView. Depending on current
    * configuration this method might not do anything.
    * If highlight windows effect is to be used it will be activated.
    * If the outline has to be shown, it will be shown.
    * Highlight windows and outline are not shown if
    * TabBoxConfig::TabBoxMode is TabBoxConfig::DesktopTabBox.
    * @see TabBoxConfig::isShowTabBox
    * @see TabBoxConfig::isHighlightWindows
    * @see TabBoxConfig::showOutline
    */
    void show();
    /**
    * Hides the TabBoxView if shown.
    * Deactivates highlight windows effect if active.
    * Removes the outline if active.
    * @see show
    */
    void hide(bool abort = false);

    /**
    * Sets the current model index in the view and updates
    * highlight windows and outline if active.
    * @param index The current Model index
    */
    void setCurrentIndex(const QModelIndex& index);
    /**
     * @returns the current index
     **/
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
    * @param desktop The desktop whose index should be retrieved
    * @return The model index of given desktop. If TabBoxMode is not
    * TabBoxConfig::DesktopTabBox an invalid model index will be returned.
    */
    QModelIndex desktopIndex(int desktop) const;
    /**
    * @return The current list of desktops.
    * If TabBoxMode is not TabBoxConfig::DesktopTabBox an empty list will
    * be returned.
    * @see DesktopModel::desktopList
    */
    QList< int > desktopList() const;
    /**
    * @return The desktop for given model index. If the index is not valid
    * or TabBoxMode is not TabBoxConfig::DesktopTabBox -1 will be returned.
    * @see DesktopModel::desktopIndex
    */
    int desktop(const QModelIndex& index) const;

    /**
    * Handles additional grabbed key events by the TabBox controller.
    * @param event The key event which has been grabbed
    */
    virtual void grabbedKeyEvent(QKeyEvent* event) const;
    /**
    * @param pos The position to be tested in global coordinates
    * @return True if the view contains the point, otherwise false.
    */
    bool containsPos(const QPoint& pos) const;
    /**
    * @param client The TabBoxClient whose index should be returned
    * @return Returns the ModelIndex of given TabBoxClient or an invalid ModelIndex
    * if the model does not contain the given TabBoxClient.
    * @see ClientModel::index
    */
    QModelIndex index(QWeakPointer<TabBoxClient> client) const;
    /**
    * @return Returns the current list of TabBoxClients.
    * If TabBoxMode is not TabBoxConfig::ClientTabBox an empty list will
    * be returned.
    * @see ClientModel::clientList
    */
    TabBoxClientList clientList() const;
    /**
    * @param index The index of the client to be returned
    * @return Returns the TabBoxClient at given model index. If
    * the index is invalid, does not point to a Client or the list
    * is empty, NULL will be returned.
    */
    TabBoxClient* client(const QModelIndex& index) const;
    /**
    * @return The first model index. That is the model index at position 0, 0.
    * It is valid, as desktop has at least one desktop and if there are no
    * clients an empty item is created.
    */
    QModelIndex first() const;

    void setEmbedded(WId wid);
    WId embedded() const;
    void setEmbeddedOffset(const QPoint &offset);
    const QPoint &embeddedOffset() const;
    void setEmbeddedSize(const QSize &size);
    const QSize &embeddedSize() const;
    void setEmbeddedAlignment(Qt::Alignment alignment);
    Qt::Alignment embeddedAlignment() const;
    void resetEmbedded();

protected:
    /**
     * Show the outline of the current selected window
     * @param outline The geometry of the window the outline will be shown
     * @since 4.7
     **/
    virtual void showOutline(const QRect &outline) = 0;

    /**
     * Hide previously shown outline
     * @since 4.7
     **/
    virtual void hideOutline() = 0;

    /**
     * Return outline window ids
     * @return The outline window ids given in the order left, top, right, bottom
     * @since 4.7
     **/
    virtual QVector<Window> outlineWindowIds() const = 0;

signals:
    /**
    * This signal is fired when the TabBoxConfig changes
    * @see setConfig
    */
    void configChanged();
    void embeddedChanged(bool enabled);
    void selectedIndexChanged();

private slots:
    void updateHighlightWindows();

private:
    friend class TabBoxHandlerPrivate;
    TabBoxHandlerPrivate* d;
};

/**
* This class is a wrapper around a KWin Client. It is used for accessing the
* required client methods from inside TabBox and has to be implemented in KWin core.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/
class TabBoxClient
{
public:
    TabBoxClient();
    virtual ~TabBoxClient();

    /**
    * @return The caption of the client
    */
    virtual QString caption() const = 0;
    /**
    * @param size Requested size of the icon
    * @return The icon of the client
    */
    virtual QPixmap icon(const QSize& size = QSize(32, 32)) const = 0;
    /**
    * @return The window Id of the client
    */
    virtual WId window() const = 0;
    /**
    * @return Minimized state of the client
    */
    virtual bool isMinimized() const = 0;
    virtual int x() const = 0;
    virtual int y() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual bool isCloseable() const = 0;
    virtual void close() = 0;
    virtual bool isFirstInTabBox() const = 0;
};

/**
 * Pointer to the global TabBoxHandler object.
 **/
extern TabBoxHandler* tabBox;

} // namespace TabBox
} // namespace KWin

#endif // TABBOXHANDLER_H
