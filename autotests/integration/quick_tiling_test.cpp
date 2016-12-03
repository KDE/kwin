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
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "shell_client.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/surface.h>

#include <linux/input.h>

Q_DECLARE_METATYPE(KWin::AbstractClient::QuickTileMode)
Q_DECLARE_METATYPE(KWin::MaximizeMode)

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_quick_tiling-0");

class QuickTilingTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testQuickTiling_data();
    void testQuickTiling();
    void testQuickMaximizing_data();
    void testQuickMaximizing();
    void testQuickTilingKeyboardMove_data();
    void testQuickTilingKeyboardMove();
    void testQuickTilingPointerMove_data();
    void testQuickTilingPointerMove();

private:
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Shell *m_shell = nullptr;
};

void QuickTilingTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<KWin::MaximizeMode>("MaximizeMode");
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    // set custom config which disables the Outline
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup group = config->group("Outline");
    group.writeEntry(QStringLiteral("QmlPath"), QString("/does/not/exist.qml"));
    group.sync();

    kwinApp()->setConfig(config);

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
}

void QuickTilingTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
    m_connection = Test::waylandConnection();
    m_compositor = Test::waylandCompositor();
    m_shell = Test::waylandShell();

    screens()->setCurrent(0);
}

void QuickTilingTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void QuickTilingTest::testQuickTiling_data()
{
    QTest::addColumn<AbstractClient::QuickTileMode>("mode");
    QTest::addColumn<QRect>("expectedGeometry");
    QTest::addColumn<QRect>("secondScreen");

#define FLAG(name) AbstractClient::QuickTileMode(AbstractClient::QuickTile##name)

    QTest::newRow("left")   << FLAG(Left)   << QRect(0, 0, 640, 1024)   << QRect(1280, 0, 640, 1024);
    QTest::newRow("top")    << FLAG(Top)    << QRect(0, 0, 1280, 512)   << QRect(1280, 0, 1280, 512);
    QTest::newRow("right")  << FLAG(Right)  << QRect(640, 0, 640, 1024) << QRect(1920, 0, 640, 1024);
    QTest::newRow("bottom") << FLAG(Bottom) << QRect(0, 512, 1280, 512) << QRect(1280, 512, 1280, 512);

    QTest::newRow("top left")     << (FLAG(Left)  | FLAG(Top))    << QRect(0, 0, 640, 512)     << QRect(1280, 0, 640, 512);
    QTest::newRow("top right")    << (FLAG(Right) | FLAG(Top))    << QRect(640, 0, 640, 512)   << QRect(1920, 0, 640, 512);
    QTest::newRow("bottom left")  << (FLAG(Left)  | FLAG(Bottom)) << QRect(0, 512, 640, 512)   << QRect(1280, 512, 640, 512);
    QTest::newRow("bottom right") << (FLAG(Right) | FLAG(Bottom)) << QRect(640, 512, 640, 512) << QRect(1920, 512, 640, 512);

    QTest::newRow("maximize") << FLAG(Maximize) << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024);

#undef FLAG
}

