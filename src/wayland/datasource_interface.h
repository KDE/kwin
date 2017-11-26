/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef WAYLAND_SERVER_DATA_SOURCE_INTERFACE_H
#define WAYLAND_SERVER_DATA_SOURCE_INTERFACE_H

#include <QObject>

#include <KWayland/Server/kwaylandserver_export.h>

#include "resource.h"
#include "datadevicemanager_interface.h"

namespace KWayland
{
namespace Server
{

/**
 * @brief Represents the Resource for the wl_data_source interface.
 **/
class KWAYLANDSERVER_EXPORT DataSourceInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~DataSourceInterface();

    void accept(const QString &mimeType);
    void requestData(const QString &mimeType, qint32 fd);
    void cancel();

    QStringList mimeTypes() const;

    static DataSourceInterface *get(wl_resource *native);

    /**
     * @returns The Drag and Drop actions supported by this DataSourceInterface.
     * @since 5.42
     **/
    DataDeviceManagerInterface::DnDActions supportedDragAndDropActions() const;

    /**
     * The user performed the drop action during a drag and drop operation.
     * @since 5.42
     **/
    void dropPerformed();
    /**
     * The drop destination finished interoperating with this data source.
     * @since 5.42
     **/
    void dndFinished();
    /**
     * This event indicates the @p action selected by the compositor after matching the
     * source/destination side actions. Only one action (or none) will be offered here.
     * @since 5.42
     **/
    void dndAction(DataDeviceManagerInterface::DnDAction action);

Q_SIGNALS:
    void mimeTypeOffered(const QString&);
    /**
     * Emitted whenever this DataSourceInterface changes the supported drag and drop actions
     * @since 5.42
     **/
    void supportedDragAndDropActionsChanged();

private:
    friend class DataDeviceManagerInterface;
    explicit DataSourceInterface(DataDeviceManagerInterface *parent, wl_resource *parentResource);

    class Private;
    Private *d_func() const;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::DataSourceInterface*)

#endif
