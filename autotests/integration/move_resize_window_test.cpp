
/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_output.h"
#include "atoms.h"
#include "platform.h"
#include "abstract_client.h"
#include "x11client.h"
#include "cursor.h"
#include "effects.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "deleted.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/surface.h>

#include <linux/input.h>
#include <xcb/xcb_icccm.h>

Q_DECLARE_METATYPE(KWin::QuickTileMode)
Q_DECLARE_METATYPE(KWin::MaximizeMode)

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_quick_tiling-0");

class MoveResizeWindowTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testMove();
    void testResize();
    void testPackTo_data();
    void testPackTo();
    void testPackAgainstClient_data();
    void testPackAgainstClient();
    void testGrowShrink_data();
    void testGrowShrink();
    void testPointerMoveEnd_data();
    void testPointerMoveEnd();
    void testClientSideMove();
    void testPlasmaShellSurfaceMovable_data();
    void testPlasmaShellSurfaceMovable();
    void testNetMove();
    void testAdjustClientGeometryOfAutohidingX11Panel_data();
    void testAdjustClientGeometryOfAutohidingX11Panel();
    void testAdjustClientGeometryOfAutohidingWaylandPanel_data();
    void testAdjustClientGeometryOfAutohidingWaylandPanel();
    void testResizeForVirtualKeyboard();
    void testResizeForVirtualKeyboardWithMaximize();
    void testResizeForVirtualKeyboardWithFullScreen();
    void testDestroyMoveClient();
    void testDestroyResizeClient();

private:
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
};

void MoveResizeWindowTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient *>();
    qRegisterMetaType<KWin::Deleted *>();
    qRegisterMetaType<KWin::MaximizeMode>("MaximizeMode");
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = kwinApp()->platform()->enabledOutputs();
    QCOMPARE(outputs.count(), 1);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
}

void MoveResizeWindowTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::PlasmaShell | Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandPointer());
    m_connection = Test::waylandConnection();
    m_compositor = Test::waylandCompositor();

    workspace()->setActiveOutput(QPoint(640, 512));
}

void MoveResizeWindowTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void MoveResizeWindowTest::testMove()
{
    using namespace KWayland::Client;

    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    // let's render
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 50));
    QSignalSpy startMoveResizedSpy(c, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(startMoveResizedSpy.isValid());
    QSignalSpy moveResizedChangedSpy(c, &AbstractClient::moveResizedChanged);
    QVERIFY(moveResizedChangedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(c, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(c, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    // effects signal handlers
    QSignalSpy windowStartUserMovedResizedSpy(effects, &EffectsHandler::windowStartUserMovedResized);
    QVERIFY(windowStartUserMovedResizedSpy.isValid());
    QSignalSpy windowStepUserMovedResizedSpy(effects, &EffectsHandler::windowStepUserMovedResized);
    QVERIFY(windowStepUserMovedResizedSpy.isValid());
    QSignalSpy windowFinishUserMovedResizedSpy(effects, &EffectsHandler::windowFinishUserMovedResized);
    QVERIFY(windowFinishUserMovedResizedSpy.isValid());

    // begin move
    QVERIFY(workspace()->moveResizeClient() == nullptr);
    QCOMPARE(c->isInteractiveMove(), false);
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), c);
    QCOMPARE(startMoveResizedSpy.count(), 1);
    QCOMPARE(moveResizedChangedSpy.count(), 1);
    QCOMPARE(windowStartUserMovedResizedSpy.count(), 1);
    QCOMPARE(c->isInteractiveMove(), true);
    QCOMPARE(c->geometryRestore(), QRect());

    // send some key events, not going through input redirection
    const QPoint cursorPos = Cursors::self()->mouse()->pos();
    c->keyPressEvent(Qt::Key_Right);
    c->updateInteractiveMoveResize(Cursors::self()->mouse()->pos());
    QCOMPARE(Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));
    QEXPECT_FAIL("", "First event is ignored", Continue);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    clientStepUserMovedResizedSpy.clear();
    windowStepUserMovedResizedSpy.clear();

    c->keyPressEvent(Qt::Key_Right);
    c->updateInteractiveMoveResize(Cursors::self()->mouse()->pos());
    QCOMPARE(Cursors::self()->mouse()->pos(), cursorPos + QPoint(16, 0));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);
    QCOMPARE(windowStepUserMovedResizedSpy.count(), 1);

    c->keyPressEvent(Qt::Key_Down | Qt::ALT);
    c->updateInteractiveMoveResize(Cursors::self()->mouse()->pos());
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);
    QCOMPARE(windowStepUserMovedResizedSpy.count(), 2);
    QCOMPARE(c->frameGeometry(), QRect(16, 32, 100, 50));
    QCOMPARE(Cursors::self()->mouse()->pos(), cursorPos + QPoint(16, 32));

    // let's end
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 0);
    c->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(moveResizedChangedSpy.count(), 2);
    QCOMPARE(windowFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(c->frameGeometry(), QRect(16, 32, 100, 50));
    QCOMPARE(c->isInteractiveMove(), false);
    QVERIFY(workspace()->moveResizeClient() == nullptr);
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(c));
}

