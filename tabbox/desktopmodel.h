/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef DESKTOPMODEL_H
#define DESKTOPMODEL_H

#include <QModelIndex>
/**
 * @file
 * This file defines the class DesktopModel, the model for desktops.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 * @since 4.4
 **/

namespace KWin
{
namespace TabBox
{
class ClientModel;

/**
 * The model for desktops used in TabBox.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 * @since 4.4
 **/
class DesktopModel
    : public QAbstractItemModel
{
public:
    enum {
        DesktopRole = Qt::UserRole, ///< Desktop number
        DesktopNameRole = Qt::UserRole + 1, ///< Desktop name
        ClientModelRole = Qt::UserRole + 2 ///< Clients on this desktop
    };
    explicit DesktopModel(QObject* parent = nullptr);
    ~DesktopModel() override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QString longestCaption() const;

    /**
     * Generates a new list of desktops based on the current config.
     * Calling this method will reset the model.
     **/
    void createDesktopList();
    /**
     * @return The current list of desktops.
     **/
    QList< int > desktopList() const {
        return m_desktopList;
    }
    /**
     * @param desktop The desktop whose ModelIndex should be retrieved
     * @return The ModelIndex of given desktop or an invalid ModelIndex if
     * the desktop is not in the model.
     **/
    QModelIndex desktopIndex(int desktop) const;

private:
    QList< int > m_desktopList;
    QMap< int, ClientModel* > m_clientModels;
};

} // namespace Tabbox
} // namespace KWin
#endif // DESKTOPMODEL_H
