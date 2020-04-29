/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "pointerconstraints_interface_p.h"

#include <functional>

namespace KWaylandServer
{

PointerConstraintsInterface::Private::Private(PointerConstraintsInterfaceVersion interfaceVersion, PointerConstraintsInterface *q, Display *d, const wl_interface *interface, quint32 version)
    : Global::Private(d, interface, version)
    , interfaceVersion(interfaceVersion)
    , q(q)
{
}

PointerConstraintsInterface::PointerConstraintsInterface(Private *d, QObject *parent)
    : Global(d, parent)
{
}

PointerConstraintsInterface::~PointerConstraintsInterface() = default;

PointerConstraintsInterfaceVersion PointerConstraintsInterface::interfaceVersion() const
{
    Q_D();
    return d->interfaceVersion;
}

PointerConstraintsInterface::Private *PointerConstraintsInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

LockedPointerInterface::Private::Private(PointerConstraintsInterfaceVersion interfaceVersion, LockedPointerInterface *q, Global *c, wl_resource *parentResource, const wl_interface *interface, const void *implementation)
    : Resource::Private(q, c, parentResource, interface, implementation)
    , interfaceVersion(interfaceVersion)
{
}

LockedPointerInterface::Private::~Private()
{
    if (resource) {
        wl_resource_destroy(resource);
        resource = nullptr;
    }
}

void LockedPointerInterface::Private::commit()
{
    if (regionIsSet) {
        region = pendingRegion;
        pendingRegion = QRegion();
        regionIsSet = false;
        emit q_func()->regionChanged();
    }
    if (hintIsSet) {
        hint = pendingHint;
        hintIsSet = false;
        emit q_func()->cursorPositionHintChanged();
    }
}

LockedPointerInterface::LockedPointerInterface(Private *p, QObject *parent)
    : Resource(p, parent)
{
    connect(this, &LockedPointerInterface::unbound, this, [this]() { setLocked(false); });
}

LockedPointerInterface::~LockedPointerInterface() = default;

PointerConstraintsInterfaceVersion LockedPointerInterface::interfaceVersion() const
{
    Q_D();
    return d->interfaceVersion;
}

LockedPointerInterface::LifeTime LockedPointerInterface::lifeTime() const
{
    Q_D();
    return d->lifeTime;
}

QRegion LockedPointerInterface::region() const
{
    Q_D();
    return d->region;
}

QPointF LockedPointerInterface::cursorPositionHint() const
{
    Q_D();
    return d->hint;
}

bool LockedPointerInterface::isLocked() const
{
    Q_D();
    return d->locked;
}

void LockedPointerInterface::setLocked(bool locked)
{
    Q_D();
    if (locked == d->locked) {
        return;
    }
    if (!locked) {
        d->hint = QPointF(-1., -1.);
    }
    d->locked = locked;
    d->updateLocked();
    emit lockedChanged();
}

LockedPointerInterface::Private *LockedPointerInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

ConfinedPointerInterface::Private::Private(PointerConstraintsInterfaceVersion interfaceVersion, ConfinedPointerInterface *q, Global *c, wl_resource *parentResource, const wl_interface *interface, const void *implementation)
    : Resource::Private(q, c, parentResource, interface, implementation)
    , interfaceVersion(interfaceVersion)
{
}

ConfinedPointerInterface::Private::~Private()
{
    if (resource) {
        wl_resource_destroy(resource);
        resource = nullptr;
    }
}

void ConfinedPointerInterface::Private::commit()
{
    if (!regionIsSet) {
        return;
    }
    region = pendingRegion;
    pendingRegion = QRegion();
    regionIsSet = false;
    emit q_func()->regionChanged();
}

ConfinedPointerInterface::ConfinedPointerInterface(Private *p, QObject *parent)
    : Resource(p, parent)
{
    connect(this, &ConfinedPointerInterface::unbound, this, [this]() { setConfined(false); });
}

ConfinedPointerInterface::~ConfinedPointerInterface() = default;

PointerConstraintsInterfaceVersion ConfinedPointerInterface::interfaceVersion() const
{
    Q_D();
    return d->interfaceVersion;
}

ConfinedPointerInterface::LifeTime ConfinedPointerInterface::lifeTime() const
{
    Q_D();
    return d->lifeTime;
}

QRegion ConfinedPointerInterface::region() const
{
    Q_D();
    return d->region;
}

bool ConfinedPointerInterface::isConfined() const
{
    Q_D();
    return d->confined;
}

void ConfinedPointerInterface::setConfined(bool confined)
{
    Q_D();
    if (confined == d->confined) {
        return;
    }
    d->confined = confined;
    d->updateConfined();
    emit confinedChanged();
}

ConfinedPointerInterface::Private *ConfinedPointerInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
