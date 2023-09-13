/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>

#include "datacontroldevicemanager_v1.h"

struct wl_resource;

namespace KWin
{
class AbstractDataSource;
class DataControlDeviceV1Interface;
class DataControlSourceV1Interface;
class DataControlOfferV1InterfacePrivate;

/**
 * The DataControlOfferV1Interface extension represents a piece of data offered for transfer.
 *
 * DataControlOfferV1Interface corresponds to the Wayland interface @c zwlr_data_control_offer_v1.
 */
class KWIN_EXPORT DataControlOfferV1Interface : public QObject
{
    Q_OBJECT

public:
    ~DataControlOfferV1Interface() override;

    void sendAllOffers();
    wl_resource *resource() const;

private:
    friend class DataControlDeviceV1InterfacePrivate;
    explicit DataControlOfferV1Interface(AbstractDataSource *source, wl_resource *resource);

    std::unique_ptr<DataControlOfferV1InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWin::DataControlOfferV1Interface *)
