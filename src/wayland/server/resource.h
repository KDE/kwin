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
#ifndef WAYLAND_SERVER_RESOURCE_H
#define WAYLAND_SERVER_RESOURCE_H

#include <QObject>

#include <KWayland/Server/kwaylandserver_export.h>

struct wl_client;
struct wl_resource;

namespace KWayland
{
namespace Server
{

class ClientConnection;
class Global;

class KWAYLANDSERVER_EXPORT Resource : public QObject
{
    Q_OBJECT
public:
    virtual ~Resource();
    void create(ClientConnection *client, quint32 version, quint32 id);

    wl_resource *resource();
    ClientConnection *client();
    Global *global();
    wl_resource *parentResource() const;
    /**
     * @returns The id of this Resource if it is created, otherwise @c 0.
     *
     * This is a convenient wrapper for wl_resource_get_id.
     * @since 5.3
     **/
    quint32 id() const;

protected:
    class Private;
    explicit Resource(Private *d, QObject *parent = nullptr);
    QScopedPointer<Private> d;

};

}
}
#endif
