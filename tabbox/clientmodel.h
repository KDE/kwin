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

#ifndef CLIENTMODEL_H
#define CLIENTMODEL_H
#include "tabboxhandler.h"

#include <QModelIndex>
/**
* @file
* This file defines the class ClientModel, the model for TabBoxClients.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/

namespace KWin
{
namespace TabBox
{


/**
* The model for TabBoxClients used in TabBox.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/
class ClientModel
    : public QAbstractItemModel
{
public:
    enum {
        ClientRole = Qt::UserRole, ///< The TabBoxClient
        CaptionRole = Qt::UserRole + 1, ///< The caption of TabBoxClient
        DesktopNameRole = Qt::UserRole + 2, ///< The name of the desktop the TabBoxClient is on
        IconRole = Qt::UserRole + 3, // TODO: to be removed
        EmptyRole = Qt::UserRole + 4, ///< Indicates if the model contains TabBoxClients
        WIdRole = Qt::UserRole + 5, ///< The window ID of TabBoxClient
        MinimizedRole = Qt::UserRole + 6 ///< TabBoxClient is minimized
    };
    ClientModel(QObject* parent = 0);
    ~ClientModel();
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;
    virtual QModelIndex parent(const QModelIndex& child) const;
    virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
    QString longestCaption() const;

    /**
    * @param client The TabBoxClient whose index should be returned
    * @return Returns the ModelIndex of given TabBoxClient or an invalid ModelIndex
    * if the model does not contain the given TabBoxClient.
    */
    QModelIndex index(TabBoxClient* client) const;

    /**
    * Generates a new list of TabBoxClients based on the current config.
    * Calling this method will reset the model. If partialReset is true
    * the top of the list is kept as a starting point. If not the the
    * current active client is used as the starting point to generate the
    * list.
    * @param desktop The desktop for which the list should be created
    * @param partialReset Keep the currently selected client or regenerate everything
    */
    void createClientList(int desktop, bool partialReset = false);
    /**
    * This method is provided as a overload for current desktop
    * @see createClientList
    */
    void createClientList(bool partialReset = false);
    /**
    * @return Returns the current list of TabBoxClients.
    */
    TabBoxClientList clientList() const {
        return m_clientList;
    }

private:
    TabBoxClientList m_clientList;
};

} // namespace Tabbox
} // namespace KWin

#endif // CLIENTMODEL_H
