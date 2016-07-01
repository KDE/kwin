
/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "kwin_wayland_test.h"
#include "platform.h"
#include "abstract_client.h"
#include "cursor.h"
#include "effects.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "shell_client.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/surface.h>

#include <linux/input.h>
#include <xcb/xcb_icccm.h>

Q_DECLARE_METATYPE(KWin::AbstractClient::QuickTileMode)
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
    void testPackTo_data();
    void testPackTo();
    void testPackAgainstClient_data();
    void testPackAgainstClient();
    void testGrowShrink_data();
    void testGrowShrink();
    void testPointerMoveEnd_data();
    void testPointerMoveEnd();
    void testPlasmaShellSurfaceMovable_data();
    void testPlasmaShellSurfaceMovable();
    void testNetMove();

private:
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Shell *m_shell = nullptr;
};

void MoveResizeWindowTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<KWin::MaximizeMode>("MaximizeMode");
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    waylandServer()->init(s_socketName.toLocal8Bit());
    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 1);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
}

void MoveResizeWindowTest::init()
{
    QVERIFY(Test::setupWaylandConnection(s_socketName, Test::AdditionalWaylandInterface::PlasmaShell));
    m_connection = Test::waylandConnection();
    m_compositor = Test::waylandCompositor();
    m_shell = Test::waylandShell();

    screens()->setCurrent(0);
}

void MoveResizeWindowTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void MoveResizeWindowTest::testMove()
{
    using namespace KWayland::Client;

    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QSignalSpy sizeChangeSpy(shellSurface.data(), &ShellSurface::sizeChanged);
    QVERIFY(sizeChangeSpy.isValid());
    // let's render
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);
    QCOMPARE(c->geometry(), QRect(0, 0, 100, 50));
    QSignalSpy geometryChangedSpy(c, &AbstractClient::geometryChanged);
    QVERIFY(geometryChangedSpy.isValid());
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
    QVERIFY(workspace()->getMovingClient() == nullptr);
    QCOMPARE(c->isMove(), false);
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->getMovingClient(), c);
    QCOMPARE(startMoveResizedSpy.count(), 1);
    QCOMPARE(moveResizedChangedSpy.count(), 1);
    QCOMPARE(windowStartUserMovedResizedSpy.count(), 1);
    QCOMPARE(c->isMove(), true);
    QCOMPARE(c->geometryRestore(), QRect(0, 0, 100, 50));

    // send some key events, not going through input redirection
    const QPoint cursorPos = Cursor::pos();
    c->keyPressEvent(Qt::Key_Right);
    c->updateMoveResize(Cursor::pos());
    QCOMPARE(Cursor::pos(), cursorPos + QPoint(8, 0));
    QEXPECT_FAIL("", "First event is ignored", Continue);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 1);

    c->keyPressEvent(Qt::Key_Right);
    c->updateMoveResize(Cursor::pos());
    QCOMPARE(Cursor::pos(), cursorPos + QPoint(16, 0));
    QEXPECT_FAIL("", "First event is ignored", Continue);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 2);
    QEXPECT_FAIL("", "First event is ignored", Continue);
    QCOMPARE(windowStepUserMovedResizedSpy.count(), 2);

    c->keyPressEvent(Qt::Key_Down | Qt::ALT);
    c->updateMoveResize(Cursor::pos());
    QEXPECT_FAIL("", "First event is ignored", Continue);
    QCOMPARE(clientStepUserMovedResizedSpy.count(), 3);
    QCOMPARE(c->geometry(), QRect(16, 32, 100, 50));
    QCOMPARE(Cursor::pos(), cursorPos + QPoint(16, 32));

    // let's end
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 0);
    c->keyPressEvent(Qt::Key_Enter);
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(moveResizedChangedSpy.count(), 2);
    QCOMPARE(windowFinishUserMovedResizedSpy.count(), 1);
    QCOMPARE(c->geometry(), QRect(16, 32, 100, 50));
    QCOMPARE(c->isMove(), false);
    QVERIFY(workspace()->getMovingClient() == nullptr);
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(c));
}

void MoveResizeWindowTest::testPackTo_data()
{
    QTest::addColumn<QString>("methodCall");
    QTest::addColumn<QRect>("expectedGeometry");

    QTest::newRow("left")  << QStringLiteral("slotWindowPackLeft")  << QRect(0, 487, 100, 50);
    QTest::newRow("up")    << QStringLiteral("slotWindowPackUp")    << QRect(590, 0, 100, 50);
    QTest::newRow("right") << QStringLiteral("slotWindowPackRight") << QRect(1180, 487, 100, 50);
    QTest::newRow("down")  << QStringLiteral("slotWindowPackDown")  << QRect(590, 974, 100, 50);
}

