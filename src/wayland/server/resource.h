/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_RESOURCE_H
#define WAYLAND_SERVER_RESOURCE_H

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

struct wl_client;
struct wl_resource;

namespace KWaylandServer
{

class ClientConnection;
class Global;

/**
 * @brief Represents a bound Resource.
 *
 * A Resource normally gets created by a @link Global @endlink.
 *
 * The Resource is a base class for all specific resources and provides
 * access to various common aspects.
 **/
class KWAYLANDSERVER_EXPORT Resource : public QObject
{
    Q_OBJECT
public:
    virtual ~Resource();
    void create(ClientConnection *client, quint32 version, quint32 id);

    /**
     * @returns the native wl_resource this Resource was created for.
     **/
    wl_resource *resource();
    /**
     * @returns The ClientConnection for which the Resource was created.
     **/
    ClientConnection *client();
    /**
     * @returns The Global which created the Resource.
     **/
    Global *global();
    /**
     * @returns the native parent wl_resource, e.g. the wl_resource bound on the Global
     **/
    wl_resource *parentResource() const;
    /**
     * @returns The id of this Resource if it is created, otherwise @c 0.
     *
     * This is a convenient wrapper for wl_resource_get_id.
     * @since 5.3
     **/
    quint32 id() const;

Q_SIGNALS:
    /**
     * This signal is emitted when the client unbound this Resource.
     * The Resource will be deleted in the next event cycle after this event.
     * @since 5.24
     **/
    void unbound();
    /**
     * This signal is emitted when the client is in the process of unbinding the Resource.
     * In opposite to @link{unbound} the @link{resource} is still valid and allows to perform
     * cleanup tasks. Example: send a keyboard leave for the Surface which is in the process of
     * getting destroyed.
     *
     * @see unbound
     * @since 5.37
     **/
    void aboutToBeUnbound();

protected:
    class Private;
    explicit Resource(Private *d, QObject *parent = nullptr);
    QScopedPointer<Private> d;

};

}

#endif
