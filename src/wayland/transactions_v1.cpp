/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "wayland/transactions_v1.h"
#include "wayland/display.h"
#include "wayland/subcompositor.h"
#include "wayland/surface_p.h"
#include "wayland/transaction.h"
#include "wayland/transactions_v1_p.h"

namespace KWin
{

static constexpr quint32 s_version = 1;

TransactionsManagerV1Private::TransactionsManagerV1Private(Display *display)
    : QtWaylandServer::wp_transaction_manager_v1(*display, s_version)
{
}

void TransactionsManagerV1Private::wp_transaction_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void TransactionsManagerV1Private::wp_transaction_manager_v1_create_transaction(Resource *resource, uint32_t id)
{
    new TransactionV1(resource->client(), id, resource->version());
}

TransactionsManagerV1::TransactionsManagerV1(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<TransactionsManagerV1Private>(display))
{
}

TransactionsManagerV1::~TransactionsManagerV1()
{
}

TransactionV1::TransactionV1(wl_client *client, int id, int version)
    : QtWaylandServer::wp_transaction_v1(client, id, version)
{
}

void TransactionV1::wp_transaction_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void TransactionV1::wp_transaction_v1_add_surface(Resource *resource, ::wl_resource *surface_resource)
{
    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);

    if (!surfacePrivate->transactionV1) {
        surfacePrivate->transactionV1 = this;

        surfaces.append(surface);
    } else {
        wl_resource_post_error(resource->handle, error_already_used, "already has a transaction");
    }
}

void TransactionV1::wp_transaction_v1_commit(Resource *resource)
{
    Transaction *transaction = new Transaction();
    for (const auto &surface : std::as_const(surfaces)) {
        if (!surface) {
            continue;
        }

        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
        if (!surfacePrivate->transaction) {
            continue;
        }

        if (surface->subSurface() && surface->subSurface()->isSynchronized()) {
            continue;
        }

        transaction->merge(surfacePrivate->transaction.get());
        surfacePrivate->transaction.reset();
    }

    transaction->commit();
    wl_resource_destroy(resource->handle);
}

} // namespace KWin

#include "moc_transactions_v1.cpp"
