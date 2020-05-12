/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_DATA_CONTROL_OFFER_INTERFACE_H
#define WAYLAND_SERVER_DATA_CONTROL_OFFER_INTERFACE_H

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

#include "datacontroldevicemanager_interface.h"

struct wl_resource;
namespace KWaylandServer
{

class AbstractDataSource;
class DataControlDeviceInterface;
class DataControlSourceInterface;

class DataControlOfferInterfacePrivate;

/**
 * @brief Represents the Resource for the wl_data_offer interface.
 * Lifespan is mapped to the underlying object
 **/
class KWAYLANDSERVER_EXPORT DataControlOfferInterface : public QObject
{
    Q_OBJECT
public:
    ~DataControlOfferInterface() override;

    void sendAllOffers();
    wl_resource *resource() const;

private:
    friend class DataControlDeviceInterfacePrivate;
    explicit DataControlOfferInterface(AbstractDataSource *source, wl_resource *resource);

    QScopedPointer<DataControlOfferInterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::DataControlOfferInterface*)

#endif
