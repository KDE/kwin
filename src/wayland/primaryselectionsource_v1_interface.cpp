/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "primaryselectionsource_v1_interface.h"
#include "clientconnection.h"
#include "primaryselectiondevicemanager_v1_interface.h"
#include "utils.h"
// Qt
#include <QStringList>
// Wayland
#include <qwayland-server-wp-primary-selection-unstable-v1.h>
// system
#include <unistd.h>

namespace KWaylandServer
{
class PrimarySelectionSourceV1InterfacePrivate : public QtWaylandServer::zwp_primary_selection_source_v1
{
public:
    PrimarySelectionSourceV1InterfacePrivate(PrimarySelectionSourceV1Interface *q, ::wl_resource *resource);

    QStringList mimeTypes;
    PrimarySelectionSourceV1Interface *q;

protected:
    void zwp_primary_selection_source_v1_destroy_resource(Resource *resource) override;
    void zwp_primary_selection_source_v1_offer(Resource *resource, const QString &mime_type) override;
    void zwp_primary_selection_source_v1_destroy(Resource *resource) override;
};

PrimarySelectionSourceV1InterfacePrivate::PrimarySelectionSourceV1InterfacePrivate(PrimarySelectionSourceV1Interface *_q, ::wl_resource *resource)
    : QtWaylandServer::zwp_primary_selection_source_v1(resource)
    , q(_q)
{
}

void PrimarySelectionSourceV1InterfacePrivate::zwp_primary_selection_source_v1_destroy_resource(
    QtWaylandServer::zwp_primary_selection_source_v1::Resource *resource)
{
    Q_EMIT q->aboutToBeDestroyed();
    delete q;
}

void PrimarySelectionSourceV1InterfacePrivate::zwp_primary_selection_source_v1_offer(Resource *, const QString &mimeType)
{
    mimeTypes << mimeType;
    Q_EMIT q->mimeTypeOffered(mimeType);
}

void PrimarySelectionSourceV1InterfacePrivate::zwp_primary_selection_source_v1_destroy(QtWaylandServer::zwp_primary_selection_source_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

PrimarySelectionSourceV1Interface::PrimarySelectionSourceV1Interface(::wl_resource *resource)
    : d(new PrimarySelectionSourceV1InterfacePrivate(this, resource))
{
}

PrimarySelectionSourceV1Interface::~PrimarySelectionSourceV1Interface() = default;

void PrimarySelectionSourceV1Interface::requestData(const QString &mimeType, qint32 fd)
{
    d->send_send(mimeType, fd);
    close(fd);
}

void PrimarySelectionSourceV1Interface::cancel()
{
    d->send_cancelled();
}

QStringList PrimarySelectionSourceV1Interface::mimeTypes() const
{
    return d->mimeTypes;
}

wl_client *PrimarySelectionSourceV1Interface::client() const
{
    return d->resource()->client();
}

PrimarySelectionSourceV1Interface *PrimarySelectionSourceV1Interface::get(wl_resource *native)
{
    if (auto sourcePrivate = resource_cast<PrimarySelectionSourceV1InterfacePrivate *>(native)) {
        return sourcePrivate->q;
    }
    return nullptr;
}

}
