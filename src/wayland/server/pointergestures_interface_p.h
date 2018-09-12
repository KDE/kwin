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
#ifndef KWAYLAND_SERVER_POINTERGESTURES_INTERFACE_P_H
#define KWAYLAND_SERVER_POINTERGESTURES_INTERFACE_P_H

#include "pointergestures_interface.h"
#include "resource_p.h"
#include "global_p.h"

namespace KWayland
{
namespace Server
{

class PointerInterface;

class PointerGesturesInterface::Private : public Global::Private
{
public:
    PointerGesturesInterfaceVersion interfaceVersion;

protected:
    Private(PointerGesturesInterfaceVersion interfaceVersion, PointerGesturesInterface *q, Display *d, const wl_interface *interface, quint32 version);
    PointerGesturesInterface *q;
};

class PointerGesturesUnstableV1Interface : public PointerGesturesInterface
{
    Q_OBJECT
public:
    explicit PointerGesturesUnstableV1Interface(Display *display, QObject *parent = nullptr);
    virtual ~PointerGesturesUnstableV1Interface();

private:
    class Private;
};

class PointerSwipeGestureInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~PointerSwipeGestureInterface();

    virtual void start(quint32 serial, quint32 fingerCount) = 0;
    virtual void update(const QSizeF &delta) = 0;
    virtual void end(quint32 serial) = 0;
    virtual void cancel(quint32 serial) = 0;

protected:
    class Private;
    explicit PointerSwipeGestureInterface(Private *p, QObject *parent = nullptr);

private:
    Private *d_func() const;
};

class PointerSwipeGestureInterface::Private : public Resource::Private
{
public:
    ~Private();

    PointerInterface *pointer;

protected:
    Private(PointerSwipeGestureInterface *q, Global *c, wl_resource *parentResource, const wl_interface *interface, const void *implementation, PointerInterface *pointer);

private:
    PointerSwipeGestureInterface *q_func() {
        return reinterpret_cast<PointerSwipeGestureInterface *>(q);
    }
};

class PointerPinchGestureInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~PointerPinchGestureInterface();

    virtual void start(quint32 serial, quint32 fingerCount) = 0;
    virtual void update(const QSizeF &delta, qreal scale, qreal rotation) = 0;
    virtual void end(quint32 serial) = 0;
    virtual void cancel(quint32 serial) = 0;

protected:
    class Private;
    explicit PointerPinchGestureInterface(Private *p, QObject *parent = nullptr);

private:
    Private *d_func() const;
};

class PointerPinchGestureInterface::Private : public Resource::Private
{
public:
    ~Private();

    PointerInterface *pointer;

protected:
    Private(PointerPinchGestureInterface *q, Global *c, wl_resource *parentResource, const wl_interface *interface, const void *implementation, PointerInterface *pointer);

private:
    PointerPinchGestureInterface *q_func() {
        return reinterpret_cast<PointerPinchGestureInterface *>(q);
    }
};

class PointerSwipeGestureUnstableV1Interface : public PointerSwipeGestureInterface
{
    Q_OBJECT
public:
    explicit PointerSwipeGestureUnstableV1Interface(PointerGesturesUnstableV1Interface *parent, wl_resource *parentResource, PointerInterface *pointer);
    virtual ~PointerSwipeGestureUnstableV1Interface();

    void start(quint32 serial, quint32 fingerCount) override;
    void update(const QSizeF &delta) override;
    void end(quint32 serial) override;
    void cancel(quint32 serial) override;

private:
    friend class PointerGesturesUnstableV1Interface;
    class Private;
    Private *d_func() const;
};

class PointerPinchGestureUnstableV1Interface : public PointerPinchGestureInterface
{
    Q_OBJECT
public:
    explicit PointerPinchGestureUnstableV1Interface(PointerGesturesUnstableV1Interface *parent, wl_resource *parentResource, PointerInterface *pointer);
    virtual ~PointerPinchGestureUnstableV1Interface();

    void start(quint32 serial, quint32 fingerCount) override;
    void update(const QSizeF &delta, qreal scale, qreal rotation) override;
    void end(quint32 serial) override;
    void cancel(quint32 serial) override;

private:
    friend class PointerGesturesUnstableV1Interface;

    class Private;
    Private *d_func() const;
};

}
}

#endif
