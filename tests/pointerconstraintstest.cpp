/*
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "pointerconstraintstest.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/region.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/pointerconstraints.h>

#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QCursor>

#include <QDebug>
#include <QScopedPointer>

#include <xcb/xproto.h>

using namespace KWayland::Client;

WaylandBackend::WaylandBackend(QObject *parent)
    : Backend(parent)
    , m_connectionThreadObject(ConnectionThread::fromApplication(this))
{
    setMode(Mode::Wayland);
}

void WaylandBackend::init(QQuickView *view)
{
    Backend::init(view);

    Registry *registry = new Registry(this);
    setupRegistry(registry);
}

void WaylandBackend::setupRegistry(Registry *registry)
{
    connect(registry, &Registry::compositorAnnounced, this,
        [this, registry](quint32 name, quint32 version) {
            m_compositor = registry->createCompositor(name, version, this);
        }
    );
    connect(registry, &Registry::seatAnnounced, this,
        [this, registry](quint32 name, quint32 version) {
            m_seat = registry->createSeat(name, version, this);
            if (m_seat->hasPointer()) {
                m_pointer = m_seat->createPointer(this);
            }
            connect(m_seat, &Seat::hasPointerChanged, this,
                [this]() {
                    delete m_pointer;
                    m_pointer = m_seat->createPointer(this);
                }
            );
        }
    );
    connect(registry, &Registry::pointerConstraintsUnstableV1Announced, this,
        [this, registry](quint32 name, quint32 version) {
            m_pointerConstraints = registry->createPointerConstraints(name, version, this);
        }
    );
    connect(registry, &Registry::interfacesAnnounced, this,
        [this] {
            Q_ASSERT(m_compositor);
            Q_ASSERT(m_seat);
            Q_ASSERT(m_pointerConstraints);
        }
    );
    registry->create(m_connectionThreadObject);
    registry->setup();
}

bool WaylandBackend::isLocked()
{
    return m_lockedPointer && m_lockedPointer->isValid();
}

bool WaylandBackend::isConfined()
{
    return m_confinedPointer && m_confinedPointer->isValid();
}

static PointerConstraints::LifeTime lifeTime(bool persistent)
{
    return persistent ? PointerConstraints::LifeTime::Persistent :
                        PointerConstraints::LifeTime::OneShot;
}

void WaylandBackend::lockRequest(bool persistent, QRect region)
{
    if (isLocked()) {
        if (!errorsAllowed()) {
            qDebug() << "Abort locking because already locked. Allow errors to test relocking (and crashing).";
            return;
        }
        qDebug() << "Trying to lock although already locked. Crash expected.";
    }
    if (isConfined()) {
        if (!errorsAllowed()) {
            qDebug() << "Abort locking because already confined. Allow errors to test locking while being confined (and crashing).";
            return;
        }
        qDebug() << "Trying to lock although already confined. Crash expected.";
    }
    qDebug() << "------ Lock requested ------";
    qDebug() << "Persistent:" << persistent << "| Region:" << region;
    QScopedPointer<Surface> winSurface(Surface::fromWindow(view()));
    QScopedPointer<Region> wlRegion(m_compositor->createRegion(this));
    wlRegion->add(region);

    auto *lockedPointer = m_pointerConstraints->lockPointer(winSurface.data(),
                                                            m_pointer,
                                                            wlRegion.data(),
                                                            lifeTime(persistent),
                                                            this);

    if (!lockedPointer) {
        qDebug() << "ERROR when receiving locked pointer!";
        return;
    }
    m_lockedPointer = lockedPointer;
    m_lockedPointerPersistent = persistent;

    connect(lockedPointer, &LockedPointer::locked, this, [this]() {
        qDebug() << "------ LOCKED! ------";
        if(lockHint()) {
            m_lockedPointer->setCursorPositionHint(QPointF(10., 10.));
            forceSurfaceCommit();
        }

        Q_EMIT lockChanged(true);
    });
    connect(lockedPointer, &LockedPointer::unlocked, this, [this]() {
        qDebug() << "------ UNLOCKED! ------";
        if (!m_lockedPointerPersistent) {
            cleanupLock();
        }
        Q_EMIT lockChanged(false);
    });
}

void WaylandBackend::unlockRequest()
{
    if (!m_lockedPointer) {
        qDebug() << "Unlock requested, but there is no lock. Abort.";
        return;
    }
    qDebug() << "------ Unlock requested ------";
    cleanupLock();
    Q_EMIT lockChanged(false);
}
void WaylandBackend::cleanupLock()
{
    if (!m_lockedPointer) {
        return;
    }
    m_lockedPointer->release();
    m_lockedPointer->deleteLater();
    m_lockedPointer = nullptr;
}

void WaylandBackend::confineRequest(bool persistent, QRect region)
{
    if (isConfined()) {
        if (!errorsAllowed()) {
            qDebug() << "Abort confining because already confined. Allow errors to test reconfining (and crashing).";
            return;
        }
        qDebug() << "Trying to lock although already locked. Crash expected.";
    }
    if (isLocked()) {
        if (!errorsAllowed()) {
            qDebug() << "Abort confining because already locked. Allow errors to test confining while being locked (and crashing).";
            return;
        }
        qDebug() << "Trying to confine although already locked. Crash expected.";
    }
    qDebug() << "------ Confine requested ------";
    qDebug() << "Persistent:" << persistent << "| Region:" << region;
    QScopedPointer<Surface> winSurface(Surface::fromWindow(view()));
    QScopedPointer<Region> wlRegion(m_compositor->createRegion(this));
    wlRegion->add(region);

    auto *confinedPointer = m_pointerConstraints->confinePointer(winSurface.data(),
                                                                 m_pointer,
                                                                 wlRegion.data(),
                                                                 lifeTime(persistent),
                                                                 this);

    if (!confinedPointer) {
        qDebug() << "ERROR when receiving confined pointer!";
        return;
    }
    m_confinedPointer = confinedPointer;
    m_confinedPointerPersistent = persistent;
    connect(confinedPointer, &ConfinedPointer::confined, this, [this]() {
        qDebug() << "------ CONFINED! ------";
        Q_EMIT confineChanged(true);
    });
    connect(confinedPointer, &ConfinedPointer::unconfined, this, [this]() {
        qDebug() << "------ UNCONFINED! ------";
        if (!m_confinedPointerPersistent) {
            cleanupConfine();
        }
        Q_EMIT confineChanged(false);
    });
}
void WaylandBackend::unconfineRequest()
{
    if (!m_confinedPointer) {
        qDebug() << "Unconfine requested, but there is no confine. Abort.";
        return;
    }
    qDebug() << "------ Unconfine requested ------";
    cleanupConfine();
    Q_EMIT confineChanged(false);
}
void WaylandBackend::cleanupConfine()
{
    if (!m_confinedPointer) {
        return;
    }
    m_confinedPointer->release();
    m_confinedPointer->deleteLater();
    m_confinedPointer = nullptr;
}

XBackend::XBackend(QObject *parent)
    : Backend(parent)
{
    setMode(Mode::X);
    if (m_xcbConn) {
        xcb_disconnect(m_xcbConn);
        free(m_xcbConn);
    }
}

void XBackend::init(QQuickView *view)
{
    Backend::init(view);
    m_xcbConn = xcb_connect(nullptr, nullptr);
    if (!m_xcbConn) {
        qDebug() << "Could not open XCB connection.";
    }
}

void XBackend::lockRequest(bool persistent, QRect region)
{
    Q_UNUSED(persistent);
    Q_UNUSED(region);

    auto winId = view()->winId();

    /* Cursor needs to be hidden such that Xwayland emulates warps. */
    QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));

    auto cookie = xcb_warp_pointer_checked(m_xcbConn, /* connection */
                                           XCB_NONE,  /* src_w */
                                           winId,     /* dest_w */
                                           0,         /* src_x */
                                           0,         /* src_y */
                                           0,         /* src_width */
                                           0,         /* src_height */
                                           20,        /* dest_x */
                                           20         /* dest_y */
                                           );
    xcb_flush(m_xcbConn);

    xcb_generic_error_t *error = xcb_request_check(m_xcbConn, cookie);
    if (error) {
        qDebug() << "Lock (warp) failed with XCB error:" << error->error_code;
        free(error);
        return;
    }
    qDebug() << "LOCK (warp)";
    Q_EMIT lockChanged(true);
}