void MoveResizeWindowTest::testResize()
{
    // a test case which manually resizes a window
    using namespace KWayland::Client;

    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QVERIFY(!shellSurface.isNull());

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested);
    QVERIFY(toplevelConfigureRequestedSpy.isValid());
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.isValid());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 1);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    // Let's render.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    // We have to receive a configure event when the client becomes active.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 2);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 50));
    QSignalSpy frameGeometryChangedSpy(c, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QSignalSpy startMoveResizedSpy(c, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(startMoveResizedSpy.isValid());
    QSignalSpy moveResizedChangedSpy(c, &AbstractClient::moveResizedChanged);
    QVERIFY(moveResizedChangedSpy.isValid());
    QSignalSpy clientStepUserMovedResizedSpy(c, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientStepUserMovedResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(c, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    // begin resize
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QCOMPARE(c->isInteractiveMove(), false);
    QCOMPARE(c->isInteractiveResize(), false);
    workspace()->slotWindowResize();
    QCOMPARE(workspace()->moveResizeClient(), c);
    QCOMPARE(startMoveResizedSpy.count(), 1);
    QCOMPARE(moveResizedChangedSpy.count(), 1);
    QCOMPARE(c->isInteractiveResize(), true);
    QCOMPARE(c->geometryRestore(), QRect());
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 3);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));

    // Trigger a change.
    const QPoint cursorPos = Cursors::self()->mouse()->pos();
    c->keyPressEvent(Qt::Key_Right);
    c->updateInteractiveMoveResize(Cursors::self()->mouse()->pos());
    QCOMPARE(Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 0));

    // The client should receive a configure event with the new size.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 4);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(108, 50));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    // Now render new size.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.data(), QSize(108, 50), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 108, 50));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    // Go down.
    c->keyPressEvent(Qt::Key_Down);
    c->updateInteractiveMoveResize(Cursors::self()->mouse()->pos());
    QCOMPARE(Cursors::self()->mouse()->pos(), cursorPos + QPoint(8, 8));

    // The client should receive another configure event.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 5);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 5);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Resizing));
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(108, 58));

    // Now render new size.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.data(), QSize(108, 58), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 108, 58));
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);

    // Let's finalize the resize operation.
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 0);
    c->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(moveResizedChangedSpy.count(), 2);
    QCOMPARE(c->isInteractiveResize(), false);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 6);
    QCOMPARE(toplevelConfigureRequestedSpy.count(), 6);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Resizing));

    // Destroy the client.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(c));
}

void MoveResizeWindowTest::testPackTo_data()
{
    QTest::addColumn<QString>("methodCall");
    QTest::addColumn<QRect>("expectedGeometry");

    QTest::newRow("left")  << QStringLiteral("slotWindowMoveLeft")  << QRect(0, 487, 100, 50);
    QTest::newRow("up")    << QStringLiteral("slotWindowMoveUp")    << QRect(590, 0, 100, 50);
    QTest::newRow("right") << QStringLiteral("slotWindowMoveRight") << QRect(1180, 487, 100, 50);
    QTest::newRow("down")  << QStringLiteral("slotWindowMoveDown")  << QRect(590, 974, 100, 50);
}

void MoveResizeWindowTest::testPackTo()
{
    using namespace KWayland::Client;

    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    // let's render
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 50));

    // let's place it centered
    Placement::self()->placeCentered(c, QRect(0, 0, 1280, 1024));
    QCOMPARE(c->frameGeometry(), QRect(590, 487, 100, 50));

    QFETCH(QString, methodCall);
    QMetaObject::invokeMethod(workspace(), methodCall.toLocal8Bit().constData());
    QTEST(c->frameGeometry(), "expectedGeometry");
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(c));
}

