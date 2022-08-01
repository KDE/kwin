/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QList>
#include <QObject>

#include "clientconnection.h"

struct wl_client;
struct wl_display;

namespace KWaylandServer
{
/**
 * @short KWayland Server.
 *
 * This namespace groups all classes related to the Server module.
 *
 * The main entry point into the KWaylandServer API is the Display class.
 * It allows to create a Wayland server and create various global objects on it.
 *
 * KWaylandServer is an API to easily create a head-less Wayland server with a
 * Qt style API.
 *
 * @see Display
 */

class ClientBuffer;
class ClientConnection;
class DisplayPrivate;
class OutputInterface;
class OutputDeviceV2Interface;
class SeatInterface;

/**
 * @brief Class holding the Wayland server display loop.
 *
 * @todo Improve documentation
 */
class KWIN_EXPORT Display : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
public:
    explicit Display(QObject *parent = nullptr);
    virtual ~Display();

    /**
     * Adds a socket with the given @p fileDescriptor to the Wayland display. This function
     * returns @c true if the socket has been added successfully; otherwise returns @c false.
     *
     * The compositor can call this function even after the display has been started.
     * @arg socketName can optionally be parsed to store the socket name represented by the given file-descriptor
     *
     * @see start()
     */
    bool addSocketFileDescriptor(int fileDescriptor, const QString &socketName = QString());
    /**
     * Adds a UNIX socket with the specified @p name to the Wayland display. This function
     * returns @c true if the socket has been added successfully; otherwise returns @c false.
     *
     * If the specified socket name @p name is empty, the display will pick a free socket with
     * a filename "wayland-%d".
     *
     * The compositor can call this function even after the display has been started.
     *
     * @see start()
     */
    bool addSocketName(const QString &name = QString());

    /**
     * Returns the list of socket names that the display listens for client connections.
     */
    QStringList socketNames() const;

    quint32 serial();
    quint32 nextSerial();

    /**
     * Start accepting client connections. If the display has started successfully, this
     * function returns @c true; otherwise @c false is returned.
     */
    bool start();
    void dispatchEvents();

    /**
     * Create a client for the given file descriptor.
     *
     * The client is created as if it connected through the normal server
     * socket. This method can be used to create a connection bypassing the
     * normal socket connection. It's recommended to use together with
     * socketpair and pass the other side of the socket to the client.
     *
     * @param fd The file descriptor for the socket to the client
     * @returns The new ClientConnection or @c null on failure.
     */
    ClientConnection *createClient(int fd);

    operator wl_display *();
    operator wl_display *() const;
    bool isRunning() const;

    void createShm();
    /**
     * @returns All SeatInterface currently managed on the Display.
     */
    QVector<SeatInterface *> seats() const;
    QList<OutputDeviceV2Interface *> outputDevices() const;
    QList<OutputInterface *> outputs() const;
    QVector<OutputInterface *> outputsIntersecting(const QRect &rect) const;

    /**
     * Gets the ClientConnection for the given @p client.
     * If there is no ClientConnection yet for the given @p client, it will be created.
     * @param client The native client for which the ClientConnection is retrieved
     * @return The ClientConnection for the given native client
     */
    ClientConnection *getConnection(wl_client *client);
    QVector<ClientConnection *> connections() const;

    /**
     * Set the EGL @p display for this Wayland display.
     * The EGLDisplay can only be set once and must be alive as long as the Wayland display
     * is alive. The user should have set up the binding between the EGLDisplay and the
     * Wayland display prior to calling this method.
     *
     * @see eglDisplay
     */
    void setEglDisplay(void *display);
    /**
     * @returns the EGLDisplay used for this Wayland display or EGL_NO_DISPLAY if not set.
     * @see setEglDisplay
     */
    void *eglDisplay() const;

    /**
     * Returns the client buffer with the specified @a resource. Returns @c null if there's
     * no such a buffer.
     */
    ClientBuffer *clientBufferForResource(wl_resource *resource) const;

private Q_SLOTS:
    void flush();

Q_SIGNALS:
    void socketNamesChanged();
    void runningChanged(bool);
    void clientConnected(KWaylandServer::ClientConnection *);
    void clientDisconnected(KWaylandServer::ClientConnection *);

private:
    friend class DisplayPrivate;
    std::unique_ptr<DisplayPrivate> d;
};

}
