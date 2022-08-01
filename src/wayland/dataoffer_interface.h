/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>

#include "datadevicemanager_interface.h"

#include <optional>

namespace KWaylandServer
{
class DataDeviceInterface;
class AbstractDataSource;
class DataOfferInterfacePrivate;

/**
 * @brief Represents the Resource for the wl_data_offer interface.
 *
 */
class KWIN_EXPORT DataOfferInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~DataOfferInterface();

    void sendAllOffers();
    void sendSourceActions();
    wl_resource *resource() const;

    /**
     * @returns The Drag and Drop actions supported by this DataOfferInterface.
     */
    std::optional<DataDeviceManagerInterface::DnDActions> supportedDragAndDropActions() const;

    /**
     * @returns The preferred Drag and Drop action of this DataOfferInterface.
     */
    std::optional<DataDeviceManagerInterface::DnDAction> preferredDragAndDropAction() const;

    /**
     * This event indicates the @p action selected by the compositor after matching the
     * source/destination side actions. Only one action (or none) will be offered here.
     */
    void dndAction(DataDeviceManagerInterface::DnDAction action);

Q_SIGNALS:
    /**
     * Emitted whenever the supported or preferred Drag and Drop actions changed.
     */
    void dragAndDropActionsChanged();

private:
    friend class DataDeviceInterfacePrivate;
    explicit DataOfferInterface(AbstractDataSource *source, wl_resource *resource);

    std::unique_ptr<DataOfferInterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::DataOfferInterface *)
