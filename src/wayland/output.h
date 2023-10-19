/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "core/output.h"
#include "kwin_export.h"

#include <QObject>
#include <QPoint>
#include <QSize>

struct wl_resource;
struct wl_client;

namespace KWin
{
class Output;
}

namespace KWin
{
class ClientConnection;
class Display;
class OutputInterfacePrivate;

/**
 * The OutputInterface class represents a screen. This class corresponds to the Wayland
 * interface @c wl_output.
 */
class KWIN_EXPORT OutputInterface : public QObject
{
    Q_OBJECT

public:
    explicit OutputInterface(Display *display, Output *handle, QObject *parent = nullptr);
    ~OutputInterface() override;

    bool isRemoved() const;
    void remove();

    Output *handle() const;

    /**
     * @returns all wl_resources bound for the @p client
     */
    QList<wl_resource *> clientResources(wl_client *client) const;

    /**
     * Submit changes to all clients.
     */
    void scheduleDone();

    /**
     * Submit changes to @p client.
     */
    void done(wl_client *client);

    static OutputInterface *get(wl_resource *native);

    Display *display() const;

Q_SIGNALS:
    void removed();

    /**
     * Emitted when a client binds to a given output
     * @internal
     */
    void bound(ClientConnection *client, wl_resource *boundResource);

private:
    std::unique_ptr<OutputInterfacePrivate> d;
};

} // namespace KWin
