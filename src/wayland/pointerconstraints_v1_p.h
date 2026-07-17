/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "pointerconstraints_v1.h"
#include "surface.h"
#include "surface_p.h"

#include <QPointer>

#include "qwayland-server-pointer-constraints-unstable-v1.h"

namespace KWin
{

class PointerConstraintsV1InterfacePrivate : public QtWaylandServer::zwp_pointer_constraints_v1
{
public:
    explicit PointerConstraintsV1InterfacePrivate(Display *display);

protected:
    void zwp_pointer_constraints_v1_lock_pointer(Resource *resource,
                                                 uint32_t id,
                                                 struct ::wl_resource *surface_resource,
                                                 struct ::wl_resource *pointer_resource,
                                                 struct ::wl_resource *region_resource,
                                                 uint32_t lifetime) override;
    void zwp_pointer_constraints_v1_confine_pointer(Resource *resource,
                                                    uint32_t id,
                                                    struct ::wl_resource *surface_resource,
                                                    struct ::wl_resource *pointer_resource,
                                                    struct ::wl_resource *region_resource,
                                                    uint32_t lifetime) override;
    void zwp_pointer_constraints_v1_destroy(Resource *resource) override;
};

class LockedPointerV1InterfacePrivate : public QtWaylandServer::zwp_locked_pointer_v1
{
public:
    static LockedPointerV1InterfacePrivate *get(LockedPointerV1Interface *pointer);

    LockedPointerV1InterfacePrivate(LockedPointerV1Interface *q, SurfaceInterface *surface, PointerConstraintLifetime lifeTime, const RegionF &region, ::wl_resource *resource);
    ~LockedPointerV1InterfacePrivate() override;

    LockedPointerV1Interface *q;
    PointerConstraintLifetime lifeTime;
    bool isLocked = false;
    QPointer<SurfaceInterface> m_surface;

protected:
    void zwp_locked_pointer_v1_destroy_resource(Resource *resource) override;
    void zwp_locked_pointer_v1_destroy(Resource *resource) override;
    void zwp_locked_pointer_v1_set_cursor_position_hint(Resource *resource, wl_fixed_t surface_x, wl_fixed_t surface_y) override;
    void zwp_locked_pointer_v1_set_region(Resource *resource, struct ::wl_resource *region_resource) override;
};

class ConfinedPointerV1InterfacePrivate : public QtWaylandServer::zwp_confined_pointer_v1
{
public:
    static ConfinedPointerV1InterfacePrivate *get(ConfinedPointerV1Interface *pointer);

    ConfinedPointerV1InterfacePrivate(ConfinedPointerV1Interface *q,
                                      SurfaceInterface *surface,
                                      PointerConstraintLifetime lifeTime,
                                      const RegionF &region,
                                      ::wl_resource *resource);
    ~ConfinedPointerV1InterfacePrivate() override;

    ConfinedPointerV1Interface *q;
    PointerConstraintLifetime lifeTime;
    bool isConfined = false;
    QPointer<SurfaceInterface> m_surface;

protected:
    void zwp_confined_pointer_v1_destroy_resource(Resource *resource) override;
    void zwp_confined_pointer_v1_destroy(Resource *resource) override;
    void zwp_confined_pointer_v1_set_region(Resource *resource, struct ::wl_resource *region_resource) override;
};

} // namespace KWin
