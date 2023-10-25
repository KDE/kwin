/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "virtualdesktopmodel.h"
#include "virtualdesktops.h"

namespace KWin
{

VirtualDesktopModel::VirtualDesktopModel(QObject *parent)
    : QAbstractListModel(parent)
{
    VirtualDesktopManager *manager = VirtualDesktopManager::self();

    connect(manager, &VirtualDesktopManager::desktopAdded,
            this, &VirtualDesktopModel::handleVirtualDesktopAdded);
    connect(manager, &VirtualDesktopManager::desktopRemoved,
            this, &VirtualDesktopModel::handleVirtualDesktopRemoved);

    m_virtualDesktops = manager->desktops();
}

VirtualDesktop *VirtualDesktopModel::create(uint position, const QString &name)
{
    return VirtualDesktopManager::self()->createVirtualDesktop(position, name);
}

void VirtualDesktopModel::remove(uint position)
{
    if (position < m_virtualDesktops.count()) {
        VirtualDesktopManager::self()->removeVirtualDesktop(m_virtualDesktops[position]);
    }
}

void VirtualDesktopModel::handleVirtualDesktopAdded(VirtualDesktop *desktop)
{
    const int position = desktop->x11DesktopNumber() - 1;
    beginInsertRows(QModelIndex(), position, position);
    m_virtualDesktops.insert(position, desktop);
    endInsertRows();
}

void VirtualDesktopModel::handleVirtualDesktopRemoved(VirtualDesktop *desktop)
{
    const int index = m_virtualDesktops.indexOf(desktop);
    Q_ASSERT(index != -1);

    beginRemoveRows(QModelIndex(), index, index);
    m_virtualDesktops.removeAt(index);
    endRemoveRows();
}

QHash<int, QByteArray> VirtualDesktopModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = QAbstractListModel::roleNames();
    roleNames.insert(DesktopRole, QByteArrayLiteral("desktop"));
    return roleNames;
}

VirtualDesktop *VirtualDesktopModel::desktopFromIndex(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_virtualDesktops.count()) {
        return nullptr;
    }
    return m_virtualDesktops[index.row()];
}

QVariant VirtualDesktopModel::data(const QModelIndex &index, int role) const
{
    VirtualDesktop *desktop = desktopFromIndex(index);
    if (!desktop) {
        return QVariant();
    }
    switch (role) {
    case Qt::DisplayRole:
    case DesktopRole:
        return QVariant::fromValue(desktop);
    default:
        return QVariant();
    }
}

int VirtualDesktopModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_virtualDesktops.count();
}

} // namespace KWin

#include "moc_virtualdesktopmodel.cpp"
