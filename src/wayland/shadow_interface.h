/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_SHADOW_INTERFACE_H
#define KWAYLAND_SERVER_SHADOW_INTERFACE_H

#include <QObject>
#include <QMarginsF>

#include <KWaylandServer/kwaylandserver_export.h>

struct wl_resource;

namespace KWaylandServer
{

class BufferInterface;
class Display;
class ShadowManagerInterfacePrivate;
class ShadowInterfacePrivate;

class KWAYLANDSERVER_EXPORT ShadowManagerInterface : public QObject
{
    Q_OBJECT

public:
    explicit ShadowManagerInterface(Display *display, QObject *parent = nullptr);
    ~ShadowManagerInterface() override;

    Display *display() const;

private:
    QScopedPointer<ShadowManagerInterfacePrivate> d;
};

class KWAYLANDSERVER_EXPORT ShadowInterface : public QObject
{
    Q_OBJECT
public:
    ~ShadowInterface() override;

    BufferInterface *left() const;
    BufferInterface *topLeft() const;
    BufferInterface *top() const;
    BufferInterface *topRight() const;
    BufferInterface *right() const;
    BufferInterface *bottomRight() const;
    BufferInterface *bottom() const;
    BufferInterface *bottomLeft() const;

    QMarginsF offset() const;

private:
    explicit ShadowInterface(ShadowManagerInterface *manager, wl_resource *resource);
    friend class ShadowManagerInterfacePrivate;

    QScopedPointer<ShadowInterfacePrivate> d;
};

}

#endif
