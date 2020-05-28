/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_DATA_CONTROL_SOURCE_INTERFACE_H
#define WAYLAND_SERVER_DATA_CONTROL_SOURCE_INTERFACE_H

#include "abstract_data_source.h"

#include <KWaylandServer/kwaylandserver_export.h>

#include "datacontroldevicemanager_interface.h"

namespace KWaylandServer
{

class DataControlSourceInterfacePrivate;

/**
 * @brief Represents the Resource for the zwlr_data_control_source_v1 interface.
 * Lifespan is mapped to the underlying object
 **/
class KWAYLANDSERVER_EXPORT DataControlSourceInterface : public AbstractDataSource
{
    Q_OBJECT

public:
    ~DataControlSourceInterface() override;

    void requestData(const QString &mimeType, qint32 fd) override;
    void cancel() override;

    QStringList mimeTypes() const override;
    wl_client *client() override;

    static DataControlSourceInterface *get(wl_resource *native);

private:
    friend class DataControlDeviceManagerInterfacePrivate;
    explicit DataControlSourceInterface(DataControlDeviceManagerInterface *parent, ::wl_resource *resource);

    QScopedPointer<DataControlSourceInterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::DataControlSourceInterface*)

#endif
