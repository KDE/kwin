/*
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "datasource.h"

namespace KWin
{
namespace Xwl
{
XwlDataSource::~XwlDataSource()
{
    Q_EMIT aboutToBeDestroyed();
}

void XwlDataSource::requestData(const QString &mimeType, qint32 fd)
{
    Q_EMIT dataRequested(mimeType, fd);
}

void XwlDataSource::cancel()
{
}

QStringList XwlDataSource::mimeTypes() const
{
    return m_mimeTypes;
}
void XwlDataSource::setMimeTypes(const QStringList &mimeTypes)
{
    m_mimeTypes = mimeTypes;
}

void XwlDataSource::accept(const QString &mimeType)
{
    m_accepted = !mimeType.isEmpty();
}

KWaylandServer::DataDeviceManagerInterface::DnDActions XwlDataSource::supportedDragAndDropActions() const
{
    return m_supportedDndActions;
}

void XwlDataSource::setSupportedDndActions(KWaylandServer::DataDeviceManagerInterface::DnDActions dndActions)
{
    m_supportedDndActions = dndActions;
    Q_EMIT supportedDragAndDropActionsChanged();
}

KWaylandServer::DataDeviceManagerInterface::DnDAction XwlDataSource::selectedDndAction() const
{
    return m_dndAction;
}

void XwlDataSource::dndAction(KWaylandServer::DataDeviceManagerInterface::DnDAction action)
{
    m_dndAction = action;
}

bool XwlDataSource::isAccepted() const
{
    return m_accepted;
}
}
}