void XBackend::unlockRequest()
{
    /* Xwayland unlocks the pointer, when the cursor is shown again. */
    QGuiApplication::restoreOverrideCursor();
    qDebug() << "------ Unlock requested ------";
    Q_EMIT lockChanged(false);
}

void XBackend::confineRequest(bool persistent, QRect region)
{
    Q_UNUSED(persistent);
    Q_UNUSED(region);

    int error;
    if (!tryConfine(error)) {
        qDebug() << "Confine (grab) failed with XCB error:" << error;
        return;
    }
    qDebug() << "CONFINE (grab)";
    Q_EMIT confineChanged(true);
}

void XBackend::unconfineRequest()
{
    auto cookie = xcb_ungrab_pointer_checked(m_xcbConn, XCB_CURRENT_TIME);
    xcb_flush(m_xcbConn);

    xcb_generic_error_t *error = xcb_request_check(m_xcbConn, cookie);
    if (error) {
        qDebug() << "Unconfine failed with XCB error:" << error->error_code;
        free(error);
        return;
    }
    qDebug() << "UNCONFINE (ungrab)";
    Q_EMIT confineChanged(false);
}

void XBackend::hideAndConfineRequest(bool confineBeforeHide)
{
    if (!confineBeforeHide) {
        QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
    }

    int error;
    if (!tryConfine(error)) {
        qDebug() << "Confine failed with XCB error:" << error;
        if (!confineBeforeHide) {
            QGuiApplication::restoreOverrideCursor();
        }
        return;
    }
    if (confineBeforeHide) {
        QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
    }
    qDebug() << "HIDE AND CONFINE (lock)";
    Q_EMIT confineChanged(true);

}