void MoveResizeWindowTest::testPackAgainstClient_data()
{
    QTest::addColumn<QString>("methodCall");
    QTest::addColumn<QRect>("expectedGeometry");

    QTest::newRow("left")  << QStringLiteral("slotWindowMoveLeft")  << QRect(10, 487, 100, 50);
    QTest::newRow("up")    << QStringLiteral("slotWindowMoveUp")    << QRect(590, 10, 100, 50);
    QTest::newRow("right") << QStringLiteral("slotWindowMoveRight") << QRect(1170, 487, 100, 50);
    QTest::newRow("down")  << QStringLiteral("slotWindowMoveDown")  << QRect(590, 964, 100, 50);
}

void MoveResizeWindowTest::testPackAgainstClient()
{
    using namespace KWayland::Client;

    QScopedPointer<KWayland::Client::Surface> surface1(Test::createSurface());
    QVERIFY(!surface1.isNull());
    QScopedPointer<KWayland::Client::Surface> surface2(Test::createSurface());
    QVERIFY(!surface2.isNull());
    QScopedPointer<KWayland::Client::Surface> surface3(Test::createSurface());
    QVERIFY(!surface3.isNull());
    QScopedPointer<KWayland::Client::Surface> surface4(Test::createSurface());
    QVERIFY(!surface4.isNull());

    QScopedPointer<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.data()));
    QVERIFY(!shellSurface1.isNull());
    QScopedPointer<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.data()));
    QVERIFY(!shellSurface2.isNull());
    QScopedPointer<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.data()));
    QVERIFY(!shellSurface3.isNull());
    QScopedPointer<Test::XdgToplevel> shellSurface4(Test::createXdgToplevelSurface(surface4.data()));
    QVERIFY(!shellSurface4.isNull());
    auto renderWindow = [] (KWayland::Client::Surface *surface, const QString &methodCall, const QRect &expectedGeometry) {
        // let's render
        auto c = Test::renderAndWaitForShown(surface, QSize(10, 10), Qt::blue);

        QVERIFY(c);
        QCOMPARE(workspace()->activeClient(), c);
        QCOMPARE(c->frameGeometry().size(), QSize(10, 10));
        // let's place it centered
        Placement::self()->placeCentered(c, QRect(0, 0, 1280, 1024));
        QCOMPARE(c->frameGeometry(), QRect(635, 507, 10, 10));
        QMetaObject::invokeMethod(workspace(), methodCall.toLocal8Bit().constData());
        QCOMPARE(c->frameGeometry(), expectedGeometry);
    };
    renderWindow(surface1.data(), QStringLiteral("slotWindowMoveLeft"),  QRect(0, 507, 10, 10));
    renderWindow(surface2.data(), QStringLiteral("slotWindowMoveUp"),    QRect(635, 0, 10, 10));
    renderWindow(surface3.data(), QStringLiteral("slotWindowMoveRight"), QRect(1270, 507, 10, 10));
    renderWindow(surface4.data(), QStringLiteral("slotWindowMoveDown"),  QRect(635, 1014, 10, 10));

    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);
    // let's place it centered
    Placement::self()->placeCentered(c, QRect(0, 0, 1280, 1024));
    QCOMPARE(c->frameGeometry(), QRect(590, 487, 100, 50));

    QFETCH(QString, methodCall);
    QMetaObject::invokeMethod(workspace(), methodCall.toLocal8Bit().constData());
    QTEST(c->frameGeometry(), "expectedGeometry");
}

void MoveResizeWindowTest::testGrowShrink_data()
{
    QTest::addColumn<QString>("methodCall");
    QTest::addColumn<QRect>("expectedGeometry");

    QTest::newRow("grow vertical")     << QStringLiteral("slotWindowExpandVertical")     << QRect(590, 487, 100, 537);
    QTest::newRow("grow horizontal")   << QStringLiteral("slotWindowExpandHorizontal")   << QRect(590, 487, 690, 50);
    QTest::newRow("shrink vertical")   << QStringLiteral("slotWindowShrinkVertical")   << QRect(590, 487, 100, 23);
    QTest::newRow("shrink horizontal") << QStringLiteral("slotWindowShrinkHorizontal") << QRect(590, 487, 40, 50);
}

