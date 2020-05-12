/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_DATA_SOURCE_INTERFACE_H
#define WAYLAND_SERVER_DATA_SOURCE_INTERFACE_H

#include "abstract_data_source.h"

#include <KWaylandServer/kwaylandserver_export.h>

#include "datadevicemanager_interface.h"

namespace KWaylandServer
{

/**
 * @brief Represents the Resource for the wl_data_source interface.
 **/
class KWAYLANDSERVER_EXPORT DataSourceInterface : public AbstractDataSource
{
    Q_OBJECT
public:
    virtual ~DataSourceInterface();

    void accept(const QString &mimeType) override;
    void requestData(const QString &mimeType, qint32 fd) override;
    void cancel() override;

    QStringList mimeTypes() const override;

    static DataSourceInterface *get(wl_resource *native);

    /**
     * @returns The Drag and Drop actions supported by this DataSourceInterface.
     * @since 5.42
     **/
    DataDeviceManagerInterface::DnDActions supportedDragAndDropActions() const override;

    void dropPerformed() override;
    void dndFinished() override;
    void dndAction(DataDeviceManagerInterface::DnDAction action) override;

private:
    friend class DataDeviceManagerInterface;
    explicit DataSourceInterface(DataDeviceManagerInterface *parent, wl_resource *parentResource);

    class Private;
    Private *d_func() const;
};

}

Q_DECLARE_METATYPE(KWaylandServer::DataSourceInterface*)

#endif
