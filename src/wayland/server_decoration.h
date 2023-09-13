/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

struct wl_resource;

namespace KWaylandServer
{
class Display;
class ServerSideDecorationInterface;
class SurfaceInterface;
class ServerSideDecorationManagerInterfacePrivate;
class ServerSideDecorationInterfacePrivate;

/**
 * @brief Manager to create ServerSideDecorationInterface.
 */
class KWIN_EXPORT ServerSideDecorationManagerInterface : public QObject
{
    Q_OBJECT

public:
    explicit ServerSideDecorationManagerInterface(Display *display, QObject *parent = nullptr);
    ~ServerSideDecorationManagerInterface() override;

    /**
     * Decoration mode used for SurfaceInterfaces.
     */
    enum class Mode {
        /**
         * Undecorated: neither client, nor server provide decoration. Example: popups.
         */
        None,
        /**
         * The decoration is part of the surface.
         */
        Client,
        /**
         * The surface gets embedded into a decoration frame provided by the Server.
         */
        Server,
    };

    /**
     * Sets the default @p mode which is pushed to the Clients on creating a ServerSideDecorationInterface.
     * @param mode The new default mode.
     * @see defaultMode
     */
    void setDefaultMode(Mode mode);
    /**
     * @returns the current default mode.
     * @see setDefaultMode
     */
    Mode defaultMode() const;

Q_SIGNALS:
    /**
     * Emitted whenever a new ServerSideDecorationInterface is created.
     */
    void decorationCreated(KWaylandServer::ServerSideDecorationInterface *);

private:
    std::unique_ptr<ServerSideDecorationManagerInterfacePrivate> d;
};

/**
 * @brief Representing how a SurfaceInterface should be decorated.
 *
 * Created by ServerSideDecorationManagerInterface and emitted with decorationCreated signal.
 */
class KWIN_EXPORT ServerSideDecorationInterface : public QObject
{
    Q_OBJECT
public:
    ~ServerSideDecorationInterface() override;

    /**
     * Sets the @p mode on the SurfaceInterface. A client might refuse the provided @p mode,
     * in that case modeRequested will be emitted.
     * @see mode
     * @see modeRequested
     */
    void setMode(ServerSideDecorationManagerInterface::Mode mode);
    /**
     * @returns the currently set mode, not the requested mode.
     * @see setMode
     * @see modeRequested
     */
    ServerSideDecorationManagerInterface::Mode mode() const;
    /**
     * Returns the preferred decoration mode as specified by the client.
     */
    ServerSideDecorationManagerInterface::Mode preferredMode() const;

    /**
     * @returns The SurfaceInterface this ServerSideDecorationInterface references.
     */
    SurfaceInterface *surface() const;

    /**
     * @returns The ServerSideDecorationInterface for the given @p surface, @c nullptr if there is none.
     */
    static ServerSideDecorationInterface *get(SurfaceInterface *surface);

Q_SIGNALS:
    /**
     * The client requested the provided mode.
     * The server needs to acknowledge the requested mode by setting it through setMode.
     * @see setMode
     * @see mode
     */
    void preferredModeChanged();

private:
    explicit ServerSideDecorationInterface(ServerSideDecorationManagerInterface *manager, SurfaceInterface *surface, wl_resource *resource);
    friend class ServerSideDecorationManagerInterfacePrivate;

    std::unique_ptr<ServerSideDecorationInterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::ServerSideDecorationInterface *)
Q_DECLARE_METATYPE(KWaylandServer::ServerSideDecorationManagerInterface::Mode)
