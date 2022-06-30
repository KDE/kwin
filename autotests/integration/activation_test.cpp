/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "cursor.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/surface.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_activation-0");

class ActivationTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testSwitchToWindowToLeft();
    void testSwitchToWindowToRight();
    void testSwitchToWindowAbove();
    void testSwitchToWindowBelow();
    void testSwitchToWindowMaximized();
    void testSwitchToWindowFullScreen();

private:
    void stackScreensHorizontally();
    void stackScreensVertically();
};

void ActivationTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void ActivationTest::init()
{
    QVERIFY(Test::setupWaylandConnection());

    workspace()->setActiveOutput(QPoint(640, 512));
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void ActivationTest::cleanup()
{
    Test::destroyWaylandConnection();

    stackScreensHorizontally();
}

void ActivationTest::testSwitchToWindowToLeft()
{
    // This test verifies that "Switch to Window to the Left" shortcut works.

    // Prepare the test environment.
    stackScreensHorizontally();

    // Create several windows on the left screen.
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1);
    QVERIFY(window1->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2);
    QVERIFY(window2->isActive());

    window1->move(QPoint(300, 200));
    window2->move(QPoint(500, 200));

    // Create several windows on the right screen.
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window3);
    QVERIFY(window3->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface4(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface4(Test::createXdgToplevelSurface(surface4.get()));
    Window *window4 = Test::renderAndWaitForShown(surface4.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window4);
    QVERIFY(window4->isActive());

    window3->move(QPoint(1380, 200));
    window4->move(QPoint(1580, 200));

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window3->isActive());

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window2->isActive());

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window1->isActive());

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window4->isActive());

    // Destroy all windows.
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(window1));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(window2));
    shellSurface3.reset();
    QVERIFY(Test::waitForWindowDestroyed(window3));
    shellSurface4.reset();
    QVERIFY(Test::waitForWindowDestroyed(window4));
}

void ActivationTest::testSwitchToWindowToRight()
{
    // This test verifies that "Switch to Window to the Right" shortcut works.

    // Prepare the test environment.
    stackScreensHorizontally();

    // Create several windows on the left screen.
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1);
    QVERIFY(window1->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2);
    QVERIFY(window2->isActive());

    window1->move(QPoint(300, 200));
    window2->move(QPoint(500, 200));

    // Create several windows on the right screen.
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window3);
    QVERIFY(window3->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface4(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface4(Test::createXdgToplevelSurface(surface4.get()));
    Window *window4 = Test::renderAndWaitForShown(surface4.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window4);
    QVERIFY(window4->isActive());

    window3->move(QPoint(1380, 200));
    window4->move(QPoint(1580, 200));

    // Switch to window to the right.
    workspace()->switchWindow(Workspace::DirectionEast);
    QVERIFY(window1->isActive());

    // Switch to window to the right.
    workspace()->switchWindow(Workspace::DirectionEast);
    QVERIFY(window2->isActive());

    // Switch to window to the right.
    workspace()->switchWindow(Workspace::DirectionEast);
    QVERIFY(window3->isActive());

    // Switch to window to the right.
    workspace()->switchWindow(Workspace::DirectionEast);
    QVERIFY(window4->isActive());

    // Destroy all windows.
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(window1));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(window2));
    shellSurface3.reset();
    QVERIFY(Test::waitForWindowDestroyed(window3));
    shellSurface4.reset();
    QVERIFY(Test::waitForWindowDestroyed(window4));
}

void ActivationTest::testSwitchToWindowAbove()
{
    // This test verifies that "Switch to Window Above" shortcut works.

    // Prepare the test environment.
    stackScreensVertically();

    // Create several windows on the top screen.
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1);
    QVERIFY(window1->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2);
    QVERIFY(window2->isActive());

    window1->move(QPoint(200, 300));
    window2->move(QPoint(200, 500));

    // Create several windows on the bottom screen.
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window3);
    QVERIFY(window3->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface4(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface4(Test::createXdgToplevelSurface(surface4.get()));
    Window *window4 = Test::renderAndWaitForShown(surface4.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window4);
    QVERIFY(window4->isActive());

    window3->move(QPoint(200, 1224));
    window4->move(QPoint(200, 1424));

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window3->isActive());

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window2->isActive());

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window1->isActive());

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window4->isActive());

    // Destroy all windows.
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(window1));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(window2));
    shellSurface3.reset();
    QVERIFY(Test::waitForWindowDestroyed(window3));
    shellSurface4.reset();
    QVERIFY(Test::waitForWindowDestroyed(window4));
}

