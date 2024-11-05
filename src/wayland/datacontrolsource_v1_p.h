/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2025 David Edmundson <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <qwayland-server-ext-data-control-v1.h>
#include <qwayland-server-wlr-data-control-unstable-v1.h>

namespace KWin
{

class DataControlSourceV1Interface;

class DataControlSourceV1InterfacePrivate
{
public:
    virtual ~DataControlSourceV1InterfacePrivate() = default;

    virtual void sendSend(const QString &mimeType, qint32 fd) = 0;
    virtual void sendCancelled() = 0;
    virtual wl_client *client() = 0;

    DataControlSourceV1Interface *q;
    QStringList mimeTypes;
};

class ExtDataControlSourcePrivate : public DataControlSourceV1InterfacePrivate, public QtWaylandServer::ext_data_control_source_v1
{
public:
    ExtDataControlSourcePrivate(wl_resource *resource);
    void sendSend(const QString &mimeType, qint32 fd) override
    {
        send_send(mimeType, fd);
    }
    void sendCancelled() override
    {
        send_cancelled();
    }
    wl_client *client() override
    {
        return resource()->client();
    };

protected:
    void ext_data_control_source_v1_destroy_resource(Resource *resource) override;
    void ext_data_control_source_v1_offer(Resource *resource, const QString &mime_type) override;
    void ext_data_control_source_v1_destroy(Resource *resource) override;
};

class WlrDataControlSourcePrivate : public DataControlSourceV1InterfacePrivate, public QtWaylandServer::zwlr_data_control_source_v1
{
public:
    WlrDataControlSourcePrivate(wl_resource *resource);
    void sendSend(const QString &mimeType, qint32 fd) override
    {
        send_send(mimeType, fd);
    }
    void sendCancelled() override
    {
        send_cancelled();
    }
    wl_client *client() override
    {
        return resource()->client();
    };

protected:
    void zwlr_data_control_source_v1_destroy_resource(Resource *resource) override;
    void zwlr_data_control_source_v1_offer(Resource *resource, const QString &mime_type) override;
    void zwlr_data_control_source_v1_destroy(Resource *resource) override;
};

}
