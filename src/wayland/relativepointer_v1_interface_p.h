/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "qwayland-server-relative-pointer-unstable-v1.h"

#include <QPointer>

namespace KWaylandServer
{

class Display;
class PointerInterface;

class RelativePointerManagerV1InterfacePrivate : public QtWaylandServer::zwp_relative_pointer_manager_v1
{
public:
    explicit RelativePointerManagerV1InterfacePrivate(Display *display);

protected:
    void zwp_relative_pointer_manager_v1_destroy(Resource *resource) override;
    void zwp_relative_pointer_manager_v1_get_relative_pointer(Resource *resource, uint32_t id,
                                                              struct ::wl_resource *pointer_resource) override;
};

class RelativePointerV1Interface : public QtWaylandServer::zwp_relative_pointer_v1
{
public:
    RelativePointerV1Interface(PointerInterface *pointer, ::wl_resource *resource);
    ~RelativePointerV1Interface() override;

    QPointer<PointerInterface> pointer;

protected:
    void zwp_relative_pointer_v1_destroy_resource(Resource *resource) override;
    void zwp_relative_pointer_v1_destroy(Resource *resource) override;
};

} // namespace KWaylandServer