void MoveResizeWindowTest::testGrowShrink()
{
    using namespace KWayland::Client;

    // block geometry helper
    QScopedPointer<KWayland::Client::Surface> surface1(Test::createSurface());
    QVERIFY(!surface1.isNull());
    QScopedPointer<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.data()));
    QVERIFY(!shellSurface1.isNull());
    Test::render(surface1.data(), QSize(650, 514), Qt::blue);
    QVERIFY(Test::waitForWaylandWindowShown());
    workspace()->slotWindowMoveRight();
    workspace()->slotWindowMoveDown();

    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested);
    QVERIFY(toplevelConfigureRequestedSpy.isValid());
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.isValid());
    // let's render
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(toplevelConfigureRequestedSpy.wait());

    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);

    // let's place it centered
    Placement::self()->placeCentered(c, QRect(0, 0, 1280, 1024));
    QCOMPARE(c->frameGeometry(), QRect(590, 487, 100, 50));

    QFETCH(QString, methodCall);
    QMetaObject::invokeMethod(workspace(), methodCall.toLocal8Bit().constData());
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    Test::render(surface.data(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::red);

    QSignalSpy frameGeometryChangedSpy(c, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    m_connection->flush();
    QVERIFY(frameGeometryChangedSpy.wait());
    QTEST(c->frameGeometry(), "expectedGeometry");
}

void MoveResizeWindowTest::testPointerMoveEnd_data()
{
    QTest::addColumn<int>("additionalButton");

    QTest::newRow("BTN_RIGHT")   << BTN_RIGHT;
    QTest::newRow("BTN_MIDDLE")  << BTN_MIDDLE;
    QTest::newRow("BTN_SIDE")    << BTN_SIDE;
    QTest::newRow("BTN_EXTRA")   << BTN_EXTRA;
    QTest::newRow("BTN_FORWARD") << BTN_FORWARD;
    QTest::newRow("BTN_BACK")    << BTN_BACK;
    QTest::newRow("BTN_TASK")    << BTN_TASK;
    for (int i=BTN_TASK + 1; i < BTN_JOYSTICK; i++) {
        QTest::newRow(QByteArray::number(i, 16).constData()) << i;
    }
}

void MoveResizeWindowTest::testPointerMoveEnd()
{
    // this test verifies that moving a window through pointer only ends if all buttons are released
    using namespace KWayland::Client;

    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    // let's render
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QCOMPARE(c, workspace()->activeClient());
    QVERIFY(!c->isInteractiveMove());

    // let's trigger the left button
    quint32 timestamp = 1;
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(!c->isInteractiveMove());
    workspace()->slotWindowMove();
    QVERIFY(c->isInteractiveMove());

    // let's press another button
    QFETCH(int, additionalButton);
    kwinApp()->platform()->pointerButtonPressed(additionalButton, timestamp++);
    QVERIFY(c->isInteractiveMove());

    // release the left button, should still have the window moving
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(c->isInteractiveMove());

    // but releasing the other button should now end moving
    kwinApp()->platform()->pointerButtonReleased(additionalButton, timestamp++);
    QVERIFY(!c->isInteractiveMove());
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(c));
}
void MoveResizeWindowTest::testClientSideMove()
{
    using namespace KWayland::Client;
    Cursors::self()->mouse()->setPos(640, 512);
    QScopedPointer<Pointer> pointer(Test::waylandSeat()->createPointer());
    QSignalSpy pointerEnteredSpy(pointer.data(), &Pointer::entered);
    QVERIFY(pointerEnteredSpy.isValid());
    QSignalSpy pointerLeftSpy(pointer.data(), &Pointer::left);
    QVERIFY(pointerLeftSpy.isValid());
    QSignalSpy buttonSpy(pointer.data(), &Pointer::buttonStateChanged);
    QVERIFY(buttonSpy.isValid());

    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);

    // move pointer into center of geometry
    const QRect startGeometry = c->frameGeometry();
    Cursors::self()->mouse()->setPos(startGeometry.center());
    QVERIFY(pointerEnteredSpy.wait());
    QCOMPARE(pointerEnteredSpy.first().last().toPoint(), QPoint(49, 24));
    // simulate press
    quint32 timestamp = 1;
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(buttonSpy.wait());
    QSignalSpy moveStartSpy(c, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(moveStartSpy.isValid());
    shellSurface->move(*Test::waylandSeat(), buttonSpy.first().first().value<quint32>());
    QVERIFY(moveStartSpy.wait());
    QCOMPARE(c->isInteractiveMove(), true);
    QVERIFY(pointerLeftSpy.wait());

    // move a bit
    QSignalSpy clientMoveStepSpy(c, &AbstractClient::clientStepUserMovedResized);
    QVERIFY(clientMoveStepSpy.isValid());
    const QPoint startPoint = startGeometry.center();
    const int dragDistance = QApplication::startDragDistance();
    // Why?
    kwinApp()->platform()->pointerMotion(startPoint + QPoint(dragDistance, dragDistance) + QPoint(6, 6), timestamp++);
    QCOMPARE(clientMoveStepSpy.count(), 1);

    // and release again
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(pointerEnteredSpy.wait());
    QCOMPARE(c->isInteractiveMove(), false);
    QCOMPARE(c->frameGeometry(), startGeometry.translated(QPoint(dragDistance, dragDistance) + QPoint(6, 6)));
    QCOMPARE(pointerEnteredSpy.last().last().toPoint(), QPoint(49, 24));
}

