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
// own
#include "desktopmodel.h"
// tabbox
#include "clientmodel.h"
#include "tabboxconfig.h"
#include "tabboxhandler.h"

#include <math.h>

namespace KWin
{
namespace TabBox
{

DesktopModel::DesktopModel(QObject* parent)
    : QAbstractItemModel(parent)
{
}

DesktopModel::~DesktopModel()
{
}

QVariant DesktopModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int desktopIndex = index.row() * columnCount() + index.column();
    if (desktopIndex >= m_desktopList.count())
        return QVariant();
    switch(role) {
    case Qt::DisplayRole:
    case DesktopNameRole:
        return tabBox->desktopName(m_desktopList[ desktopIndex ]);
    case DesktopRole:
        return m_desktopList[ desktopIndex ];
    case ClientModelRole:
        return qVariantFromValue((void*)m_clientModels[ m_desktopList[ desktopIndex ] ]);
    default:
        return QVariant();
    }
}

int DesktopModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

int DesktopModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return m_desktopList.count();
}

QModelIndex DesktopModel::parent(const QModelIndex& child) const
{
    Q_UNUSED(child)
    return QModelIndex();
}

QModelIndex DesktopModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    int index = row * columnCount() + column;
    if (index > m_desktopList.count() || m_desktopList.isEmpty())
        return QModelIndex();
    return createIndex(row, column);
}

QModelIndex DesktopModel::desktopIndex(int desktop) const
{
    if (desktop > m_desktopList.count())
        return QModelIndex();
    int index = m_desktopList.indexOf(desktop);
    int row = index / columnCount();
    int column = index % columnCount();
    return createIndex(row, column);
}

void DesktopModel::createDesktopList()
{
    m_desktopList.clear();
    qDeleteAll(m_clientModels);
    m_clientModels.clear();

    switch(tabBox->config().desktopSwitchingMode()) {
    case TabBoxConfig::MostRecentlyUsedDesktopSwitching: {
        int desktop = tabBox->currentDesktop();
        do {
            m_desktopList.append(desktop);
            ClientModel* clientModel = new ClientModel(this);
            clientModel->createClientList(desktop);
            m_clientModels.insert(desktop, clientModel);
            desktop = tabBox->nextDesktopFocusChain(desktop);
        } while (desktop != tabBox->currentDesktop());
        break;
    }
    case TabBoxConfig::StaticDesktopSwitching: {
        for (int i = 1; i <= tabBox->numberOfDesktops(); i++) {
            m_desktopList.append(i);
            ClientModel* clientModel = new ClientModel(this);
            clientModel->createClientList(i);
            m_clientModels.insert(i, clientModel);
        }
        break;
    }
    }
    reset();
}

} // namespace Tabbox
} // namespace KWin
