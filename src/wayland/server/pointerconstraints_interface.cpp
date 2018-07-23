/****************************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
****************************************************************************/
#include "pointerconstraints_interface_p.h"

#include <functional>

namespace KWayland
{
namespace Server
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
    connect(this, &LockedPointerInterface::unbound, this, std::bind(&LockedPointerInterface::setLocked, this, false));
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
    connect(this, &ConfinedPointerInterface::unbound, this, std::bind(&ConfinedPointerInterface::setConfined, this, false));
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
}