void MoveResizeWindowTest::testPlasmaShellSurfaceMovable_data()
{
    QTest::addColumn<KWayland::Client::PlasmaShellSurface::Role>("role");
    QTest::addColumn<bool>("movable");
    QTest::addColumn<bool>("movableAcrossScreens");
    QTest::addColumn<bool>("resizable");

    QTest::newRow("normal")  << KWayland::Client::PlasmaShellSurface::Role::Normal          << true  << true  << true;
    QTest::newRow("desktop") << KWayland::Client::PlasmaShellSurface::Role::Desktop         << false << false << false;
    QTest::newRow("panel")   << KWayland::Client::PlasmaShellSurface::Role::Panel           << false << false << false;
    QTest::newRow("osd")     << KWayland::Client::PlasmaShellSurface::Role::OnScreenDisplay << false << false << false;
}

void MoveResizeWindowTest::testPlasmaShellSurfaceMovable()
{
    // this test verifies that certain window types from PlasmaShellSurface are not moveable or resizable
    using namespace KWayland::Client;
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    // and a PlasmaShellSurface
    QScopedPointer<PlasmaShellSurface> plasmaSurface(Test::waylandPlasmaShell()->createSurface(surface.data()));
    QVERIFY(!plasmaSurface.isNull());
    QFETCH(KWayland::Client::PlasmaShellSurface::Role, role);
    plasmaSurface->setRole(role);
    // let's render
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QTEST(c->isMovable(), "movable");
    QTEST(c->isMovableAcrossScreens(), "movableAcrossScreens");
    QTEST(c->isResizable(), "resizable");
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(c));
}

struct XcbConnectionDeleter
{
    static inline void cleanup(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

void MoveResizeWindowTest::testNetMove()
{
    // this test verifies that a move request for an X11 window through NET API works
    // create an xcb window
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));

    xcb_window_t w = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, w, rootWindow(),
                      0, 0, 100, 100,
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, 0, 0);
    xcb_icccm_size_hints_set_size(&hints, 1, 100, 100);
    xcb_icccm_set_wm_normal_hints(c.data(), w, &hints);
    // let's set a no-border
    NETWinInfo winInfo(c.data(), w, rootWindow(), NET::WMWindowType, NET::Properties2());
    winInfo.setWindowType(NET::Override);
    xcb_map_window(c.data(), w);
    xcb_flush(c.data());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    X11Client *client = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(client);
    QCOMPARE(client->window(), w);
    const QRect origGeo = client->frameGeometry();

    // let's move the cursor outside the window
    Cursors::self()->mouse()->setPos(screens()->geometry(0).center());
    QVERIFY(!origGeo.contains(Cursors::self()->mouse()->pos()));

    QSignalSpy moveStartSpy(client, &X11Client::clientStartUserMovedResized);
    QVERIFY(moveStartSpy.isValid());
    QSignalSpy moveEndSpy(client, &X11Client::clientFinishUserMovedResized);
    QVERIFY(moveEndSpy.isValid());
    QSignalSpy moveStepSpy(client, &X11Client::clientStepUserMovedResized);
    QVERIFY(moveStepSpy.isValid());
    QVERIFY(!workspace()->moveResizeClient());

    // use NETRootInfo to trigger a move request
    NETRootInfo root(c.data(), NET::Properties());
    root.moveResizeRequest(w, origGeo.center().x(), origGeo.center().y(), NET::Move);
    xcb_flush(c.data());

    QVERIFY(moveStartSpy.wait());
    QCOMPARE(workspace()->moveResizeClient(), client);
    QVERIFY(client->isInteractiveMove());
    QCOMPARE(client->geometryRestore(), origGeo);
    QCOMPARE(Cursors::self()->mouse()->pos(), origGeo.center());

    // let's move a step
    Cursors::self()->mouse()->setPos(Cursors::self()->mouse()->pos() + QPoint(10, 10));
    QCOMPARE(moveStepSpy.count(), 1);
    QCOMPARE(moveStepSpy.first().last().toRect(), origGeo.translated(10, 10));

    // let's cancel the move resize again through the net API
    root.moveResizeRequest(w, client->frameGeometry().center().x(), client->frameGeometry().center().y(), NET::MoveResizeCancel);
    xcb_flush(c.data());
    QVERIFY(moveEndSpy.wait());

    // and destroy the window again
    xcb_unmap_window(c.data(), w);
    xcb_destroy_window(c.data(), w);
    xcb_flush(c.data());
    c.reset();

    QSignalSpy windowClosedSpy(client, &X11Client::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    QVERIFY(windowClosedSpy.wait());
}

