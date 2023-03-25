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
    std::shared_ptr<TabBoxClient> client = m_clientList[clientIndex].lock();
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
        return QVariant::fromValue<void *>(client.get());
    case DesktopNameRole: {
        return tabBox->desktopName(client.get());
    }
    case WIdRole:
        return client->internalId();
    case MinimizedRole:
        return client->isMinimized();
    case CloseableRole:
        return client->isCloseable();
    case IconRole:
        return client->icon();
    default:
        return QVariant();
    }
}

QString ClientModel::longestCaption() const
{
    QString caption;
    for (const std::weak_ptr<TabBoxClient> &clientPointer : std::as_const(m_clientList)) {
        std::shared_ptr<TabBoxClient> client = clientPointer.lock();
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

QModelIndex ClientModel::index(std::weak_ptr<TabBoxClient> client) const
{
    const auto it = std::find_if(m_clientList.cbegin(), m_clientList.cend(), [c = client.lock()](auto &client) {
        return client.lock() == c;
    });
    if (it == m_clientList.cend()) {
        return QModelIndex();
    }
    int index = std::distance(m_clientList.cbegin(), it);
    int row = index / columnCount();
    int column = index % columnCount();
    return createIndex(row, column);
}

void ClientModel::createClientList(bool partialReset)
{
    createClientList(tabBox->currentDesktop(), partialReset);
}

void ClientModel::createFocusChainClientList(int desktop, const std::shared_ptr<TabBoxClient> &start)
{
    auto c = start;
    if (!tabBox->isInFocusChain(c.get())) {
        std::shared_ptr<TabBoxClient> firstClient = tabBox->firstClientFocusChain().lock();
        if (firstClient) {
            c = firstClient;
        }
    }
    auto stop = c;
    do {
        std::shared_ptr<TabBoxClient> add = tabBox->clientToAddToList(c.get(), desktop).lock();
        if (add) {
            m_mutableClientList += add;
        }
        c = tabBox->nextClientFocusChain(c.get()).lock();
    } while (c && c != stop);
}

void ClientModel::createStackingOrderClientList(int desktop, const std::shared_ptr<TabBoxClient> &start)
{
    // TODO: needs improvement
    const TabBoxClientList stacking = tabBox->stackingOrder();
    auto c = stacking.first().lock();
    auto stop = c;
    int index = 0;
    while (c) {
        std::shared_ptr<TabBoxClient> add = tabBox->clientToAddToList(c.get(), desktop).lock();
        if (add) {
            if (start == add) {
                m_mutableClientList.erase(std::remove_if(m_mutableClientList.begin(), m_mutableClientList.end(), [&add](auto &client) {
                                              return client.lock() == add;
                                          }),
                                          m_mutableClientList.end());
                m_mutableClientList.prepend(add);
            } else {
                m_mutableClientList += add;
            }
        }
        if (index >= stacking.size() - 1) {
            c = nullptr;
        } else {
            c = stacking[++index].lock();
        }

        if (c == stop) {
            break;
        }
    }
}

void ClientModel::createClientList(int desktop, bool partialReset)
{
    auto start = tabBox->activeClient().lock();
    // TODO: new clients are not added at correct position
    if (partialReset && !m_mutableClientList.isEmpty()) {
        std::shared_ptr<TabBoxClient> firstClient = m_mutableClientList.constFirst().lock();
        if (firstClient) {
            start = firstClient;
        }
    }

    m_mutableClientList.clear();

    switch (tabBox->config().clientSwitchingMode()) {
    case TabBoxConfig::FocusChainSwitching: {
        createFocusChainClientList(desktop, start);
        break;
    }
    case TabBoxConfig::StackingOrderSwitching: {
        createStackingOrderClientList(desktop, start);
        break;
    }
    }

    if (tabBox->config().orderMinimizedMode() == TabBoxConfig::GroupByMinimized) {
        // Put all non-minimized included clients first.
        std::stable_partition(m_mutableClientList.begin(), m_mutableClientList.end(), [](const auto &client) {
            return !client.lock()->isMinimized();
        });
    }

    if (tabBox->config().clientApplicationsMode() != TabBoxConfig::AllWindowsCurrentApplication
        && (tabBox->config().showDesktopMode() == TabBoxConfig::ShowDesktopClient || m_mutableClientList.isEmpty())) {
        std::weak_ptr<TabBoxClient> desktopClient = tabBox->desktopClient();
        if (!desktopClient.expired()) {
            m_mutableClientList.append(desktopClient);
        }
    }

    bool equal = m_clientList.size() == m_mutableClientList.size();
    for (int i = 0; i < m_clientList.size() && equal; i++) {
        equal &= m_clientList[i].lock() == m_mutableClientList[i].lock();
    }
    if (equal) {
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
    std::shared_ptr<TabBoxClient> client = m_mutableClientList.at(i).lock();
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
