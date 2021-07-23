/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <wayland-server-core.h>

#include <QHash>
#include <QList>
#include <QSocketNotifier>
#include <QString>
#include <QVector>

#include <EGL/egl.h>

struct wl_resource;

namespace KWaylandServer
{
class ClientBufferIntegration;
class ClientBuffer;
class ClientConnection;
class Display;
class OutputInterface;
class OutputDeviceV2Interface;
class SeatInterface;
struct ClientBufferDestroyListener;

class DisplayPrivate
{
public:
    static DisplayPrivate *get(Display *display);
    DisplayPrivate(Display *q);

    void registerSocketName(const QString &socketName);

    void registerClientBuffer(ClientBuffer *clientBuffer);
    void unregisterClientBuffer(ClientBuffer *clientBuffer);

    Display *q;
    QSocketNotifier *socketNotifier = nullptr;
    wl_display *display = nullptr;
    wl_event_loop *loop = nullptr;
    bool running = false;
    QList<OutputInterface *> outputs;
    QList<OutputDeviceV2Interface *> outputdevicesV2;
    QVector<SeatInterface *> seats;
    QVector<ClientConnection *> clients;
    QStringList socketNames;
    EGLDisplay eglDisplay = EGL_NO_DISPLAY;
    QHash<::wl_resource *, ClientBuffer *> resourceToBuffer;
    QHash<ClientBuffer *, ClientBufferDestroyListener *> bufferToListener;
    QList<ClientBufferIntegration *> bufferIntegrations;
};

} // namespace KWaylandServer
