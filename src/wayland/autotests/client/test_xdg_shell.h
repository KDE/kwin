/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef TEST_XDG_SHELL_H
#define TEST_XDG_SHELL_H

// Qt
#include <QtTest>
// client
#include "KWayland/Client/xdgshell.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/output.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/surface.h"
// server
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/output_interface.h"
#include "../../src/server/seat_interface.h"
#include "../../src/server/surface_interface.h"
#include "../../src/server/xdgshell_interface.h"

using namespace KWayland::Client;
using namespace KWaylandServer;

Q_DECLARE_METATYPE(Qt::MouseButton)

class XdgShellTest : public QObject
{
    Q_OBJECT

protected:
    XdgShellTest(XdgShellInterfaceVersion version);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreateSurface();
    void testTitle();
    void testWindowClass();
    void testMaximize();
    void testMinimize();
    void testFullscreen();
    void testShowWindowMenu();
    void testMove();
    void testResize_data();
    void testResize();
    void testTransient();
    void testPing();
    void testClose();
    void testConfigureStates_data();
    void testConfigureStates();
    void testConfigureMultipleAcks();

protected:
    XdgShellInterface *m_xdgShellInterface = nullptr;
    Compositor *m_compositor = nullptr;
    XdgShell *m_xdgShell = nullptr;
    Display *m_display = nullptr;
    CompositorInterface *m_compositorInterface = nullptr;
    OutputInterface *m_o1Interface = nullptr;
    OutputInterface *m_o2Interface = nullptr;
    SeatInterface *m_seatInterface = nullptr;
    ConnectionThread *m_connection = nullptr;
    QThread *m_thread = nullptr;
    EventQueue *m_queue = nullptr;
    ShmPool *m_shmPool = nullptr;
    Output *m_output1 = nullptr;
    Output *m_output2 = nullptr;
    Seat *m_seat = nullptr;

private:
    XdgShellInterfaceVersion m_version;
};

#define SURFACE \
    QSignalSpy xdgSurfaceCreatedSpy(m_xdgShellInterface, &XdgShellInterface::surfaceCreated); \
    QVERIFY(xdgSurfaceCreatedSpy.isValid()); \
    QScopedPointer<Surface> surface(m_compositor->createSurface()); \
    QScopedPointer<XdgShellSurface> xdgSurface(m_xdgShell->createSurface(surface.data())); \
    QCOMPARE(xdgSurface->size(), QSize()); \
    QVERIFY(xdgSurfaceCreatedSpy.wait()); \
    auto serverXdgSurface = xdgSurfaceCreatedSpy.first().first().value<XdgShellSurfaceInterface*>(); \
    QVERIFY(serverXdgSurface);

#endif
