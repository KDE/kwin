/*
    SPDX-FileCopyrightText: 2014, 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QObject>
#include <QSize>

namespace KWayland
{
namespace Client
{
class Compositor;
class ConnectionThread;
class EventQueue;
class Output;
class Registry;
class ShmPool;
class Surface;
}
}

class QThread;
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
    QThread *m_connectionThread;
    KWayland::Client::ConnectionThread *m_connectionThreadObject;
    KWayland::Client::EventQueue *m_eventQueue;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::Output *m_output;
    KWayland::Client::Surface *m_surface;
    KWayland::Client::ShmPool *m_shm;
    QSize m_currentSize;
    QTimer *m_timer;
};