void ActivationTest::testSwitchToWindowBelow()
{
    // This test verifies that "Switch to Window Bottom" shortcut works.

    // Prepare the test environment.
    stackScreensVertically();

    // Create several windows on the top screen.
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1);
    QVERIFY(window1->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2);
    QVERIFY(window2->isActive());

    window1->move(QPoint(200, 300));
    window2->move(QPoint(200, 500));

    // Create several windows on the bottom screen.
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window3);
    QVERIFY(window3->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface4(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface4(Test::createXdgToplevelSurface(surface4.get()));
    Window *window4 = Test::renderAndWaitForShown(surface4.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window4);
    QVERIFY(window4->isActive());

    window3->move(QPoint(200, 1224));
    window4->move(QPoint(200, 1424));

    // Switch to window below.
    workspace()->switchWindow(Workspace::DirectionSouth);
    QVERIFY(window1->isActive());

    // Switch to window below.
    workspace()->switchWindow(Workspace::DirectionSouth);
    QVERIFY(window2->isActive());

    // Switch to window below.
    workspace()->switchWindow(Workspace::DirectionSouth);
    QVERIFY(window3->isActive());

    // Switch to window below.
    workspace()->switchWindow(Workspace::DirectionSouth);
    QVERIFY(window4->isActive());

    // Destroy all windows.
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(window1));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(window2));
    shellSurface3.reset();
    QVERIFY(Test::waitForWindowDestroyed(window3));
    shellSurface4.reset();
    QVERIFY(Test::waitForWindowDestroyed(window4));
}

void ActivationTest::testSwitchToWindowMaximized()
{
    // This test verifies that we switch to the top-most maximized window, i.e.
    // the one that user sees at the moment. See bug 411356.

    // Prepare the test environment.
    stackScreensHorizontally();

    // Create several maximized windows on the left screen.
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    QSignalSpy toplevelConfigureRequestedSpy1(shellSurface1.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy1(shellSurface1->xdgSurface(), &Test::XdgSurface::configureRequested);
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1);
    QVERIFY(window1->isActive());
    QVERIFY(surfaceConfigureRequestedSpy1.wait()); // Wait for the configure event with the activated state.
    workspace()->slotWindowMaximize();
    QVERIFY(surfaceConfigureRequestedSpy1.wait());
    QSignalSpy frameGeometryChangedSpy1(window1, &Window::frameGeometryChanged);
    shellSurface1->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy1.last().at(0).value<quint32>());
    Test::render(surface1.get(), toplevelConfigureRequestedSpy1.last().at(0).toSize(), Qt::red);
    QVERIFY(frameGeometryChangedSpy1.wait());

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    QSignalSpy toplevelConfigureRequestedSpy2(shellSurface2.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy2(shellSurface2->xdgSurface(), &Test::XdgSurface::configureRequested);
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2);
    QVERIFY(window2->isActive());
    QVERIFY(surfaceConfigureRequestedSpy2.wait()); // Wait for the configure event with the activated state.
    workspace()->slotWindowMaximize();
    QVERIFY(surfaceConfigureRequestedSpy2.wait());
    QSignalSpy frameGeometryChangedSpy2(window2, &Window::frameGeometryChanged);
    shellSurface2->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy2.last().at(0).value<quint32>());
    Test::render(surface2.get(), toplevelConfigureRequestedSpy2.last().at(0).toSize(), Qt::red);
    QVERIFY(frameGeometryChangedSpy2.wait());

    const QList<Window *> stackingOrder = workspace()->stackingOrder();
    QVERIFY(stackingOrder.indexOf(window1) < stackingOrder.indexOf(window2));
    QCOMPARE(window1->maximizeMode(), MaximizeFull);
    QCOMPARE(window2->maximizeMode(), MaximizeFull);

    // Create several windows on the right screen.
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window3);
    QVERIFY(window3->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface4(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface4(Test::createXdgToplevelSurface(surface4.get()));
    Window *window4 = Test::renderAndWaitForShown(surface4.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window4);
    QVERIFY(window4->isActive());

    window3->move(QPoint(1380, 200));
    window4->move(QPoint(1580, 200));

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window3->isActive());

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window2->isActive());

    // Switch to window to the left.
    workspace()->switchWindow(Workspace::DirectionWest);
    QVERIFY(window4->isActive());

    // Destroy all windows.
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(window1));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(window2));
    shellSurface3.reset();
    QVERIFY(Test::waitForWindowDestroyed(window3));
    shellSurface4.reset();
    QVERIFY(Test::waitForWindowDestroyed(window4));
}

