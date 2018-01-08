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

// own
#include "clientmodel.h"
// tabbox
#include "tabboxconfig.h"
#include "tabboxhandler.h"
// Qt
#include <QIcon>
// TODO: remove with Qt 5, only for HTML escaping the caption
#include <QTextDocument>
#include <QTextStream>
// other
#include <math.h>

namespace KWin
{
namespace TabBox
{

ClientModel::ClientModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    QHash<int, QByteArray> roles;
    roles[CaptionRole] = "caption";
    roles[DesktopNameRole] = "desktopName";
    roles[MinimizedRole] = "minimized";
    roles[WIdRole] = "windowId";
    roles[CloseableRole] = "closeable";
    roles[IconRole] = "icon";
    setRoleNames(roles);
}

ClientModel::~ClientModel()
{
}

QVariant ClientModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (m_clientList.isEmpty()) {
        return QVariant();
    }

    int clientIndex = index.row();
    if (clientIndex >= m_clientList.count())
        return QVariant();
    QSharedPointer<TabBoxClient> client = m_clientList[ clientIndex ].toStrongRef();
    if (!client) {
        return QVariant();
    }
    switch(role) {
    case Qt::DisplayRole:
    case CaptionRole: {
        QString caption = client->caption();
        if (Qt::mightBeRichText(caption)) {
            caption = Qt::escape(caption);
        }
        return caption;
    }
    case ClientRole:
        return qVariantFromValue((void*)client.data());
    case DesktopNameRole: {
        return tabBox->desktopName(client.data());
    }
    case WIdRole:
        return qulonglong(client->window());
    case MinimizedRole:
        return client->isMinimized();
    case CloseableRole:
        //clients that claim to be first are not closeable
        return client->isCloseable() && !client->isFirstInTabBox();
    case IconRole:
        return client->icon();
    default:
        return QVariant();
    }
}

QString ClientModel::longestCaption() const
{
    QString caption;
    foreach (const QWeakPointer<TabBoxClient> &clientPointer, m_clientList) {
        QSharedPointer<TabBoxClient> client = clientPointer.toStrongRef();
        if (!client) {
            continue;
        }
        if (client->caption().size() > caption.size()) {
            caption = client->caption();
        }
    }
    return caption;
}

int ClientModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

int ClientModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_clientList.count();
}

QModelIndex ClientModel::parent(const QModelIndex& child) const
{
    Q_UNUSED(child)
    return QModelIndex();
}

QModelIndex ClientModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || column != 0 || parent.isValid()) {
        return QModelIndex();
    }
    int index = row * columnCount();
    if (index >= m_clientList.count() && !m_clientList.isEmpty())
        return QModelIndex();
    return createIndex(row, 0);
}

QModelIndex ClientModel::index(QWeakPointer<TabBoxClient> client) const
{
    if (!m_clientList.contains(client))
        return QModelIndex();
    int index = m_clientList.indexOf(client);
    int row = index / columnCount();
    int column = index % columnCount();
    return createIndex(row, column);
}

void ClientModel::createClientList(bool partialReset)
{
    createClientList(tabBox->currentDesktop(), partialReset);
}

void ClientModel::createClientList(int desktop, bool partialReset)
{
    TabBoxClient* start = tabBox->activeClient().toStrongRef().data();
    // TODO: new clients are not added at correct position
    if (partialReset && !m_clientList.isEmpty()) {
        QSharedPointer<TabBoxClient> firstClient = m_clientList.first().toStrongRef();
        if (firstClient) {
            start = firstClient.data();
        }
    }

    m_clientList.clear();
    QList< QWeakPointer< TabBoxClient > > stickyClients;

    switch(tabBox->config().clientSwitchingMode()) {
    case TabBoxConfig::FocusChainSwitching: {
        TabBoxClient* c = start;
        if (!tabBox->isInFocusChain(c)) {
            QSharedPointer<TabBoxClient> firstClient = tabBox->firstClientFocusChain().toStrongRef();
            if (firstClient) {
                c = firstClient.data();
            }
        }
        TabBoxClient* stop = c;
        do {
            QWeakPointer<TabBoxClient> add = tabBox->clientToAddToList(c, desktop);
            if (!add.isNull()) {
                m_clientList += add;
                if (add.data()->isFirstInTabBox()) {
                    stickyClients << add;
                }
            }
            c = tabBox->nextClientFocusChain(c).data();
        } while (c && c != stop);
        break;
    }
    case TabBoxConfig::StackingOrderSwitching: {
        // TODO: needs improvement
        TabBoxClientList stacking = tabBox->stackingOrder();
        TabBoxClient* c = stacking.first().data();
        TabBoxClient* stop = c;
        int index = 0;
        while (c) {
            QWeakPointer<TabBoxClient> add = tabBox->clientToAddToList(c, desktop);
            if (!add.isNull()) {
                if (start == add.data()) {
                    m_clientList.removeAll(add);
                    m_clientList.prepend(add);
                } else
                    m_clientList += add;
                if (add.data()->isFirstInTabBox()) {
                    stickyClients << add;
                }
            }
            if (index >= stacking.size() - 1) {
                c = nullptr;
            } else {
                c = stacking[++index].data();
            }

            if (c == stop)
                break;
        }
        break;
    }
    }
    foreach (const QWeakPointer< TabBoxClient > &c, stickyClients) {
        m_clientList.removeAll(c);
        m_clientList.prepend(c);
    }
    if (tabBox->config().clientApplicationsMode() != TabBoxConfig::AllWindowsCurrentApplication
            && (tabBox->config().showDesktopMode() == TabBoxConfig::ShowDesktopClient || m_clientList.isEmpty())) {
        QWeakPointer<TabBoxClient> desktopClient = tabBox->desktopClient();
        if (!desktopClient.isNull())
            m_clientList.append(desktopClient);
    }
    reset();
}

void ClientModel::close(int i)
{
    QModelIndex ind = index(i, 0);
    if (!ind.isValid()) {
        return;
    }
    QSharedPointer<TabBoxClient> client = m_clientList.at(i).toStrongRef();
    if (client) {
        client->close();
    }
}

void ClientModel::activate(int i)
{
    QModelIndex ind = index(i, 0);
    if (!ind.isValid()) {
        return;
    }
    tabBox->setCurrentIndex(ind);
    tabBox->activateAndClose();
}

} // namespace Tabbox
} // namespace KWin
