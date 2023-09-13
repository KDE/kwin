/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

struct wl_resource;

namespace KWin
{
class AbstractDataSource;
class DataControlDeviceManagerV1Interface;
class DataControlDeviceV1InterfacePrivate;
class DataControlOfferV1Interface;
class DataControlSourceV1Interface;
class SeatInterface;
class SurfaceInterface;

/**
 * The DataControlDeviceV1Interface extensions allows clients to manage seat's current selection.
 *
 * DataControlDeviceV1Interface corresponds to the Wayland interface @c zwlr_data_control_device_v1.
 */
class KWIN_EXPORT DataControlDeviceV1Interface : public QObject
{
    Q_OBJECT

public:
    ~DataControlDeviceV1Interface() override;

    SeatInterface *seat() const;
    DataControlSourceV1Interface *selection() const;
    DataControlSourceV1Interface *primarySelection() const;

    void sendSelection(AbstractDataSource *other);

    void sendPrimarySelection(AbstractDataSource *other);

Q_SIGNALS:
    void selectionChanged(KWin::DataControlSourceV1Interface *dataSource);

    void primarySelectionChanged(KWin::DataControlSourceV1Interface *dataSource);

private:
    friend class DataControlDeviceManagerV1InterfacePrivate;
    explicit DataControlDeviceV1Interface(SeatInterface *seat, wl_resource *resource);

    std::unique_ptr<DataControlDeviceV1InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWin::DataControlDeviceV1Interface *)