void QuickTilingTest::testQuickTiling()
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
    QCOMPARE(c->quickTileMode(), AbstractClient::QuickTileNone);
    QSignalSpy quickTileChangedSpy(c, &AbstractClient::quickTileModeChanged);
    QVERIFY(quickTileChangedSpy.isValid());
    QSignalSpy geometryChangedSpy(c, &AbstractClient::geometryChanged);
    QVERIFY(geometryChangedSpy.isValid());

    QFETCH(AbstractClient::QuickTileMode, mode);
    QFETCH(QRect, expectedGeometry);
    c->setQuickTileMode(mode, true);
    QCOMPARE(quickTileChangedSpy.count(), 1);
    // at this point the geometry did not yet change
    QCOMPARE(c->geometry(), QRect(0, 0, 100, 50));
    // but quick tile mode already changed
    QCOMPARE(c->quickTileMode(), mode);

    // but we got requested a new geometry
    QVERIFY(sizeChangeSpy.wait());
    QCOMPARE(sizeChangeSpy.count(), 1);
    QCOMPARE(sizeChangeSpy.first().first().toSize(), expectedGeometry.size());

    // attach a new image
    Test::render(surface.data(), expectedGeometry.size(), Qt::red);
    m_connection->flush();

    QVERIFY(geometryChangedSpy.wait());
    QEXPECT_FAIL("maximize", "Geometry changed called twice for maximize", Continue);
    QCOMPARE(geometryChangedSpy.count(), 1);
    QCOMPARE(c->geometry(), expectedGeometry);

    // send window to other screen
    QCOMPARE(c->screen(), 0);
    c->sendToScreen(1);
    QCOMPARE(c->screen(), 1);
    // quick tile should not be changed
    QCOMPARE(c->quickTileMode(), mode);
    QTEST(c->geometry(), "secondScreen");
}

void QuickTilingTest::testQuickMaximizing_data()
{
    QTest::addColumn<AbstractClient::QuickTileMode>("mode");

#define FLAG(name) AbstractClient::QuickTileMode(AbstractClient::QuickTile##name)

    QTest::newRow("maximize") << FLAG(Maximize);
    QTest::newRow("none") << FLAG(None);

#undef FLAG
}

