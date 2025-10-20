/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "pointerconstraints_v1.h"
#include "surface.h"

#include <QPointer>

#include "qwayland-server-pointer-constraints-unstable-v1.h"
#include <QPointer>

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

class LockedPointerV1Commit : public SurfaceAttachedState<LockedPointerV1Commit>
{
public:
    std::optional<RegionF> region;
    std::optional<QPointF> hint;
};

class LockedPointerV1InterfacePrivate final : public QtWaylandServer::zwp_locked_pointer_v1, public SurfaceExtension<LockedPointerV1InterfacePrivate, LockedPointerV1Commit>
{
public:
    static LockedPointerV1InterfacePrivate *get(LockedPointerV1Interface *pointer);

    LockedPointerV1InterfacePrivate(LockedPointerV1Interface *q, SurfaceInterface *surface, LockedPointerV1Interface::LifeTime lifeTime, const RegionF &region, ::wl_resource *resource);

    void apply(LockedPointerV1Commit *commit);

    LockedPointerV1Interface *q;
    LockedPointerV1Interface::LifeTime lifeTime;
    RegionF effectiveRegion;
    RegionF region;
    QPointF hint = QPointF(-1, -1);
    bool isLocked = false;

protected:
    void zwp_locked_pointer_v1_destroy_resource(Resource *resource) override;
    void zwp_locked_pointer_v1_destroy(Resource *resource) override;
    void zwp_locked_pointer_v1_set_cursor_position_hint(Resource *resource, wl_fixed_t surface_x, wl_fixed_t surface_y) override;
    void zwp_locked_pointer_v1_set_region(Resource *resource, struct ::wl_resource *region_resource) override;
};

class ConfinedPointerV1Commit : public SurfaceAttachedState<ConfinedPointerV1Commit>
{
public:
    std::optional<RegionF> region;
};

class ConfinedPointerV1InterfacePrivate final : public QtWaylandServer::zwp_confined_pointer_v1, public SurfaceExtension<ConfinedPointerV1InterfacePrivate, ConfinedPointerV1Commit>
{
public:
    static ConfinedPointerV1InterfacePrivate *get(ConfinedPointerV1Interface *pointer);

    ConfinedPointerV1InterfacePrivate(ConfinedPointerV1Interface *q,
                                      SurfaceInterface *surface,
                                      ConfinedPointerV1Interface::LifeTime lifeTime,
                                      const RegionF &region,
                                      ::wl_resource *resource);

    void apply(ConfinedPointerV1Commit *commit);

    ConfinedPointerV1Interface *q;
    ConfinedPointerV1Interface::LifeTime lifeTime;
    RegionF effectiveRegion;
    RegionF region;
    bool isConfined = false;

protected:
    void zwp_confined_pointer_v1_destroy_resource(Resource *resource) override;
    void zwp_confined_pointer_v1_destroy(Resource *resource) override;
    void zwp_confined_pointer_v1_set_region(Resource *resource, struct ::wl_resource *region_resource) override;
};

} // namespace KWin
