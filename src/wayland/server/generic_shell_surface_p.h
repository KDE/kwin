/********************************************************************
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
*********************************************************************/
#ifndef KWAYLAND_SERVER_GENERIC_SHELL_SURFACE_P_H
#define KWAYLAND_SERVER_GENERIC_SHELL_SURFACE_P_H

#include "seat_interface.h"
#include "surface_interface.h"
#include "surfacerole_p.h"
#include <wayland-server.h>

namespace KWayland
{

namespace Server
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
}

#endif
