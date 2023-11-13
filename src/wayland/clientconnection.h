/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <sys/types.h>

#include <QObject>
#include <memory>

struct wl_client;
struct wl_resource;

namespace KWin
{
class ClientConnectionPrivate;
class Display;

/**
 * @brief Convenient Class which represents a wl_client.
 *
 * The ClientConnection gets automatically created for a wl_client. In particular,
 * the signal @link Display::clientConnected @endlink will be emitted.
 *
 * @see Display
 */
class KWIN_EXPORT ClientConnection : public QObject
{
    Q_OBJECT
public:
    virtual ~ClientConnection();

    /**
     * Flushes the connection to this client. Ensures that all events are pushed to the client.
     */
    void flush();
    /**
     * Get the wl_resource associated with the given @p id.
     */
    wl_resource *getResource(quint32 id) const;

    /**
     * @returns the native wl_client this ClientConnection represents.
     */
    wl_client *client() const;
    /**
     * @returns The Display this ClientConnection is connected to
     */
    Display *display() const;

    /**
     * The pid of the ClientConnection endpoint.
     *
     * Please note: if the ClientConnection got created with @link Display::createClient @endlink
     * the pid will be identical to the process running the KWin::Display.
     *
     * @returns The pid of the connection.
     */
    pid_t processId() const;
    /**
     * The uid of the ClientConnection endpoint.
     *
     * Please note: if the ClientConnection got created with @link Display::createClient @endlink
     * the uid will be identical to the process running the KWin::Display.
     *
     * @returns The uid of the connection.
     */
    uid_t userId() const;
    /**
     * The gid of the ClientConnection endpoint.
     *
     * Please note: if the ClientConnection got created with @link Display::createClient @endlink
     * the gid will be identical to the process running the KWin::Display.
     *
     * @returns The gid of the connection.
     */
    gid_t groupId() const;

    /**
     * The absolute path to the executable.
     *
     * Please note: if the ClientConnection got created with @link Display::createClient @endlink
     * the executablePath will be identical to the process running the KWin::Display.
     *
     * If the executable path cannot be resolved an empty QString is returned.
     *
     * @see processId
     */
    QString executablePath() const;

    /**
     * Cast operator the native wl_client this ClientConnection represents.
     */
    operator wl_client *();
    /**
     * Cast operator the native wl_client this ClientConnection represents.
     */
    operator wl_client *() const;

    /**
     * Destroys this ClientConnection.
     * This is a convenient wrapper around wl_client_destroy. The use case is in combination
     * with ClientConnections created through @link Display::createClient @endlink. E.g. once
     * the process for the ClientConnection exited, the ClientConnection needs to be destroyed, too.
     */
    void destroy();

    /**
     * Set an additional mapping between kwin's logical co-ordinate space and
     * the client's logical co-ordinate space.
     *
     * This is used in the same way as if the client was setting the
     * surface.buffer_scale on every surface i.e a value of 2.0 will make
     * the windows appear smaller on a regular DPI monitor.
     *
     * Only the minimal set of protocols used by xwayland have support.
     *
     * Buffer sizes are unaffected.
     */
    void setScaleOverride(qreal scaleOverride);
    qreal scaleOverride() const;

    void setSecurityContextAppId(const QString &appId);
    QString securityContextAppId() const;

Q_SIGNALS:
    /**
     * This signal is emitted when the client is about to be destroyed.
     */
    void aboutToBeDestroyed();
    /**
     * Signal emitted when the ClientConnection got disconnected from the server.
     */
    void disconnected(KWin::ClientConnection *);

    void scaleOverrideChanged();

private:
    friend class Display;
    explicit ClientConnection(wl_client *c, Display *parent);
    std::unique_ptr<ClientConnectionPrivate> d;
};

}

Q_DECLARE_METATYPE(KWin::ClientConnection *)
