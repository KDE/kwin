/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_GENERIC_SHELL_SURFACE_P_H
#define KWAYLAND_SERVER_GENERIC_SHELL_SURFACE_P_H

#include "seat_interface.h"
#include "surface_interface.h"
#include "surfacerole_p.h"
#include <wayland-server.h>

namespace KWaylandServer
{

template <class T>
class GenericShellSurface : public SurfaceRole
{
public:
    GenericShellSurface(T *shellSurface, SurfaceInterface *surface)
        : SurfaceRole(surface)
        , surface(surface)
        , shellSurface(shellSurface)
    {}

    SurfaceInterface *surface;
    QString title;
    QByteArray windowClass;

protected:
    void setTitle(const QString &title);
    void setWindowClass(const QByteArray &wc);

    static void moveCallback(wl_client *client, wl_resource *resource, wl_resource *seat, uint32_t serial);
    template <typename U>
    static void resizeCallback(wl_client *client, wl_resource *resource, wl_resource * seat, uint32_t serial, uint32_t edges);
    static void setTitleCallback(wl_client *client, wl_resource *resource, const char *title);
    static void setAppIdCallback(wl_client *client, wl_resource *resource, const char *app_id);

private:
    T *q_func() {
        return shellSurface;
    }
    static typename T::Private *userData(wl_resource *resource) {
        return reinterpret_cast<typename T::Private*>(wl_resource_get_user_data(resource));
    }
    T *shellSurface;
};

template <class T>
void GenericShellSurface<T>::setTitleCallback(wl_client *client, wl_resource *resource, const char *title)
{
    auto s = userData(resource);
    Q_ASSERT(client == *s->client);
    s->setTitle(QString::fromUtf8(title));
}

template <class T>
void GenericShellSurface<T>::setAppIdCallback(wl_client *client, wl_resource *resource, const char *app_id)
{
    auto s = userData(resource);
    Q_ASSERT(client == *s->client);
    s->setWindowClass(QByteArray(app_id));
}

template <class T>
void GenericShellSurface<T>::setTitle(const QString &t)
{
    if (title == t) {
        return;
    }
    title = t;
    Q_Q(T);
    emit q->titleChanged(title);
}

template <class T>
void GenericShellSurface<T>::setWindowClass(const QByteArray &wc)
{
    if (windowClass == wc) {
        return;
    }
    windowClass = wc;
    Q_Q(T);
    emit q->windowClassChanged(windowClass);
}

template <class T>
void GenericShellSurface<T>::moveCallback(wl_client *client, wl_resource *resource, wl_resource *seat, uint32_t serial)
{
    auto s = userData(resource);
    Q_ASSERT(client == *s->client);
    emit s->q_func()->moveRequested(SeatInterface::get(seat), serial);
}

namespace {
template <typename T>
Qt::Edges edgesToQtEdges(T edges);
}

template <class T>
template <typename U>
void GenericShellSurface<T>::resizeCallback(wl_client *client, wl_resource *resource, wl_resource * seat, uint32_t serial, uint32_t edges)
{
    auto s = userData(resource);
    Q_ASSERT(client == *s->client);
    emit s->q_func()->resizeRequested(SeatInterface::get(seat), serial, edgesToQtEdges(U(edges)));
}

}

#endif
