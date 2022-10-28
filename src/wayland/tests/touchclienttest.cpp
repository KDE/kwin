/*
    SPDX-FileCopyrightText: 2014, 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "touchclienttest.h"
// KWin::Wayland
#include <KWayland/Client/buffer.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>
// Qt
#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QThread>
#include <QTimer>

#include <linux/input.h>

using namespace KWayland::Client;

static Qt::GlobalColor s_colors[] = {Qt::white, Qt::red, Qt::green, Qt::blue, Qt::black};
static int s_colorIndex = 0;

WaylandClientTest::WaylandClientTest(QObject *parent)
    : QObject(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread(nullptr))
    , m_eventQueue(nullptr)
    , m_compositor(nullptr)
    , m_output(nullptr)
    , m_surface(nullptr)
    , m_shm(nullptr)
    , m_timer(new QTimer(this))
{
    init();
}

WaylandClientTest::~WaylandClientTest()
{
    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
}

void WaylandClientTest::init()
{
    connect(
        m_connectionThreadObject,
        &ConnectionThread::connected,
        this,
        [this]() {
            // create the event queue for the main gui thread
            m_eventQueue = new EventQueue(this);
            m_eventQueue->setup(m_connectionThreadObject);
            // setup registry
            Registry *registry = new Registry(this);
            setupRegistry(registry);
        },
        Qt::QueuedConnection);

    m_connectionThreadObject->moveToThread(m_connectionThread);
    m_connectionThread->start();

    m_connectionThreadObject->initConnection();

    connect(m_timer, &QTimer::timeout, this, [this]() {
        s_colorIndex = (s_colorIndex + 1) % 5;
        render();
    });
    m_timer->setInterval(1000);
    m_timer->start();
}

void WaylandClientTest::setupRegistry(Registry *registry)
{
    connect(registry, &Registry::compositorAnnounced, this, [this, registry](quint32 name) {
        m_compositor = registry->createCompositor(name, 1, this);
        m_surface = m_compositor->createSurface(this);
    });
    connect(registry, &Registry::shellAnnounced, this, [this, registry](quint32 name) {
        Shell *shell = registry->createShell(name, 1, this);
        ShellSurface *shellSurface = shell->createSurface(m_surface, m_surface);
        shellSurface->setToplevel();
        render(QSize(400, 200));
    });
    connect(registry, &Registry::outputAnnounced, this, [this, registry](quint32 name) {
        if (m_output) {
            return;
        }
        m_output = registry->createOutput(name, 2, this);
    });
    connect(registry, &Registry::shmAnnounced, this, [this, registry](quint32 name) {
        m_shm = registry->createShmPool(name, 1, this);
    });
    connect(registry, &Registry::seatAnnounced, this, [this, registry](quint32 name) {
        Seat *s = registry->createSeat(name, 2, this);
        connect(s, &Seat::hasKeyboardChanged, this, [this, s](bool has) {
            if (!has) {
                return;
            }
            Keyboard *k = s->createKeyboard(this);
            connect(k, &Keyboard::keyChanged, this, [](quint32 key, Keyboard::KeyState state) {
                if (key == KEY_Q && state == Keyboard::KeyState::Released) {
                    QCoreApplication::instance()->quit();
                }
            });
        });
        connect(s, &Seat::hasPointerChanged, this, [this, s](bool has) {
            if (!has) {
                return;
            }
            Pointer *p = s->createPointer(this);
            connect(p, &Pointer::buttonStateChanged, this, [this](quint32 serial, quint32 time, quint32 button, Pointer::ButtonState state) {
                if (state == Pointer::ButtonState::Released) {
                    if (button == BTN_LEFT) {
                        if (m_timer->isActive()) {
                            m_timer->stop();
                        } else {
                            m_timer->start();
                        }
                    }
                    if (button == BTN_RIGHT) {
                        QCoreApplication::instance()->quit();
                    }
                }
            });
        });
        connect(s, &Seat::hasTouchChanged, this, [this, s](bool has) {
            if (!has) {
                return;
            }
            Touch *t = s->createTouch(this);
            connect(t, &Touch::sequenceStarted, this, [](KWayland::Client::TouchPoint *startPoint) {
                qDebug() << "Touch sequence started at" << startPoint->position() << "with id" << startPoint->id();
            });
            connect(t, &Touch::sequenceCanceled, this, []() {
                qDebug() << "Touch sequence canceled";
            });
            connect(t, &Touch::sequenceEnded, this, []() {
                qDebug() << "Touch sequence finished";
            });
            connect(t, &Touch::frameEnded, this, []() {
                qDebug() << "End of touch contact point list";
            });
            connect(t, &Touch::pointAdded, this, [](KWayland::Client::TouchPoint *point) {
                qDebug() << "Touch point added at" << point->position() << "with id" << point->id();
            });
            connect(t, &Touch::pointRemoved, this, [](KWayland::Client::TouchPoint *point) {
                qDebug() << "Touch point " << point->id() << " removed at" << point->position();
            });
            connect(t, &Touch::pointMoved, this, [](KWayland::Client::TouchPoint *point) {
                qDebug() << "Touch point " << point->id() << " moved to" << point->position();
            });
        });
    });
    registry->create(m_connectionThreadObject->display());
    registry->setEventQueue(m_eventQueue);
    registry->setup();
}

void WaylandClientTest::render(const QSize &size)
{
    m_currentSize = size;
    render();
}

void WaylandClientTest::render()
{
    if (!m_shm || !m_surface || !m_surface->isValid() || !m_currentSize.isValid()) {
        return;
    }
    auto buffer = m_shm->getBuffer(m_currentSize, m_currentSize.width() * 4).toStrongRef();
    buffer->setUsed(true);
    QImage image(buffer->address(), m_currentSize.width(), m_currentSize.height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(s_colors[s_colorIndex]);

    m_surface->attachBuffer(*buffer);
    m_surface->damage(QRect(QPoint(0, 0), m_currentSize));
    m_surface->commit(Surface::CommitFlag::None);
    buffer->setUsed(false);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    new WaylandClientTest(&app);

    return app.exec();
}
