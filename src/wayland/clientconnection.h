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
#ifndef KWAYLAND_SERVER_CLIENTCONNECTION_H
#define KWAYLAND_SERVER_CLIENTCONNECTION_H

#include <QObject>

#include <KWayland/Server/kwaylandserver_export.h>

struct wl_client;
struct wl_interface;
struct wl_resource;

namespace KWayland
{
namespace Server
{

class Display;

class KWAYLANDSERVER_EXPORT ClientConnection : public QObject
{
    Q_OBJECT
public:
    virtual ~ClientConnection();

    void flush();
    wl_resource *createResource(const wl_interface *interface, quint32 version, quint32 id);

    wl_client *client();
    Display *display();

    pid_t processId() const;
    uid_t userId() const;
    gid_t groupId() const;

    operator wl_client*();
    operator wl_client*() const;

Q_SIGNALS:
    void disconnected(KWayland::Server::ClientConnection*);

private:
    friend class Display;
    explicit ClientConnection(wl_client *c, Display *parent);
    class Private;
    QScopedPointer<Private> d;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::ClientConnection*)

#endif
