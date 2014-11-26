/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef WAYLAND_SERVER_KEYBOARD_INTERFACE_H
#define WAYLAND_SERVER_KEYBOARD_INTERFACE_H

#include <QObject>
#include <QPoint>

#include <KWayland/Server/kwaylandserver_export.h>

struct wl_client;
struct wl_resource;

namespace KWayland
{
namespace Server
{

class SeatInterface;
class SurfaceInterface;

class KWAYLANDSERVER_EXPORT KeyboardInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~KeyboardInterface();

    void createInterfae(wl_client *client, wl_resource *parentResource, uint32_t id);

    SurfaceInterface *focusedSurface() const;

    wl_resource *resource() const;

private:
    void setFocusedSurface(SurfaceInterface *surface, quint32 serial);
    void setKeymap(int fd, quint32 size);
    void updateModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group, quint32 serial);
    void keyPressed(quint32 key, quint32 serial);
    void keyReleased(quint32 key, quint32 serial);
    friend class SeatInterface;
    explicit KeyboardInterface(SeatInterface *parent);

    class Private;
    QScopedPointer<Private> d;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::KeyboardInterface*)

#endif
