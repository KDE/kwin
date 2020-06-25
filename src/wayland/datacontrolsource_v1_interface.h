/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_DATA_CONTROL_SOURCE_INTERFACE_H
#define WAYLAND_SERVER_DATA_CONTROL_SOURCE_INTERFACE_H

#include "abstract_data_source.h"

#include <KWaylandServer/kwaylandserver_export.h>

#include "datacontroldevicemanager_v1_interface.h"

namespace KWaylandServer
{

class DataControlSourceV1InterfacePrivate;

/**
 * The DataControlSourceV1Interface class represents the source side in a data transfer.
 *
 * DataControlSourceV1Interface corresponds to the wayland interface zwlr_data_control_source_v1.
 */
class KWAYLANDSERVER_EXPORT DataControlSourceV1Interface : public AbstractDataSource
{
    Q_OBJECT

public:
    ~DataControlSourceV1Interface() override;

    void requestData(const QString &mimeType, qint32 fd) override;
    void cancel() override;

    QStringList mimeTypes() const override;
    wl_client *client() const override;

    static DataControlSourceV1Interface *get(wl_resource *native);

private:
    friend class DataControlDeviceManagerV1InterfacePrivate;
    explicit DataControlSourceV1Interface(DataControlDeviceManagerV1Interface *parent, ::wl_resource *resource);

    QScopedPointer<DataControlSourceV1InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::DataControlSourceV1Interface*)

#endif
