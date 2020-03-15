/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_DATA_OFFER_INTERFACE_H
#define WAYLAND_SERVER_DATA_OFFER_INTERFACE_H

#include <QObject>

#include <KWayland/Server/kwaylandserver_export.h>

#include "resource.h"
#include "datadevicemanager_interface.h"

namespace KWayland
{
namespace Server
{

class DataDeviceInterface;
class DataSourceInterface;

/**
 * @brief Represents the Resource for the wl_data_offer interface.
 *
 **/
class KWAYLANDSERVER_EXPORT DataOfferInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~DataOfferInterface();

    void sendAllOffers();

    /**
     * @returns The Drag and Drop actions supported by this DataOfferInterface.
     * @since 5.42
     **/
    DataDeviceManagerInterface::DnDActions supportedDragAndDropActions() const;

    /**
     * @returns The preferred Drag and Drop action of this DataOfferInterface.
     * @since 5.42
     **/
    DataDeviceManagerInterface::DnDAction preferredDragAndDropAction() const;

    /**
     * This event indicates the @p action selected by the compositor after matching the
     * source/destination side actions. Only one action (or none) will be offered here.
     * @since 5.42
     **/
    void dndAction(DataDeviceManagerInterface::DnDAction action);

Q_SIGNALS:
    /**
     * Emitted whenever the supported or preferred Drag and Drop actions changed.
     * @since 5.42
     **/
    void dragAndDropActionsChanged();

private:
    friend class DataDeviceInterface;
    explicit DataOfferInterface(DataSourceInterface *source, DataDeviceInterface *parentInterface, wl_resource *parentResource);

    class Private;
    Private *d_func() const;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::DataOfferInterface*)

#endif
