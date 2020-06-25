/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_PRIMARY_SELECTION_SOURCE_INTERFACE_H
#define WAYLAND_SERVER_PRIMARY_SELECTION_SOURCE_INTERFACE_H

#include "abstract_data_source.h"

#include <KWaylandServer/kwaylandserver_export.h>

#include "primaryselectiondevicemanager_v1_interface.h"

namespace KWaylandServer
{

class PrimarySelectionSourceV1InterfacePrivate;

/**
 * @brief Represents the Resource for the zwp_primary_selection_source_v1 interface.
 * Lifespan is mapped to the underlying object
 **/
class KWAYLANDSERVER_EXPORT PrimarySelectionSourceV1Interface : public AbstractDataSource
{
    Q_OBJECT
public:
    ~PrimarySelectionSourceV1Interface() override;

    void requestData(const QString &mimeType, qint32 fd) override;
    void cancel() override;

    QStringList mimeTypes() const override;

    static PrimarySelectionSourceV1Interface *get(wl_resource *native);
    wl_client *client() const override;

private:
    friend class PrimarySelectionDeviceManagerV1InterfacePrivate;
    explicit PrimarySelectionSourceV1Interface(PrimarySelectionDeviceManagerV1Interface *parent, ::wl_resource *resource);

    QScopedPointer<PrimarySelectionSourceV1InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::PrimarySelectionSourceV1Interface*)

#endif
