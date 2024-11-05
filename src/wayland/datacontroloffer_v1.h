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
 * DataControlOfferV1Interface corresponds to the Wayland interface @c ext_data_control_offer_v1.
 */
class KWIN_EXPORT DataControlOfferV1Interface : public QObject
{
    Q_OBJECT

public:
    ~DataControlOfferV1Interface() override;

    void sendAllOffers();

private:
    template<typename T, typename D>
    friend DataControlOfferV1Interface *createDataOffer(AbstractDataSource *source, D *device);
    friend class ExtDataControlDevicePrivate;
    friend class WlrDataControlDevicePrivate;
    explicit DataControlOfferV1Interface(std::unique_ptr<DataControlOfferV1InterfacePrivate> &&d);

    std::unique_ptr<DataControlOfferV1InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWin::DataControlOfferV1Interface *)
