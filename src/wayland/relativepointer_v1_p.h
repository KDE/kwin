/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "qwayland-server-relative-pointer-unstable-v1.h"

#include <QPointF>

namespace KWaylandServer
{
class ClientConnection;
class Display;
class PointerInterface;

class RelativePointerManagerV1InterfacePrivate : public QtWaylandServer::zwp_relative_pointer_manager_v1
{
public:
    explicit RelativePointerManagerV1InterfacePrivate(Display *display);

protected:
    void zwp_relative_pointer_manager_v1_destroy(Resource *resource) override;
    void zwp_relative_pointer_manager_v1_get_relative_pointer(Resource *resource, uint32_t id, struct ::wl_resource *pointer_resource) override;
};

class RelativePointerV1Interface : public QtWaylandServer::zwp_relative_pointer_v1
{
public:
    explicit RelativePointerV1Interface(PointerInterface *pointer);

    static RelativePointerV1Interface *get(PointerInterface *pointer);
    void sendRelativeMotion(const QPointF &delta, const QPointF &deltaNonAccelerated, std::chrono::microseconds time);

protected:
    void zwp_relative_pointer_v1_destroy(Resource *resource) override;

private:
    PointerInterface *pointer;
};

} // namespace KWaylandServer
