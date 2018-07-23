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
#ifndef KWAYLAND_SERVER_POINTERCONSTRAINTS_P_H
#define KWAYLAND_SERVER_POINTERCONSTRAINTS_P_H
#include "pointerconstraints_interface.h"
#include "global_p.h"
#include "resource_p.h"

#include <QRegion>

namespace KWayland
{
namespace Server
{

class PointerConstraintsInterface::Private : public Global::Private
{
public:
    PointerConstraintsInterfaceVersion interfaceVersion;

protected:
    Private(PointerConstraintsInterfaceVersion interfaceVersion, PointerConstraintsInterface *q, Display *d, const wl_interface *interface, quint32 version);
    PointerConstraintsInterface *q;
};

class PointerConstraintsUnstableV1Interface : public PointerConstraintsInterface
{
    Q_OBJECT
public:
    explicit PointerConstraintsUnstableV1Interface(Display *display, QObject *parent = nullptr);
    virtual ~PointerConstraintsUnstableV1Interface();

private:
    class Private;
};

class LockedPointerInterface::Private : public Resource::Private
{
public:
    ~Private();

    virtual void updateLocked() = 0;
    void commit();

    PointerConstraintsInterfaceVersion interfaceVersion;

    LifeTime lifeTime;
    QRegion region;
    bool locked = false;
    QPointF hint = QPointF(-1., -1.);

protected:
    Private(PointerConstraintsInterfaceVersion interfaceVersion, LockedPointerInterface *q, Global *c, wl_resource *parentResource, const wl_interface *interface, const void *implementation);

    QRegion pendingRegion;
    bool regionIsSet = false;

    QPointF pendingHint;
    bool hintIsSet = false;

private:
    LockedPointerInterface *q_func() {
        return reinterpret_cast<LockedPointerInterface *>(q);
    }
};

class LockedPointerUnstableV1Interface : public LockedPointerInterface
{
    Q_OBJECT
public:
    explicit LockedPointerUnstableV1Interface(PointerConstraintsUnstableV1Interface *parent, wl_resource *parentResource);
    virtual ~LockedPointerUnstableV1Interface();

private:
    class Private;
    Private *d_func() const;
    friend class PointerConstraintsUnstableV1Interface;
};

class ConfinedPointerInterface::Private : public Resource::Private
{
public:
    ~Private();

    virtual void updateConfined() = 0;
    void commit();

    PointerConstraintsInterfaceVersion interfaceVersion;

    LifeTime lifeTime;
    QRegion region;

    bool confined = false;

protected:
    Private(PointerConstraintsInterfaceVersion interfaceVersion, ConfinedPointerInterface *q, Global *c, wl_resource *parentResource, const wl_interface *interface, const void *implementation);

    QRegion pendingRegion;
    bool regionIsSet = false;

private:
    ConfinedPointerInterface *q_func() {
        return reinterpret_cast<ConfinedPointerInterface *>(q);
    }
};

class ConfinedPointerUnstableV1Interface : public ConfinedPointerInterface
{
    Q_OBJECT
public:
    explicit ConfinedPointerUnstableV1Interface(PointerConstraintsUnstableV1Interface *parent, wl_resource *parentResource);
    virtual ~ConfinedPointerUnstableV1Interface();

private:
    class Private;
    Private *d_func() const;
    friend class PointerConstraintsUnstableV1Interface;
};

}
}

#endif
