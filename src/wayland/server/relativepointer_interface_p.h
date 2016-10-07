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
#ifndef KWAYLAND_SERVER_RELATIVEPOINTER_INTERFACE_P_H
#define KWAYLAND_SERVER_RELATIVEPOINTER_INTERFACE_P_H
#include "relativepointer_interface.h"
#include "resource_p.h"
#include "global_p.h"


namespace KWayland
{
namespace Server
{

class RelativePointerManagerInterface::Private : public Global::Private
{
public:
    RelativePointerInterfaceVersion interfaceVersion;

protected:
    Private(RelativePointerInterfaceVersion interfaceVersion, RelativePointerManagerInterface *q, Display *d, const wl_interface *interface, quint32 version);
    RelativePointerManagerInterface *q;
};

class RelativePointerManagerUnstableV1Interface : public RelativePointerManagerInterface
{
    Q_OBJECT
public:
    explicit RelativePointerManagerUnstableV1Interface(Display *display, QObject *parent = nullptr);
    virtual ~RelativePointerManagerUnstableV1Interface();

private:
    class Private;
};

class RelativePointerInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~RelativePointerInterface();
    void relativeMotion(const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 microseconds);

protected:
    class Private;
    explicit RelativePointerInterface(Private *p, QObject *parent = nullptr);

private:
    Private *d_func() const;
};

class RelativePointerInterface::Private : public Resource::Private
{
public:
    ~Private();
    virtual void relativeMotion(const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 microseconds) = 0;

protected:
    Private(RelativePointerInterface *q, Global *c, wl_resource *parentResource, const wl_interface *interface, const void *implementation);

private:
    RelativePointerInterface *q_func() {
        return reinterpret_cast<RelativePointerInterface *>(q);
    }
};

class RelativePointerUnstableV1Interface : public RelativePointerInterface
{
    Q_OBJECT
public:
    virtual ~RelativePointerUnstableV1Interface();

private:
    explicit RelativePointerUnstableV1Interface(RelativePointerManagerUnstableV1Interface *parent, wl_resource *parentResource);
    friend class RelativePointerManagerUnstableV1Interface;

    class Private;
    Private *d_func() const;
};

}
}

#endif
