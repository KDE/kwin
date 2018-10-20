/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#include "screenedge.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "shell_client.h"
#include <kwineffects.h>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/surface_interface.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_transient_placement-0");

class TransientPlacementTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testSimplePosition_data();
    void testSimplePosition();
    void testDecorationPosition_data();
    void testDecorationPosition();

private:
    AbstractClient *showWindow(const QSize &size, bool decorated = false, KWayland::Client::Surface *parent = nullptr, const QPoint &offset = QPoint());
    KWayland::Client::Surface *surfaceForClient(AbstractClient *c) const;
};

void TransientPlacementTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
    waylandServer()->initWorkspace();
}

void TransientPlacementTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Decoration));

    screens()->setCurrent(0);
    Cursor::setPos(QPoint(640, 512));
}

void TransientPlacementTest::cleanup()
{
    Test::destroyWaylandConnection();
}

AbstractClient *TransientPlacementTest::showWindow(const QSize &size, bool decorated, KWayland::Client::Surface *parent, const QPoint &offset)
{
    using namespace KWayland::Client;
#define VERIFY(statement) \
    if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__))\
        return nullptr;
#define COMPARE(actual, expected) \
    if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__))\
        return nullptr;

    Surface *surface = Test::createSurface(Test::waylandCompositor());
    VERIFY(surface);
    ShellSurface *shellSurface = Test::createShellSurface(surface, surface);
    VERIFY(shellSurface);
    if (parent) {
        shellSurface->setTransient(parent, offset);
    }
    if (decorated) {
        auto deco = Test::waylandServerSideDecoration()->create(surface, surface);
        QSignalSpy decoSpy(deco, &ServerSideDecoration::modeChanged);
        VERIFY(decoSpy.isValid());
        VERIFY(decoSpy.wait());
        deco->requestMode(ServerSideDecoration::Mode::Server);
        VERIFY(decoSpy.wait());
        COMPARE(deco->mode(), ServerSideDecoration::Mode::Server);
    }
    // let's render
    auto c = Test::renderAndWaitForShown(surface, size, Qt::blue);

    VERIFY(c);
    COMPARE(workspace()->activeClient(), c);

#undef VERIFY
#undef COMPARE

    return c;
}

KWayland::Client::Surface *TransientPlacementTest::surfaceForClient(AbstractClient *c) const
{
    const auto &surfaces = KWayland::Client::Surface::all();
    auto it = std::find_if(surfaces.begin(), surfaces.end(), [c] (KWayland::Client::Surface *s) { return s->id() == c->surface()->id(); });
    if (it != surfaces.end()) {
        return *it;
    }
    return nullptr;
}

void TransientPlacementTest::testSimplePosition_data()
{
    QTest::addColumn<QSize>("parentSize");
    QTest::addColumn<QPoint>("parentPosition");
    QTest::addColumn<QSize>("transientSize");
    QTest::addColumn<QPoint>("transientOffset");
    QTest::addColumn<QRect>("expectedGeometry");

    QTest::newRow("0/0") << QSize(640, 512) << QPoint(0, 0) << QSize(10, 100) << QPoint(0, 0) << QRect(0, 0, 10, 100);
    QTest::newRow("bottomRight") << QSize(640, 512) << QPoint(0, 0) << QSize(10, 100) << QPoint(639, 511) << QRect(639, 511, 10, 100);
    QTest::newRow("offset") << QSize(640, 512) << QPoint(200, 300) << QSize(100, 10) << QPoint(320, 256) << QRect(520, 556, 100, 10);
    QTest::newRow("right border") << QSize(1280, 1024) << QPoint(0, 0) << QSize(10, 100) << QPoint(1279, 50) << QRect(1270, 50, 10, 100);
    QTest::newRow("bottom border") << QSize(1280, 1024) << QPoint(0, 0) << QSize(10, 100) << QPoint(512, 1020) << QRect(512, 924, 10, 100);
    QTest::newRow("bottom right") << QSize(1280, 1024) << QPoint(0, 0) << QSize(10, 100) << QPoint(1279, 1020) << QRect(1270, 924, 10, 100);
    QTest::newRow("top border") << QSize(1280, 1024) << QPoint(0, -100) << QSize(10, 100) << QPoint(512, 50) << QRect(512, 0, 10, 100);
    QTest::newRow("left border") << QSize(1280, 1024) << QPoint(-100, 0) << QSize(100, 10) << QPoint(50, 512) << QRect(0, 512, 100, 10);
    QTest::newRow("top left") << QSize(1280, 1024) << QPoint(-100, -100) << QSize(100, 100) << QPoint(50, 50) << QRect(0, 0, 100, 100);
    QTest::newRow("bottom left") << QSize(1280, 1024) << QPoint(-100, 0) << QSize(100, 100) << QPoint(50, 1000) << QRect(0, 924, 100, 100);
}

void TransientPlacementTest::testSimplePosition()
{
    // this test verifies that the position of a transient window is taken from the passed position
    // there are no further constraints like window too large to fit screen, cascading transients, etc
    // some test cases also verify that the transient fits on the screen
    QFETCH(QSize, parentSize);
    AbstractClient *parent = showWindow(parentSize);
    QVERIFY(parent->clientPos().isNull());
    QVERIFY(!parent->isDecorated());
    QFETCH(QPoint, parentPosition);
    parent->move(parentPosition);
    QFETCH(QSize, transientSize);
    QFETCH(QPoint, transientOffset);
    AbstractClient *transient = showWindow(transientSize, false, surfaceForClient(parent), transientOffset);
    QVERIFY(transient);
    QVERIFY(!transient->isDecorated());
    QVERIFY(transient->hasTransientPlacementHint());
    QTEST(transient->geometry(), "expectedGeometry");
}

void TransientPlacementTest::testDecorationPosition_data()
{
    QTest::addColumn<QSize>("parentSize");
    QTest::addColumn<QPoint>("parentPosition");
    QTest::addColumn<QSize>("transientSize");
    QTest::addColumn<QPoint>("transientOffset");
    QTest::addColumn<QRect>("expectedGeometry");

    QTest::newRow("0/0") << QSize(640, 512) << QPoint(0, 0) << QSize(10, 100) << QPoint(0, 0) << QRect(0, 0, 10, 100);
    QTest::newRow("bottomRight") << QSize(640, 512) << QPoint(0, 0) << QSize(10, 100) << QPoint(639, 511) << QRect(639, 511, 10, 100);
    QTest::newRow("offset") << QSize(640, 512) << QPoint(200, 300) << QSize(100, 10) << QPoint(320, 256) << QRect(520, 556, 100, 10);
}

void TransientPlacementTest::testDecorationPosition()
{
    // this test verifies that a transient window is correctly placed if the parent window has a
    // server side decoration
    QFETCH(QSize, parentSize);
    AbstractClient *parent = showWindow(parentSize, true);
    QVERIFY(!parent->clientPos().isNull());
    QVERIFY(parent->isDecorated());
    QFETCH(QPoint, parentPosition);
    parent->move(parentPosition);
    QFETCH(QSize, transientSize);
    QFETCH(QPoint, transientOffset);
    AbstractClient *transient = showWindow(transientSize, false, surfaceForClient(parent), transientOffset);
    QVERIFY(transient);
    QVERIFY(!transient->isDecorated());
    QVERIFY(transient->hasTransientPlacementHint());
    QFETCH(QRect, expectedGeometry);
    expectedGeometry.translate(parent->clientPos());
    QCOMPARE(transient->geometry(), expectedGeometry);
}

}

WAYLANDTEST_MAIN(KWin::TransientPlacementTest)
#include "transient_placement.moc"
