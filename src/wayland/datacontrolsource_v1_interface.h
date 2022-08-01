/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include "abstract_data_source.h"
#include "datacontroldevicemanager_v1_interface.h"

namespace KWaylandServer
{
class DataControlSourceV1InterfacePrivate;

/**
 * The DataControlSourceV1Interface class represents the source side in a data transfer.
 *
 * DataControlSourceV1Interface corresponds to the wayland interface zwlr_data_control_source_v1.
 */
class KWIN_EXPORT DataControlSourceV1Interface : public AbstractDataSource
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

    std::unique_ptr<DataControlSourceV1InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::DataControlSourceV1Interface *)