void MoveResizeWindowTest::testPackTo()
{
    using namespace KWayland::Client;

    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QSignalSpy sizeChangeSpy(shellSurface.data(), &ShellSurface::sizeChanged);
    QVERIFY(sizeChangeSpy.isValid());
    // let's render
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);
    QCOMPARE(c->geometry(), QRect(0, 0, 100, 50));

    // let's place it centered
    Placement::self()->placeCentered(c, QRect(0, 0, 1280, 1024));
    QCOMPARE(c->geometry(), QRect(590, 487, 100, 50));

    QFETCH(QString, methodCall);
    QMetaObject::invokeMethod(workspace(), methodCall.toLocal8Bit().constData());
    QTEST(c->geometry(), "expectedGeometry");
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(c));
}

void MoveResizeWindowTest::testPackAgainstClient_data()
{
    QTest::addColumn<QString>("methodCall");
    QTest::addColumn<QRect>("expectedGeometry");

    QTest::newRow("left")  << QStringLiteral("slotWindowPackLeft")  << QRect(10, 487, 100, 50);
    QTest::newRow("up")    << QStringLiteral("slotWindowPackUp")    << QRect(590, 10, 100, 50);
    QTest::newRow("right") << QStringLiteral("slotWindowPackRight") << QRect(1170, 487, 100, 50);
    QTest::newRow("down")  << QStringLiteral("slotWindowPackDown")  << QRect(590, 964, 100, 50);
}

void MoveResizeWindowTest::testPackAgainstClient()
{
    using namespace KWayland::Client;

    QScopedPointer<Surface> surface1(Test::createSurface());
    QVERIFY(!surface1.isNull());
    QScopedPointer<Surface> surface2(Test::createSurface());
    QVERIFY(!surface2.isNull());
    QScopedPointer<Surface> surface3(Test::createSurface());
    QVERIFY(!surface3.isNull());
    QScopedPointer<Surface> surface4(Test::createSurface());
    QVERIFY(!surface4.isNull());

    QScopedPointer<ShellSurface> shellSurface1(Test::createShellSurface(surface1.data()));
    QVERIFY(!shellSurface1.isNull());
    QScopedPointer<ShellSurface> shellSurface2(Test::createShellSurface(surface2.data()));
    QVERIFY(!shellSurface2.isNull());
    QScopedPointer<ShellSurface> shellSurface3(Test::createShellSurface(surface3.data()));
    QVERIFY(!shellSurface3.isNull());
    QScopedPointer<ShellSurface> shellSurface4(Test::createShellSurface(surface4.data()));
    QVERIFY(!shellSurface4.isNull());
    auto renderWindow = [this] (Surface *surface, const QString &methodCall, const QRect &expectedGeometry) {
        // let's render
        auto c = Test::renderAndWaitForShown(surface, QSize(10, 10), Qt::blue);

        QVERIFY(c);
        QCOMPARE(workspace()->activeClient(), c);
        QCOMPARE(c->geometry().size(), QSize(10, 10));
        // let's place it centered
        Placement::self()->placeCentered(c, QRect(0, 0, 1280, 1024));
        QCOMPARE(c->geometry(), QRect(635, 507, 10, 10));
        QMetaObject::invokeMethod(workspace(), methodCall.toLocal8Bit().constData());
        QCOMPARE(c->geometry(), expectedGeometry);
    };
    renderWindow(surface1.data(), QStringLiteral("slotWindowPackLeft"),  QRect(0, 507, 10, 10));
    renderWindow(surface2.data(), QStringLiteral("slotWindowPackUp"),    QRect(635, 0, 10, 10));
    renderWindow(surface3.data(), QStringLiteral("slotWindowPackRight"), QRect(1270, 507, 10, 10));
    renderWindow(surface4.data(), QStringLiteral("slotWindowPackDown"),  QRect(635, 1014, 10, 10));

    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);
    // let's place it centered
    Placement::self()->placeCentered(c, QRect(0, 0, 1280, 1024));
    QCOMPARE(c->geometry(), QRect(590, 487, 100, 50));

    QFETCH(QString, methodCall);
    QMetaObject::invokeMethod(workspace(), methodCall.toLocal8Bit().constData());
    QTEST(c->geometry(), "expectedGeometry");
}

void MoveResizeWindowTest::testGrowShrink_data()
{
    QTest::addColumn<QString>("methodCall");
    QTest::addColumn<QRect>("expectedGeometry");

    QTest::newRow("grow vertical")     << QStringLiteral("slotWindowGrowVertical")     << QRect(590, 487, 100, 537);
    QTest::newRow("grow horizontal")   << QStringLiteral("slotWindowGrowHorizontal")   << QRect(590, 487, 690, 50);
    QTest::newRow("shrink vertical")   << QStringLiteral("slotWindowShrinkVertical")   << QRect(590, 487, 100, 23);
    QTest::newRow("shrink horizontal") << QStringLiteral("slotWindowShrinkHorizontal") << QRect(590, 487, 40, 50);
}

