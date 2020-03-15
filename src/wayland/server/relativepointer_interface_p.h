/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
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