void MoveResizeWindowTest::testAdjustClientGeometryOfAutohidingX11Panel_data()
{
    QTest::addColumn<QRect>("panelGeometry");
    QTest::addColumn<QPoint>("targetPoint");
    QTest::addColumn<QPoint>("expectedAdjustedPoint");
    QTest::addColumn<quint32>("hideLocation");

    QTest::newRow("top") << QRect(0, 0, 100, 20) << QPoint(50, 25) << QPoint(50, 20) << 0u;
    QTest::newRow("bottom") << QRect(0, 1024-20, 100, 20) << QPoint(50, 1024 - 25 - 50) << QPoint(50, 1024 - 20 - 50) << 2u;
    QTest::newRow("left") << QRect(0, 0, 20, 100) << QPoint(25, 50) << QPoint(20, 50) << 3u;
    QTest::newRow("right") << QRect(1280 - 20, 0, 20, 100) << QPoint(1280 - 25 - 100, 50) << QPoint(1280 - 20 - 100, 50) << 1u;
}

void MoveResizeWindowTest::testAdjustClientGeometryOfAutohidingX11Panel()
{
    // this test verifies that auto hiding panels are ignored when adjusting client geometry
    // see BUG 365892

    // first create our panel
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));

    xcb_window_t w = xcb_generate_id(c.data());
    QFETCH(QRect, panelGeometry);
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, w, rootWindow(),
                      panelGeometry.x(), panelGeometry.y(), panelGeometry.width(), panelGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, panelGeometry.x(), panelGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, panelGeometry.width(), panelGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), w, &hints);
    NETWinInfo winInfo(c.data(), w, rootWindow(), NET::WMWindowType, NET::Properties2());
    winInfo.setWindowType(NET::Dock);
    xcb_map_window(c.data(), w);
    xcb_flush(c.data());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    X11Client *panel = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(panel);
    QCOMPARE(panel->window(), w);
    QCOMPARE(panel->frameGeometry(), panelGeometry);
    QVERIFY(panel->isDock());

    // let's create a window
    using namespace KWayland::Client;
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    auto testWindow = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(testWindow);
    QVERIFY(testWindow->isMovable());
    // panel is not yet hidden, we should snap against it
    QFETCH(QPoint, targetPoint);
    QTEST(Workspace::self()->adjustClientPosition(testWindow, targetPoint, false), "expectedAdjustedPoint");

    // now let's hide the panel
    QSignalSpy panelHiddenSpy(panel, &AbstractClient::windowHidden);
    QVERIFY(panelHiddenSpy.isValid());
    QFETCH(quint32, hideLocation);
    xcb_change_property(c.data(), XCB_PROP_MODE_REPLACE, w, atoms->kde_screen_edge_show, XCB_ATOM_CARDINAL, 32, 1, &hideLocation);
    xcb_flush(c.data());
    QVERIFY(panelHiddenSpy.wait());

    // now try to snap again
    QCOMPARE(Workspace::self()->adjustClientPosition(testWindow, targetPoint, false), targetPoint);

    // and destroy the panel again
    xcb_unmap_window(c.data(), w);
    xcb_destroy_window(c.data(), w);
    xcb_flush(c.data());
    c.reset();

    QSignalSpy panelClosedSpy(panel, &X11Client::windowClosed);
    QVERIFY(panelClosedSpy.isValid());
    QVERIFY(panelClosedSpy.wait());

    // snap once more
    QCOMPARE(Workspace::self()->adjustClientPosition(testWindow, targetPoint, false), targetPoint);

    // and close
    QSignalSpy windowClosedSpy(testWindow, &AbstractClient::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    shellSurface.reset();
    surface.reset();
    QVERIFY(windowClosedSpy.wait());
}

