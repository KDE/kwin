/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// own
#include "clientmodel.h"
// tabbox
#include "tabboxconfig.h"
// Qt
#include <QIcon>
#include <QUuid>
// TODO: remove with Qt 5, only for HTML escaping the caption
#include <QTextDocument>
// other
#include <cmath>

namespace KWin
{
namespace TabBox
{

ClientModel::ClientModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

ClientModel::~ClientModel()
{
}

QVariant ClientModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (m_clientList.isEmpty()) {
        return QVariant();
    }

    int clientIndex = index.row();
    if (clientIndex >= m_clientList.count()) {
        return QVariant();
    }
    QSharedPointer<TabBoxClient> client = m_clientList[clientIndex].toStrongRef();
    if (!client) {
        return QVariant();
    }
    switch (role) {
    case Qt::DisplayRole:
    case CaptionRole: {
        QString caption = client->caption();
        if (Qt::mightBeRichText(caption)) {
            caption = caption.toHtmlEscaped();
        }
        return caption;
    }
    case ClientRole:
        return QVariant::fromValue<void *>(client.data());
    case DesktopNameRole: {
        return tabBox->desktopName(client.data());
    }
    case WIdRole:
        return client->internalId();
    case MinimizedRole:
        return client->isMinimized();
    case CloseableRole:
        // clients that claim to be first are not closeable
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
    for (const QWeakPointer<TabBoxClient> &clientPointer : std::as_const(m_clientList)) {
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

int ClientModel::columnCount(const QModelIndex &parent) const
{
    return 1;
}

int ClientModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_clientList.count();
}

QModelIndex ClientModel::parent(const QModelIndex &child) const
{
    return QModelIndex();
}

QModelIndex ClientModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column != 0 || parent.isValid()) {
        return QModelIndex();
    }
    int index = row * columnCount();
    if (index >= m_clientList.count() && !m_clientList.isEmpty()) {
        return QModelIndex();
    }
    return createIndex(row, 0);
}

QHash<int, QByteArray> ClientModel::roleNames() const
{
    return {
        {CaptionRole, QByteArrayLiteral("caption")},
        {DesktopNameRole, QByteArrayLiteral("desktopName")},
        {MinimizedRole, QByteArrayLiteral("minimized")},
        {WIdRole, QByteArrayLiteral("windowId")},
        {CloseableRole, QByteArrayLiteral("closeable")},
        {IconRole, QByteArrayLiteral("icon")},
    };
}

QModelIndex ClientModel::index(QWeakPointer<TabBoxClient> client) const
{
    if (!m_clientList.contains(client)) {
        return QModelIndex();
    }
    int index = m_clientList.indexOf(client);
    int row = index / columnCount();
    int column = index % columnCount();
    return createIndex(row, column);
}

void ClientModel::createClientList(bool partialReset)
{
    createClientList(tabBox->currentDesktop(), partialReset);
}

void ClientModel::createFocusChainClientList(int desktop,
    const QSharedPointer<TabBoxClient> &start, TabBoxClientList &stickyClients)
{
    auto c = start;
    if (!tabBox->isInFocusChain(c.data())) {
        QSharedPointer<TabBoxClient> firstClient = tabBox->firstClientFocusChain().toStrongRef();
        if (firstClient) {
            c = firstClient;
        }
    }
    auto stop = c;
    do {
        QSharedPointer<TabBoxClient> add = tabBox->clientToAddToList(c.data(), desktop);
        if (!add.isNull()) {
            m_mutableClientList += add;
            if (add.data()->isFirstInTabBox()) {
                stickyClients << add;
            }
        }
        c = tabBox->nextClientFocusChain(c.data());
    } while (c && c != stop);
}

void ClientModel::createStackingOrderClientList(int desktop,
    const QSharedPointer<TabBoxClient> &start, TabBoxClientList &stickyClients)
{
    // TODO: needs improvement
    const TabBoxClientList stacking = tabBox->stackingOrder();
    auto c = stacking.first().toStrongRef();
    auto stop = c;
    int index = 0;
    while (c) {
        QSharedPointer<TabBoxClient> add = tabBox->clientToAddToList(c.data(), desktop);
        if (!add.isNull()) {
            if (start == add.data()) {
                m_mutableClientList.removeAll(add);
                m_mutableClientList.prepend(add);
            } else {
                m_mutableClientList += add;
            }
            if (add.data()->isFirstInTabBox()) {
                stickyClients << add;
            }
        }
        if (index >= stacking.size() - 1) {
            c = nullptr;
        } else {
            c = stacking[++index];
        }

        if (c == stop) {
            break;
        }
    }
}

void ClientModel::createClientList(int desktop, bool partialReset)
{
    auto start = tabBox->activeClient().toStrongRef();
    // TODO: new clients are not added at correct position
    if (partialReset && !m_mutableClientList.isEmpty()) {
        QSharedPointer<TabBoxClient> firstClient = m_mutableClientList.constFirst();
        if (firstClient) {
            start = firstClient;
        }
    }

    m_mutableClientList.clear();
    TabBoxClientList stickyClients;

    switch (tabBox->config().clientSwitchingMode()) {
    case TabBoxConfig::FocusChainSwitching: {
        createFocusChainClientList(desktop, start, stickyClients);
        break;
    }
    case TabBoxConfig::StackingOrderSwitching: {
        createStackingOrderClientList(desktop, start, stickyClients);
        break;
    }
    }

    if (tabBox->config().orderMinimizedMode() == TabBoxConfig::GroupByMinimized) {
        // Put all non-minimized included clients first.
        std::stable_partition(m_mutableClientList.begin(), m_mutableClientList.end(), [](const auto &client) {
            return !client.toStrongRef()->isMinimized();
        });
    }

    for (const QWeakPointer<TabBoxClient> &c : std::as_const(stickyClients)) {
        m_mutableClientList.removeAll(c);
        m_mutableClientList.prepend(c);
    }
    if (tabBox->config().clientApplicationsMode() != TabBoxConfig::AllWindowsCurrentApplication
        && (tabBox->config().showDesktopMode() == TabBoxConfig::ShowDesktopClient || m_mutableClientList.isEmpty())) {
        QWeakPointer<TabBoxClient> desktopClient = tabBox->desktopClient();
        if (!desktopClient.isNull()) {
            m_mutableClientList.append(desktopClient);
        }
    }

    if (m_clientList == m_mutableClientList) {
        return;
    }

    beginResetModel();
    m_clientList = m_mutableClientList;
    endResetModel();
}

void ClientModel::close(int i)
{
    QModelIndex ind = index(i, 0);
    if (!ind.isValid()) {
        return;
    }
    QSharedPointer<TabBoxClient> client = m_mutableClientList.at(i).toStrongRef();
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
