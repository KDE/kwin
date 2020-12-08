/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "desktopmodel.h"
// tabbox
#include "clientmodel.h"
#include "tabboxconfig.h"
#include "tabboxhandler.h"

#include <cmath>

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
    if (!index.isValid() || index.column() != 0)
        return QVariant();

    if (index.parent().isValid()) {
        // parent is valid -> access to Client
        ClientModel *model = m_clientModels[ m_desktopList[ index.internalId() - 1] ];
        return model->data(model->index(index.row(), 0), role);
    }

    const int desktopIndex = index.row();
    if (desktopIndex >= m_desktopList.count())
        return QVariant();
    switch(role) {
    case Qt::DisplayRole:
    case DesktopNameRole:
        return tabBox->desktopName(m_desktopList[ desktopIndex ]);
    case DesktopRole:
        return m_desktopList[ desktopIndex ];
    case ClientModelRole:
        return QVariant::fromValue<void *>(m_clientModels[m_desktopList[desktopIndex]]);
    default:
        return QVariant();
    }
}

QString DesktopModel::longestCaption() const
{
    QString caption;
    for (int desktop : m_desktopList) {
        QString desktopName = tabBox->desktopName(desktop);
        if (desktopName.size() > caption.size()) {
            caption = desktopName;
        }
    }
    return caption;
}

int DesktopModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

int DesktopModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        if (parent.internalId() != 0 || parent.row() >= m_desktopList.count()) {
            return 0;
        }
        const int desktop = m_desktopList.at(parent.row());
        const ClientModel *model = m_clientModels.value(desktop);
        return model->rowCount();
    }
    return m_desktopList.count();
}

QModelIndex DesktopModel::parent(const QModelIndex& child) const
{
    if (!child.isValid() || child.internalId() == 0) {
        return QModelIndex();
    }
    const int row = child.internalId() -1;
    if (row >= m_desktopList.count()) {
        return QModelIndex();
    }
    return createIndex(row, 0);
}

QModelIndex DesktopModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0) {
        return QModelIndex();
    }
    if (row < 0) {
        return QModelIndex();
    }
    if (parent.isValid()) {
        if (parent.row() < 0 || parent.row() >= m_desktopList.count() || parent.internalId() != 0) {
            return QModelIndex();
        }
        const int desktop = m_desktopList.at(parent.row());
        const ClientModel *model = m_clientModels.value(desktop);
        if (row >= model->rowCount()) {
            return QModelIndex();
        }
        return createIndex(row, column, parent.row() + 1);
    }
    if (row > m_desktopList.count() || m_desktopList.isEmpty())
        return QModelIndex();
    return createIndex(row, column);
}

QHash<int, QByteArray> DesktopModel::roleNames() const
{
    return {
        { Qt::DisplayRole, QByteArrayLiteral("display") },
        { DesktopNameRole, QByteArrayLiteral("caption") },
        { DesktopRole, QByteArrayLiteral("desktop") },
        { ClientModelRole, QByteArrayLiteral("client") },
    };
}

QModelIndex DesktopModel::desktopIndex(int desktop) const
{
    if (desktop > m_desktopList.count())
        return QModelIndex();
    return createIndex(m_desktopList.indexOf(desktop), 0);
}

void DesktopModel::createDesktopList()
{
    beginResetModel();
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
    endResetModel();
}

} // namespace Tabbox
} // namespace KWin
