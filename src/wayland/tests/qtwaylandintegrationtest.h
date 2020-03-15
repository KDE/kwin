/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef QTWAYLANDINTEGRATIONTEST_H
#define QTWAYLANDINTEGRATIONTEST_H

#include <QObject>
#include <QSize>

namespace KWayland
{
namespace Client
{
class Compositor;
class ConnectionThread;
class Registry;
class ShellSurface;
class ShmPool;
class Surface;
}
}

class QTimer;

class WaylandClientTest : public QObject
{
    Q_OBJECT
public:
    explicit WaylandClientTest(QObject *parent = nullptr);
    virtual ~WaylandClientTest();

private:
    void init();
    void render(const QSize &size);
    void render();
    void setupRegistry(KWayland::Client::Registry *registry);
    void toggleTimer();
    KWayland::Client::ConnectionThread *m_connectionThreadObject;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::Surface *m_surface;
    KWayland::Client::ShmPool *m_shm;
    KWayland::Client::ShellSurface *m_shellSurface;
    QSize m_currentSize;
    QTimer *m_timer;
};

#endif