void QuickTilingTest::testQuickMaximizing()
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
    QCOMPARE(c->quickTileMode(), AbstractClient::QuickTileNone);
    QCOMPARE(c->maximizeMode(), MaximizeRestore);
    QSignalSpy quickTileChangedSpy(c, &AbstractClient::quickTileModeChanged);
    QVERIFY(quickTileChangedSpy.isValid());
    QSignalSpy geometryChangedSpy(c, &AbstractClient::geometryChanged);
    QVERIFY(geometryChangedSpy.isValid());
    QSignalSpy maximizeChangedSpy1(c, SIGNAL(clientMaximizedStateChanged(KWin::AbstractClient*,MaximizeMode)));
    QVERIFY(maximizeChangedSpy1.isValid());
    QSignalSpy maximizeChangedSpy2(c, SIGNAL(clientMaximizedStateChanged(KWin::AbstractClient*,bool,bool)));
    QVERIFY(maximizeChangedSpy2.isValid());

    c->setQuickTileMode(AbstractClient::QuickTileMaximize, true);
    QCOMPARE(quickTileChangedSpy.count(), 1);
    QCOMPARE(maximizeChangedSpy1.count(), 1);
    QCOMPARE(maximizeChangedSpy1.first().first().value<KWin::AbstractClient*>(), c);
    QCOMPARE(maximizeChangedSpy1.first().last().value<KWin::MaximizeMode>(), MaximizeFull);
    QCOMPARE(maximizeChangedSpy2.count(), 1);
    QCOMPARE(maximizeChangedSpy2.first().first().value<KWin::AbstractClient*>(), c);
    QCOMPARE(maximizeChangedSpy2.first().at(1).toBool(), true);
    QCOMPARE(maximizeChangedSpy2.first().at(2).toBool(), true);
    // at this point the geometry did not yet change
    QCOMPARE(c->geometry(), QRect(0, 0, 100, 50));
    // but quick tile mode already changed
    QCOMPARE(c->quickTileMode(), AbstractClient::QuickTileMaximize);
    QCOMPARE(c->maximizeMode(), MaximizeFull);
    QCOMPARE(c->geometryRestore(), QRect(0, 0, 100, 50));

    // but we got requested a new geometry
    QVERIFY(sizeChangeSpy.wait());
    QCOMPARE(sizeChangeSpy.count(), 1);
    QCOMPARE(sizeChangeSpy.first().first().toSize(), QSize(1280, 1024));

    // attach a new image
    Test::render(surface.data(), QSize(1280, 1024), Qt::red);
    m_connection->flush();

    QVERIFY(geometryChangedSpy.wait());
    QCOMPARE(geometryChangedSpy.count(), 2);
    QCOMPARE(c->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(c->geometryRestore(), QRect(0, 0, 100, 50));

    // go back to quick tile none
    QFETCH(AbstractClient::QuickTileMode, mode);
    c->setQuickTileMode(mode, true);
    QCOMPARE(quickTileChangedSpy.count(), 2);
    QCOMPARE(maximizeChangedSpy1.count(), 2);
    QCOMPARE(maximizeChangedSpy1.last().first().value<KWin::AbstractClient*>(), c);
    QCOMPARE(maximizeChangedSpy1.last().last().value<KWin::MaximizeMode>(), MaximizeRestore);
    QCOMPARE(maximizeChangedSpy2.count(), 2);
    QCOMPARE(maximizeChangedSpy2.last().first().value<KWin::AbstractClient*>(), c);
    QCOMPARE(maximizeChangedSpy2.last().at(1).toBool(), false);
    QCOMPARE(maximizeChangedSpy2.last().at(2).toBool(), false);
    QCOMPARE(c->quickTileMode(), AbstractClient::QuickTileNone);
    QCOMPARE(c->maximizeMode(), MaximizeRestore);
    // geometry not yet changed
    QCOMPARE(c->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(c->geometryRestore(), QRect(0, 0, 100, 50));
    // we got requested a new geometry
    QVERIFY(sizeChangeSpy.wait());
    QCOMPARE(sizeChangeSpy.count(), 2);
    QCOMPARE(sizeChangeSpy.last().first().toSize(), QSize(100, 50));

    // render again
    Test::render(surface.data(), QSize(100, 50), Qt::yellow);
    m_connection->flush();

    QVERIFY(geometryChangedSpy.wait());
    QCOMPARE(geometryChangedSpy.count(), 4);
    QCOMPARE(c->geometry(), QRect(0, 0, 100, 50));
    QCOMPARE(c->geometryRestore(), QRect(0, 0, 100, 50));
}

void QuickTilingTest::testQuickTilingKeyboardMove_data()
{
    QTest::addColumn<QPoint>("targetPos");
    QTest::addColumn<AbstractClient::QuickTileMode>("expectedMode");

    QTest::newRow("topRight") << QPoint(2559, 24) << AbstractClient::QuickTileMode(AbstractClient::QuickTileTop | AbstractClient::QuickTileRight);
    QTest::newRow("right") << QPoint(2559, 512) << AbstractClient::QuickTileMode(AbstractClient::QuickTileRight);
    QTest::newRow("bottomRight") << QPoint(2559, 1023) << AbstractClient::QuickTileMode(AbstractClient::QuickTileBottom | AbstractClient::QuickTileRight);
    QTest::newRow("bottomLeft") << QPoint(0, 1023) << AbstractClient::QuickTileMode(AbstractClient::QuickTileBottom | AbstractClient::QuickTileLeft);
    QTest::newRow("Left") << QPoint(0, 512) << AbstractClient::QuickTileMode(AbstractClient::QuickTileLeft);
    QTest::newRow("topLeft") << QPoint(0, 24) << AbstractClient::QuickTileMode(AbstractClient::QuickTileTop | AbstractClient::QuickTileLeft);
}

void QuickTilingTest::testQuickTilingKeyboardMove()
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
    QCOMPARE(c->quickTileMode(), AbstractClient::QuickTileNone);
    QCOMPARE(c->maximizeMode(), MaximizeRestore);

    QSignalSpy quickTileChangedSpy(c, &AbstractClient::quickTileModeChanged);
    QVERIFY(quickTileChangedSpy.isValid());

    workspace()->performWindowOperation(c, Options::UnrestrictedMoveOp);
    QCOMPARE(c, workspace()->getMovingClient());
    QCOMPARE(Cursor::pos(), QPoint(49, 24));

    QFETCH(QPoint, targetPos);
    quint32 timestamp = 1;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    while (Cursor::pos().x() > targetPos.x()) {
        kwinApp()->platform()->keyboardKeyPressed(KEY_LEFT, timestamp++);
        kwinApp()->platform()->keyboardKeyReleased(KEY_LEFT, timestamp++);
    }
    while (Cursor::pos().x() < targetPos.x()) {
        kwinApp()->platform()->keyboardKeyPressed(KEY_RIGHT, timestamp++);
        kwinApp()->platform()->keyboardKeyReleased(KEY_RIGHT, timestamp++);
    }
    while (Cursor::pos().y() < targetPos.y()) {
        kwinApp()->platform()->keyboardKeyPressed(KEY_DOWN, timestamp++);
        kwinApp()->platform()->keyboardKeyReleased(KEY_DOWN, timestamp++);
    }
    while (Cursor::pos().y() > targetPos.y()) {
        kwinApp()->platform()->keyboardKeyPressed(KEY_UP, timestamp++);
        kwinApp()->platform()->keyboardKeyReleased(KEY_UP, timestamp++);
    }
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_ENTER, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_ENTER, timestamp++);
    QCOMPARE(Cursor::pos(), targetPos);
    QVERIFY(!workspace()->getMovingClient());

    QCOMPARE(quickTileChangedSpy.count(), 1);
    QTEST(c->quickTileMode(), "expectedMode");
}



