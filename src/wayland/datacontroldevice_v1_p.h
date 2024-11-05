/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <qwayland-server-ext-data-control-v1.h>
#include <qwayland-server-wlr-data-control-unstable-v1.h>

#include <QPointer>

namespace KWin
{
class AbstractDataSource;
class DataControlDeviceV1Interface;
class DataControlOfferV1Interface;
class DataControlSourceV1Interface;
class SeatInterface;

class DataControlDeviceV1InterfacePrivate
{
public:
    DataControlDeviceV1InterfacePrivate(SeatInterface *seat);
    virtual ~DataControlDeviceV1InterfacePrivate() = default;

    virtual void sendSelection(AbstractDataSource *other) = 0;
    virtual void sendPrimarySelection(AbstractDataSource *other) = 0;

    DataControlDeviceV1Interface *q;
    QPointer<SeatInterface> seat;
    QPointer<DataControlSourceV1Interface> selection;
    QPointer<DataControlSourceV1Interface> primarySelection;

protected:
    void setSelection(DataControlSourceV1Interface *source);
    void setPrimarySelection(DataControlSourceV1Interface *source);
};

class ExtDataControlDevicePrivate : public DataControlDeviceV1InterfacePrivate, public QtWaylandServer::ext_data_control_device_v1
{
public:
    ExtDataControlDevicePrivate(SeatInterface *seat, wl_resource *resource);

    void sendSelection(AbstractDataSource *other) override;
    void sendPrimarySelection(AbstractDataSource *other) override;

protected:
    void ext_data_control_device_v1_destroy_resource(Resource *resource) override;
    void ext_data_control_device_v1_set_selection(Resource *resource, wl_resource *source) override;
    void ext_data_control_device_v1_set_primary_selection(Resource *resource, struct ::wl_resource *source) override;
    void ext_data_control_device_v1_destroy(Resource *resource) override;
};
class WlrDataControlDevicePrivate : public DataControlDeviceV1InterfacePrivate, public QtWaylandServer::zwlr_data_control_device_v1
{
public:
    WlrDataControlDevicePrivate(SeatInterface *seat, wl_resource *resource);

    void sendSelection(AbstractDataSource *other) override;
    void sendPrimarySelection(AbstractDataSource *other) override;

protected:
    void zwlr_data_control_device_v1_destroy_resource(Resource *resource) override;
    void zwlr_data_control_device_v1_set_selection(Resource *resource, wl_resource *source) override;
    void zwlr_data_control_device_v1_set_primary_selection(Resource *resource, struct ::wl_resource *source) override;
    void zwlr_data_control_device_v1_destroy(Resource *resource) override;
};

}