void XBackend::undoHideRequest()
{
    QGuiApplication::restoreOverrideCursor();
    qDebug() << "UNDO HIDE AND CONFINE (unlock)";
}

bool XBackend::tryConfine(int &error)
{
    auto winId = view()->winId();

    auto cookie = xcb_grab_pointer(m_xcbConn,             /* display */
                                   1,                     /* owner_events */
                                   winId,                 /* grab_window */
                                   0,                     /* event_mask */
                                   XCB_GRAB_MODE_ASYNC,   /* pointer_mode */
                                   XCB_GRAB_MODE_ASYNC,   /* keyboard_mode */
                                   winId,                 /* confine_to */
                                   XCB_NONE,              /* cursor */
                                   XCB_CURRENT_TIME       /* time */
                                   );
    xcb_flush(m_xcbConn);

    xcb_generic_error_t *e = nullptr;
    auto *reply = xcb_grab_pointer_reply(m_xcbConn, cookie, &e);
    if (!reply) {
        error = e->error_code;
        free(e);
        return false;
    }
    free(reply);
    return true;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    Backend *backend;
    if (app.platformName() == QStringLiteral("wayland")) {
        qDebug() << "Starting up: Wayland native mode";
        backend = new WaylandBackend(&app);
    } else {
        qDebug() << "Starting up: Xserver/Xwayland legacy mode";
        backend = new XBackend(&app);
    }

    QQuickView view;

    QQmlContext* context = view.engine()->rootContext();
    context->setContextProperty(QStringLiteral("org_kde_kwin_tests_pointerconstraints_backend"), backend);

    view.setSource(QUrl::fromLocalFile(QStringLiteral(DIR) +QStringLiteral("/pointerconstraintstest.qml")));
    view.show();

    backend->init(&view);

    return app.exec();
}
