/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_PRIMARY_SELECTION_DEVICE_INTERFACE_H
#define WAYLAND_SERVER_PRIMARY_SELECTION_DEVICE_INTERFACE_H

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

struct wl_resource;
struct wl_client;

namespace KWaylandServer
{

class AbstractDataSource;
class PrimarySelectionDeviceManagerV1Interface;
class PrimarySelectionOfferV1Interface;
class PrimarySelectionSourceV1Interface;
class SeatInterface;
class SurfaceInterface;
class PrimarySelectionDeviceV1InterfacePrivate;

/**
 * @brief Represents the Resource for the wl_data_device interface.
 *
 * @see SeatInterface
 * @see PrimarySelectionSourceInterface
 * Lifespan is mapped to the underlying object
 **/
class KWAYLANDSERVER_EXPORT PrimarySelectionDeviceV1Interface : public QObject
{
    Q_OBJECT
public:
    virtual ~PrimarySelectionDeviceV1Interface();

    SeatInterface *seat() const;

    PrimarySelectionSourceV1Interface *selection() const;

    void sendSelection(AbstractDataSource *other);
    void sendClearSelection();

    wl_client *client() const;

Q_SIGNALS:
    void selectionChanged(KWaylandServer::PrimarySelectionSourceV1Interface*);
    void selectionCleared();

private:
    friend class PrimarySelectionDeviceManagerV1InterfacePrivate;
    explicit PrimarySelectionDeviceV1Interface(SeatInterface *seat, wl_resource *resource);

    QScopedPointer<PrimarySelectionDeviceV1InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::PrimarySelectionDeviceV1Interface*)

#endif