void MoveResizeWindowTest::testGrowShrink()
{
    using namespace KWayland::Client;

    // block geometry helper
    QScopedPointer<Surface> surface1(Test::createSurface());
    QVERIFY(!surface1.isNull());
    QScopedPointer<ShellSurface> shellSurface1(Test::createShellSurface(surface1.data()));
    QVERIFY(!shellSurface1.isNull());
    Test::render(surface1.data(), QSize(650, 514), Qt::blue);
    QVERIFY(Test::waitForWaylandWindowShown());
    workspace()->slotWindowPackRight();
    workspace()->slotWindowPackDown();

    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QSignalSpy sizeChangeSpy(shellSurface.data(), &ShellSurface::sizeChanged);
    QVERIFY(sizeChangeSpy.isValid());
    // let's render
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);

    // let's place it centered
    Placement::self()->placeCentered(c, QRect(0, 0, 1280, 1024));
    QCOMPARE(c->geometry(), QRect(590, 487, 100, 50));

    QFETCH(QString, methodCall);
    QMetaObject::invokeMethod(workspace(), methodCall.toLocal8Bit().constData());
    QVERIFY(sizeChangeSpy.wait());
    Test::render(surface.data(), shellSurface->size(), Qt::red);

    QSignalSpy geometryChangedSpy(c, &AbstractClient::geometryChanged);
    QVERIFY(geometryChangedSpy.isValid());
    m_connection->flush();
    QVERIFY(geometryChangedSpy.wait());
    QTEST(c->geometry(), "expectedGeometry");
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

    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QSignalSpy sizeChangeSpy(shellSurface.data(), &ShellSurface::sizeChanged);
    QVERIFY(sizeChangeSpy.isValid());
    // let's render
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QCOMPARE(c, workspace()->activeClient());
    QVERIFY(!c->isMove());

    // let's trigger the left button
    quint32 timestamp = 1;
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(!c->isMove());
    workspace()->slotWindowMove();
    QVERIFY(c->isMove());

    // let's press another button
    QFETCH(int, additionalButton);
    kwinApp()->platform()->pointerButtonPressed(additionalButton, timestamp++);
    QVERIFY(c->isMove());

    // release the left button, should still have the window moving
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(c->isMove());

    // but releasing the other button should now end moving
    kwinApp()->platform()->pointerButtonReleased(additionalButton, timestamp++);
    QVERIFY(!c->isMove());
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(c));
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
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
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

void MoveResizeWindowTest::testNetMove()
{
    // this test verifies that a move request for an X11 window through NET API works
    // create an xcb window
    struct XcbConnectionDeleter
    {
        static inline void cleanup(xcb_connection_t *pointer)
        {
            xcb_disconnect(pointer);
        }
    };
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
    Client *client = windowCreatedSpy.first().first().value<Client*>();
    QVERIFY(client);
    QCOMPARE(client->window(), w);
    const QRect origGeo = client->geometry();

    // let's move the cursor outside the window
    Cursor::setPos(screens()->geometry(0).center());
    QVERIFY(!origGeo.contains(Cursor::pos()));

    QSignalSpy moveStartSpy(client, &Client::clientStartUserMovedResized);
    QVERIFY(moveStartSpy.isValid());
    QSignalSpy moveEndSpy(client, &Client::clientFinishUserMovedResized);
    QVERIFY(moveEndSpy.isValid());
    QSignalSpy moveStepSpy(client, &Client::clientStepUserMovedResized);
    QVERIFY(moveStepSpy.isValid());
    QVERIFY(!workspace()->getMovingClient());

    // use NETRootInfo to trigger a move request
    NETRootInfo root(c.data(), NET::Properties());
    root.moveResizeRequest(w, origGeo.center().x(), origGeo.center().y(), NET::Move);
    xcb_flush(c.data());

    QVERIFY(moveStartSpy.wait());
    QCOMPARE(workspace()->getMovingClient(), client);
    QVERIFY(client->isMove());
    QCOMPARE(client->geometryRestore(), origGeo);
    QCOMPARE(Cursor::pos(), origGeo.center());

    // let's move a step
    Cursor::setPos(Cursor::pos() + QPoint(10, 10));
    QCOMPARE(moveStepSpy.count(), 1);
    QCOMPARE(moveStepSpy.first().last().toRect(), origGeo.translated(10, 10));

    // let's cancel the move resize again through the net API
    root.moveResizeRequest(w, client->geometry().center().x(), client->geometry().center().y(), NET::MoveResizeCancel);
    xcb_flush(c.data());
    QVERIFY(moveEndSpy.wait());

    // and destroy the window again
    xcb_unmap_window(c.data(), w);
    xcb_destroy_window(c.data(), w);
    xcb_flush(c.data());
    c.reset();

    QSignalSpy windowClosedSpy(client, &Client::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    QVERIFY(windowClosedSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::MoveResizeWindowTest)
#include "move_resize_window_test.moc"
