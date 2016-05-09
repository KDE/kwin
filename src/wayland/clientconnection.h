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

#include <sys/types.h>

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

/**
 * @brief Convenient Class which represents a wl_client.
 *
 * The ClientConnection gets automatically created for a wl_client when a wl_client is
 * first used in the context of KWayland::Server. In particular the signal
 * @link Display::clientConnected @endlink will be emitted.
 *
 * @see Display
 **/
class KWAYLANDSERVER_EXPORT ClientConnection : public QObject
{
    Q_OBJECT
public:
    virtual ~ClientConnection();

    /**
     * Flushes the connection to this client. Ensures that all events are pushed to the client.
     **/
    void flush();
    /**
     * Creates a new wl_resource for the provided @p interface.
     *
     * Thus a convenient wrapper around wl_resource_create
     *
     * @param interface
     * @param version
     * @param id
     * @returns the created native wl_resource
     **/
    wl_resource *createResource(const wl_interface *interface, quint32 version, quint32 id);
    /**
     * Get the wl_resource associated with the given @p id.
     * @since 5.3
     **/
    wl_resource *getResource(quint32 id);

    /**
     * @returns the native wl_client this ClientConnection represents.
     **/
    wl_client *client();
    /**
     * @returns The Display this ClientConnection is connected to
     **/
    Display *display();

    /**
     * The pid of the ClientConnection endpoint.
     *
     * Please note: if the ClientConnection got created with @link Display::createClient @endlink
     * the pid will be identical to the process running the KWayland::Server::Display.
     *
     * @returns The pid of the connection.
     **/
    pid_t processId() const;
    /**
     * The uid of the ClientConnection endpoint.
     *
     * Please note: if the ClientConnection got created with @link Display::createClient @endlink
     * the uid will be identical to the process running the KWayland::Server::Display.
     *
     * @returns The uid of the connection.
     **/
    uid_t userId() const;
    /**
     * The gid of the ClientConnection endpoint.
     *
     * Please note: if the ClientConnection got created with @link Display::createClient @endlink
     * the gid will be identical to the process running the KWayland::Server::Display.
     *
     * @returns The gid of the connection.
     **/
    gid_t groupId() const;

    /**
     * The absolute path to the executable.
     *
     * Please note: if the ClientConnection got created with @link Display::createClient @endlink
     * the executablePath will be identical to the process running the KWayland::Server::Display.
     *
     * If the executable path cannot be resolved an empty QString is returned.
     *
     * @see processId
     * @since 5.6
     **/
    QString executablePath() const;

    /**
     * Cast operator the native wl_client this ClientConnection represents.
     **/
    operator wl_client*();
    /**
     * Cast operator the native wl_client this ClientConnection represents.
     **/
    operator wl_client*() const;

    /**
     * Destroys this ClientConnection.
     * This is a convenient wrapper around wl_client_destroy. The use case is in combination
     * with ClientConnections created through @link Display::createClient @endlink. E.g. once
     * the process for the ClientConnection exited, the ClientConnection needs to be destroyed, too.
     * @since 5.5
     **/
    void destroy();

Q_SIGNALS:
    /**
     * Signal emitted when the ClientConnection got disconnected from the server.
     **/
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