void MoveResizeWindowTest::testAdjustClientGeometryOfAutohidingWaylandPanel_data()
{
    QTest::addColumn<QRect>("panelGeometry");
    QTest::addColumn<QPoint>("targetPoint");
    QTest::addColumn<QPoint>("expectedAdjustedPoint");

    QTest::newRow("top") << QRect(0, 0, 100, 20) << QPoint(50, 25) << QPoint(50, 20);
    QTest::newRow("bottom") << QRect(0, 1024-20, 100, 20) << QPoint(50, 1024 - 25 - 50) << QPoint(50, 1024 - 20 - 50);
    QTest::newRow("left") << QRect(0, 0, 20, 100) << QPoint(25, 50) << QPoint(20, 50);
    QTest::newRow("right") << QRect(1280 - 20, 0, 20, 100) << QPoint(1280 - 25 - 100, 50) << QPoint(1280 - 20 - 100, 50);
}

void MoveResizeWindowTest::testAdjustClientGeometryOfAutohidingWaylandPanel()
{
    // this test verifies that auto hiding panels are ignored when adjusting client geometry
    // see BUG 365892

    // first create our panel
    using namespace KWayland::Client;
    QScopedPointer<KWayland::Client::Surface> panelSurface(Test::createSurface());
    QVERIFY(!panelSurface.isNull());
    QScopedPointer<Test::XdgToplevel> panelShellSurface(Test::createXdgToplevelSurface(panelSurface.data()));
    QVERIFY(!panelShellSurface.isNull());
    QScopedPointer<PlasmaShellSurface> plasmaSurface(Test::waylandPlasmaShell()->createSurface(panelSurface.data()));
    QVERIFY(!plasmaSurface.isNull());
    plasmaSurface->setRole(PlasmaShellSurface::Role::Panel);
    plasmaSurface->setPanelBehavior(PlasmaShellSurface::PanelBehavior::AutoHide);
    QFETCH(QRect, panelGeometry);
    plasmaSurface->setPosition(panelGeometry.topLeft());
    // let's render
    auto panel = Test::renderAndWaitForShown(panelSurface.data(), panelGeometry.size(), Qt::blue);
    QVERIFY(panel);
    QCOMPARE(panel->frameGeometry(), panelGeometry);
    QVERIFY(panel->isDock());

    // let's create a window
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    auto testWindow = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(testWindow);
    QVERIFY(testWindow->isMovable());
    // panel is not yet hidden, we should snap against it
    QFETCH(QPoint, targetPoint);
    QTEST(Workspace::self()->adjustClientPosition(testWindow, targetPoint, false), "expectedAdjustedPoint");

    // now let's hide the panel
    QSignalSpy panelHiddenSpy(panel, &AbstractClient::windowHidden);
    QVERIFY(panelHiddenSpy.isValid());
    plasmaSurface->requestHideAutoHidingPanel();
    QVERIFY(panelHiddenSpy.wait());

    // now try to snap again
    QCOMPARE(Workspace::self()->adjustClientPosition(testWindow, targetPoint, false), targetPoint);

    // and destroy the panel again
    QSignalSpy panelClosedSpy(panel, &AbstractClient::windowClosed);
    QVERIFY(panelClosedSpy.isValid());
    plasmaSurface.reset();
    panelShellSurface.reset();
    panelSurface.reset();
    QVERIFY(panelClosedSpy.wait());

    // snap once more
    QCOMPARE(Workspace::self()->adjustClientPosition(testWindow, targetPoint, false), targetPoint);

    // and close
    QSignalSpy windowClosedSpy(testWindow, &AbstractClient::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    shellSurface.reset();
    surface.reset();
    QVERIFY(windowClosedSpy.wait());
}

void MoveResizeWindowTest::testResizeForVirtualKeyboard()
{
    using namespace KWayland::Client;

    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // let's render
    auto client = Test::renderAndWaitForShown(surface.data(), QSize(500, 800), Qt::blue);
    QVERIFY(client);

    // The client should receive a configure event upon becoming active.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested);
    QVERIFY(toplevelConfigureRequestedSpy.isValid());
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.isValid());
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    client->move(QPoint(100, 300));
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());

    QCOMPARE(client->frameGeometry(), QRect(100, 300, 500, 800));
    client->setVirtualKeyboardGeometry(QRect(0, 100, 1280, 500));
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last()[0].toInt());
    // render at the new size
    Test::render(surface.data(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());

    QCOMPARE(client->frameGeometry(), QRect(100, 0, 500, 101));
    client->setVirtualKeyboardGeometry(QRect());
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last()[0].toInt());
    // render at the new size
    Test::render(surface.data(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->frameGeometry(), QRect(100, 300, 500, 800));
}

