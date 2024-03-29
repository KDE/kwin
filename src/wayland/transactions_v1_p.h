/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "wayland/transactions_v1.h"

#include "wayland/qwayland-server-transactions-v1.h"

namespace KWin
{

class SurfaceInterface;

class TransactionsManagerV1Private : public QtWaylandServer::wp_transaction_manager_v1
{
public:
    TransactionsManagerV1Private(Display *display);

protected:
    void wp_transaction_manager_v1_destroy(Resource *resource) override;
    void wp_transaction_manager_v1_create_transaction(Resource *resource, uint32_t id) override;
};

class TransactionV1 : public QObject, public QtWaylandServer::wp_transaction_v1
{
    Q_OBJECT

public:
    TransactionV1(wl_client *client, int id, int version);

    QList<QPointer<SurfaceInterface>> surfaces;

protected:
    void wp_transaction_v1_destroy_resource(Resource *resource) override;
    void wp_transaction_v1_add_surface(Resource *resource, struct ::wl_resource *surface) override;
    void wp_transaction_v1_commit(Resource *resource) override;
};

} // namespace KWin