void QuickTilingTest::testQuickTilingPointerMove_data()
{
    QTest::addColumn<QPoint>("targetPos");
    QTest::addColumn<AbstractClient::QuickTileMode>("expectedMode");

    QTest::newRow("topRight") << QPoint(2559, 24) << AbstractClient::QuickTileMode(AbstractClient::QuickTileTop | AbstractClient::QuickTileRight);
    QTest::newRow("right") << QPoint(2559, 512) << AbstractClient::QuickTileMode(AbstractClient::QuickTileRight);
    QTest::newRow("bottomRight") << QPoint(2559, 1023) << AbstractClient::QuickTileMode(AbstractClient::QuickTileBottom | AbstractClient::QuickTileRight);
    QTest::newRow("bottomLeft") << QPoint(0, 1023) << AbstractClient::QuickTileMode(AbstractClient::QuickTileBottom | AbstractClient::QuickTileLeft);
    QTest::newRow("Left") << QPoint(0, 512) << AbstractClient::QuickTileMode(AbstractClient::QuickTileLeft);
    QTest::newRow("topLeft") << QPoint(0, 24) << AbstractClient::QuickTileMode(AbstractClient::QuickTileTop | AbstractClient::QuickTileLeft);
}

void QuickTilingTest::testQuickTilingPointerMove()
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
    QCOMPARE(c->quickTileMode(), AbstractClient::QuickTileNone);
    QCOMPARE(c->maximizeMode(), MaximizeRestore);

    QSignalSpy quickTileChangedSpy(c, &AbstractClient::quickTileModeChanged);
    QVERIFY(quickTileChangedSpy.isValid());

    workspace()->performWindowOperation(c, Options::UnrestrictedMoveOp);
    QCOMPARE(c, workspace()->getMovingClient());
    QCOMPARE(Cursor::pos(), QPoint(49, 24));

    QFETCH(QPoint, targetPos);
    quint32 timestamp = 1;
    kwinApp()->platform()->pointerMotion(targetPos, timestamp++);
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QCOMPARE(Cursor::pos(), targetPos);
    QVERIFY(!workspace()->getMovingClient());

    QCOMPARE(quickTileChangedSpy.count(), 1);
    QTEST(c->quickTileMode(), "expectedMode");
}

}

WAYLANDTEST_MAIN(KWin::QuickTilingTest)
#include "quick_tiling_test.moc"