void MoveResizeWindowTest::testResizeForVirtualKeyboardWithMaximize()
{
    using namespace KWayland::Client;

    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // let's render
    auto client = Test::renderAndWaitForShown(surface.data(), QSize(500, 800), Qt::blue);
    QVERIFY(client);

    // The client should receive a configure event upon becoming active.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested);
    QVERIFY(toplevelConfigureRequestedSpy.isValid());
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.isValid());
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    client->move(QPoint(100, 300));
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());

    QCOMPARE(client->frameGeometry(), QRect(100, 300, 500, 800));
    client->setVirtualKeyboardGeometry(QRect(0, 100, 1280, 500));
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last()[0].toInt());
    // render at the new size
    Test::render(surface.data(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->frameGeometry(), QRect(100, 0, 500, 101));

    client->setMaximize(true, true);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last()[0].toInt());
    Test::render(surface.data(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->frameGeometry(), QRect(0, 0, 1280, 1024));

    client->setVirtualKeyboardGeometry(QRect());
    QVERIFY(!surfaceConfigureRequestedSpy.wait(10));

    // render at the size of the configureRequested.. it won't have changed
    Test::render(surface.data(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);
    QVERIFY(!frameGeometryChangedSpy.wait(10));

    // Size will NOT be restored
    QCOMPARE(client->frameGeometry(), QRect(0, 0, 1280, 1024));
}

void MoveResizeWindowTest::testResizeForVirtualKeyboardWithFullScreen()
{
    using namespace KWayland::Client;

    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // let's render
    auto client = Test::renderAndWaitForShown(surface.data(), QSize(500, 800), Qt::blue);
    QVERIFY(client);

    // The client should receive a configure event upon becoming active.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested);
    QVERIFY(toplevelConfigureRequestedSpy.isValid());
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.isValid());
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    client->move(QPoint(100, 300));
    QSignalSpy frameGeometryChangedSpy(client, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());

    QCOMPARE(client->frameGeometry(), QRect(100, 300, 500, 800));
    client->setVirtualKeyboardGeometry(QRect(0, 100, 1280, 500));
    QVERIFY(surfaceConfigureRequestedSpy.wait());

    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last()[0].toInt());
    // render at the new size
    Test::render(surface.data(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->frameGeometry(), QRect(100, 0, 500, 101));

    client->setFullScreen(true, true);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last()[0].toInt());
    Test::render(surface.data(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(client->frameGeometry(), QRect(0, 0, 1280, 1024));

    client->setVirtualKeyboardGeometry(QRect());
    QVERIFY(!surfaceConfigureRequestedSpy.wait(10));

    // render at the size of the configureRequested.. it won't have changed
    Test::render(surface.data(), toplevelConfigureRequestedSpy.last().first().toSize(), Qt::blue);
    QVERIFY(!frameGeometryChangedSpy.wait(10));
    // Size will NOT be restored
    QCOMPARE(client->frameGeometry(), QRect(0, 0, 1280, 1024));
}

void MoveResizeWindowTest::testDestroyMoveClient()
{
    // This test verifies that active move operation gets finished when
    // the associated client is destroyed.

    // Create the test client.
    using namespace KWayland::Client;
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);

    // Start moving the client.
    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(client, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QCOMPARE(client->isInteractiveMove(), false);
    QCOMPARE(client->isInteractiveResize(), false);
    workspace()->slotWindowMove();
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), client);
    QCOMPARE(client->isInteractiveMove(), true);
    QCOMPARE(client->isInteractiveResize(), false);

    // Let's pretend that the client crashed.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
}

void MoveResizeWindowTest::testDestroyResizeClient()
{
    // This test verifies that active resize operation gets finished when
    // the associated client is destroyed.

    // Create the test client.
    using namespace KWayland::Client;
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);

    // Start resizing the client.
    QSignalSpy clientStartMoveResizedSpy(client, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(clientStartMoveResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(client, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    QCOMPARE(workspace()->moveResizeClient(), nullptr);
    QCOMPARE(client->isInteractiveMove(), false);
    QCOMPARE(client->isInteractiveResize(), false);
    workspace()->slotWindowResize();
    QCOMPARE(clientStartMoveResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), client);
    QCOMPARE(client->isInteractiveMove(), false);
    QCOMPARE(client->isInteractiveResize(), true);

    // Let's pretend that the client crashed.
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(client));
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(workspace()->moveResizeClient(), nullptr);
}

}

WAYLANDTEST_MAIN(KWin::MoveResizeWindowTest)
#include "move_resize_window_test.moc"
