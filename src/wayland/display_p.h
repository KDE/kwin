/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <wayland-server-core.h>

#include "utils/filedescriptor.h"
#include <QList>
#include <QSocketNotifier>
#include <QString>

struct wl_resource;

namespace KWin
{
class ClientConnection;
class Display;
class OutputInterface;
class OutputDeviceV2Interface;
class SeatInterface;

class DisplayPrivate
{
public:
    static DisplayPrivate *get(Display *display);
    DisplayPrivate(Display *q);

    void registerSocketName(const QString &socketName);

    Display *q;
    QSocketNotifier *socketNotifier = nullptr;
    wl_display *display = nullptr;
    wl_event_loop *loop = nullptr;
    bool running = false;
    QList<OutputInterface *> outputs;
    QList<OutputDeviceV2Interface *> outputdevicesV2;
    QList<SeatInterface *> seats;
    QList<ClientConnection *> clients;
    QStringList socketNames;
};

/**
 * @brief The SecurityContext is a helper for the SecurityContextProtocol
 * It stays alive whilst closeFd remains open, listening for new connections on listenFd
 * Any new clients created via listenFd are tagged with the appId
 * It is parented to the display
 */
class SecurityContext : public QObject
{
    Q_OBJECT
public:
    SecurityContext(Display *display, FileDescriptor &&listenFd, FileDescriptor &&closeFd, const QString &appId);
    ~SecurityContext() override;

private:
    void onCloseFdActivated();
    void onListenFdActivated(QSocketDescriptor descriptor);
    Display *m_display;
    FileDescriptor m_listenFd;
    FileDescriptor m_closeFd;
    QString m_appId;
};

} // namespace KWin