void ActivationTest::testSwitchToWindowFullScreen()
{
    // This test verifies that we switch to the top-most fullscreen window, i.e.
    // the one that user sees at the moment. See bug 411356.

    // Prepare the test environment.
    stackScreensVertically();

    // Create several maximized windows on the top screen.
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    QSignalSpy toplevelConfigureRequestedSpy1(shellSurface1.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy1(shellSurface1->xdgSurface(), &Test::XdgSurface::configureRequested);
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1);
    QVERIFY(window1->isActive());
    QVERIFY(surfaceConfigureRequestedSpy1.wait()); // Wait for the configure event with the activated state.
    workspace()->slotWindowFullScreen();
    QVERIFY(surfaceConfigureRequestedSpy1.wait());
    QSignalSpy frameGeometryChangedSpy1(window1, &Window::frameGeometryChanged);
    shellSurface1->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy1.last().at(0).value<quint32>());
    Test::render(surface1.get(), toplevelConfigureRequestedSpy1.last().at(0).toSize(), Qt::red);
    QVERIFY(frameGeometryChangedSpy1.wait());

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    QSignalSpy toplevelConfigureRequestedSpy2(shellSurface2.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy2(shellSurface2->xdgSurface(), &Test::XdgSurface::configureRequested);
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window2);
    QVERIFY(window2->isActive());
    QVERIFY(surfaceConfigureRequestedSpy2.wait()); // Wait for the configure event with the activated state.
    workspace()->slotWindowFullScreen();
    QVERIFY(surfaceConfigureRequestedSpy2.wait());
    QSignalSpy frameGeometryChangedSpy2(window2, &Window::frameGeometryChanged);
    shellSurface2->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy2.last().at(0).value<quint32>());
    Test::render(surface2.get(), toplevelConfigureRequestedSpy2.last().at(0).toSize(), Qt::red);
    QVERIFY(frameGeometryChangedSpy2.wait());

    const QList<Window *> stackingOrder = workspace()->stackingOrder();
    QVERIFY(stackingOrder.indexOf(window1) < stackingOrder.indexOf(window2));
    QVERIFY(window1->isFullScreen());
    QVERIFY(window2->isFullScreen());

    // Create several windows on the bottom screen.
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    Window *window3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window3);
    QVERIFY(window3->isActive());

    std::unique_ptr<KWayland::Client::Surface> surface4(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface4(Test::createXdgToplevelSurface(surface4.get()));
    Window *window4 = Test::renderAndWaitForShown(surface4.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window4);
    QVERIFY(window4->isActive());

    window3->move(QPoint(200, 1224));
    window4->move(QPoint(200, 1424));

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window3->isActive());

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window2->isActive());

    // Switch to window above.
    workspace()->switchWindow(Workspace::DirectionNorth);
    QVERIFY(window4->isActive());

    // Destroy all windows.
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(window1));
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(window2));
    shellSurface3.reset();
    QVERIFY(Test::waitForWindowDestroyed(window3));
    shellSurface4.reset();
    QVERIFY(Test::waitForWindowDestroyed(window4));
}

void ActivationTest::stackScreensHorizontally()
{
    // Process pending wl_output bind requests before destroying all outputs.
    QTest::qWait(1);

    const QVector<QRect> screenGeometries{
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    };

    QMetaObject::invokeMethod(kwinApp()->outputBackend(),
                              "setVirtualOutputs",
                              Qt::DirectConnection,
                              Q_ARG(QVector<QRect>, screenGeometries));
}

void ActivationTest::stackScreensVertically()
{
    // Process pending wl_output bind requests before destroying all outputs.
    QTest::qWait(1);

    const QVector<QRect> screenGeometries{
        QRect(0, 0, 1280, 1024),
        QRect(0, 1024, 1280, 1024),
    };

    QMetaObject::invokeMethod(kwinApp()->outputBackend(),
                              "setVirtualOutputs",
                              Qt::DirectConnection,
                              Q_ARG(QVector<QRect>, screenGeometries));
}

}

WAYLANDTEST_MAIN(KWin::ActivationTest)
#include "activation_test.moc"
