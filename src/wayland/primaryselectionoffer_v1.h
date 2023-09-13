/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

#include "primaryselectiondevicemanager_v1.h"

struct wl_resource;
namespace KWin
{
class AbstractDataSource;
class PrimarySelectionDeviceV1Interface;
class PrimarySelectionSourceV1Interface;
class PrimarySelectionOfferV1InterfacePrivate;

/**
 * @brief Represents the Resource for the wl_data_offer interface.
 * Lifespan is mapped to the underlying object
 */
class KWIN_EXPORT PrimarySelectionOfferV1Interface : public QObject
{
    Q_OBJECT
public:
    ~PrimarySelectionOfferV1Interface() override;

    void sendAllOffers();
    wl_resource *resource() const;

private:
    friend class PrimarySelectionDeviceV1InterfacePrivate;
    explicit PrimarySelectionOfferV1Interface(AbstractDataSource *source, wl_resource *resource);

    std::unique_ptr<PrimarySelectionOfferV1InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWin::PrimarySelectionOfferV1Interface *)
