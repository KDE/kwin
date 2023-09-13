/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <qwayland-server-wayland.h>

#include "datadevicemanager.h"

namespace KWin
{
class DataSourceInterface;
class XdgToplevelDragV1Interface;

class DataSourceInterfacePrivate : public QtWaylandServer::wl_data_source
{
public:
    DataSourceInterfacePrivate(DataSourceInterface *_q, ::wl_resource *resource);

    static DataSourceInterfacePrivate *get(DataSourceInterface *dataSource);

    DataSourceInterface *q;
    QStringList mimeTypes;
    DataDeviceManagerInterface::DnDActions supportedDnDActions = DataDeviceManagerInterface::DnDAction::None;
    DataDeviceManagerInterface::DnDAction selectedDndAction = DataDeviceManagerInterface::DnDAction::None;
    bool isAccepted = false;
    bool dropPerformed = false;
    bool isCanceled = false;
    XdgToplevelDragV1Interface *xdgToplevelDrag = nullptr;

protected:
    void data_source_destroy_resource(Resource *resource) override;
    void data_source_offer(Resource *resource, const QString &mime_type) override;
    void data_source_destroy(Resource *resource) override;
    void data_source_set_actions(Resource *resource, uint32_t dnd_actions) override;

private:
    void offer(const QString &mimeType);
};
}
