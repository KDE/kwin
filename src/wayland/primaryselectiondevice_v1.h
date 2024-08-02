/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

struct wl_resource;
struct wl_client;

namespace KWin
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
 */
class KWIN_EXPORT PrimarySelectionDeviceV1Interface : public QObject
{
    Q_OBJECT
public:
    virtual ~PrimarySelectionDeviceV1Interface();

    SeatInterface *seat() const;

    PrimarySelectionSourceV1Interface *selection() const;

    void sendSelection(AbstractDataSource *other);

    wl_client *client() const;

Q_SIGNALS:
    void selectionChanged(KWin::PrimarySelectionSourceV1Interface *, quint32 serial);

private:
    friend class PrimarySelectionDeviceManagerV1InterfacePrivate;
    explicit PrimarySelectionDeviceV1Interface(SeatInterface *seat, wl_resource *resource);

    std::unique_ptr<PrimarySelectionDeviceV1InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWin::PrimarySelectionDeviceV1Interface *)
