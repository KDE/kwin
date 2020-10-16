/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "platform.h"
#include "abstract_client.h"
#include "x11client.h"
#include "cursor.h"
#include "decorations/decorationbridge.h"
#include "decorations/settings.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "scripting/scripting.h"

#include <KDecoration2/DecoratedClient>
#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationSettings>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgshell.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QTemporaryFile>
#include <QTextStream>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

#include <linux/input.h>

Q_DECLARE_METATYPE(KWin::QuickTileMode)
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
    void testQuickTilingTouchMove_data();
    void testQuickTilingTouchMove();
    void testX11QuickTiling_data();
    void testX11QuickTiling();
    void testX11QuickTilingAfterVertMaximize_data();
    void testX11QuickTilingAfterVertMaximize();
    void testShortcut_data();
    void testShortcut();
    void testScript_data();
    void testScript();

private:
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
};

void QuickTilingTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<KWin::MaximizeMode>("MaximizeMode");
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    // set custom config which disables the Outline
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup group = config->group("Outline");
    group.writeEntry(QStringLiteral("QmlPath"), QString("/does/not/exist.qml"));
    group.sync();

    kwinApp()->setConfig(config);

    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
}

void QuickTilingTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Decoration));
    m_connection = Test::waylandConnection();
    m_compositor = Test::waylandCompositor();

    screens()->setCurrent(0);
}

void QuickTilingTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void QuickTilingTest::testQuickTiling_data()
{
    QTest::addColumn<QuickTileMode>("mode");
    QTest::addColumn<QRect>("expectedGeometry");
    QTest::addColumn<QRect>("secondScreen");
    QTest::addColumn<QuickTileMode>("expectedModeAfterToggle");

#define FLAG(name) QuickTileMode(QuickTileFlag::name)

    QTest::newRow("left")   << FLAG(Left)   << QRect(0, 0, 640, 1024)   << QRect(1280, 0, 640, 1024) << FLAG(Right);
    QTest::newRow("top")    << FLAG(Top)    << QRect(0, 0, 1280, 512)   << QRect(1280, 0, 1280, 512) << FLAG(Top);
    QTest::newRow("right")  << FLAG(Right)  << QRect(640, 0, 640, 1024) << QRect(1920, 0, 640, 1024) << QuickTileMode();
    QTest::newRow("bottom") << FLAG(Bottom) << QRect(0, 512, 1280, 512) << QRect(1280, 512, 1280, 512) << FLAG(Bottom);

    QTest::newRow("top left")     << (FLAG(Left)  | FLAG(Top))    << QRect(0, 0, 640, 512)     << QRect(1280, 0, 640, 512) << (FLAG(Right) | FLAG(Top));
    QTest::newRow("top right")    << (FLAG(Right) | FLAG(Top))    << QRect(640, 0, 640, 512)   << QRect(1920, 0, 640, 512) << QuickTileMode();
    QTest::newRow("bottom left")  << (FLAG(Left)  | FLAG(Bottom)) << QRect(0, 512, 640, 512)   << QRect(1280, 512, 640, 512) << (FLAG(Right)  | FLAG(Bottom));
    QTest::newRow("bottom right") << (FLAG(Right) | FLAG(Bottom)) << QRect(640, 512, 640, 512) << QRect(1920, 512, 640, 512) << QuickTileMode();

    QTest::newRow("maximize") << FLAG(Maximize) << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024) << QuickTileMode();

#undef FLAG
}

void QuickTilingTest::testQuickTiling()
{
    using namespace KWayland::Client;

    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // Map the client.
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 50));
    QCOMPARE(c->quickTileMode(), QuickTileMode(QuickTileFlag::None));

    // We have to receive a configure event when the client becomes active.
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);

    QSignalSpy quickTileChangedSpy(c, &AbstractClient::quickTileModeChanged);
    QVERIFY(quickTileChangedSpy.isValid());
    QSignalSpy frameGeometryChangedSpy(c, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());

    QFETCH(QuickTileMode, mode);
    QFETCH(QRect, expectedGeometry);
    c->setQuickTileMode(mode, true);
    QCOMPARE(quickTileChangedSpy.count(), 1);
    // at this point the geometry did not yet change
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 50));
    // but quick tile mode already changed
    QCOMPARE(c->quickTileMode(), mode);

    // but we got requested a new geometry
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), expectedGeometry.size());

    // attach a new image
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), expectedGeometry.size(), Qt::red);

    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), 1);
    QCOMPARE(c->frameGeometry(), expectedGeometry);

    // send window to other screen
    QCOMPARE(c->screen(), 0);
    c->sendToScreen(1);
    QCOMPARE(c->screen(), 1);
    // quick tile should not be changed
    QCOMPARE(c->quickTileMode(), mode);
    QTEST(c->frameGeometry(), "secondScreen");

    // now try to toggle again
    c->setQuickTileMode(mode, true);
    QTEST(c->quickTileMode(), "expectedModeAfterToggle");
}

void QuickTilingTest::testQuickMaximizing_data()
{
    QTest::addColumn<QuickTileMode>("mode");

#define FLAG(name) QuickTileMode(QuickTileFlag::name)

    QTest::newRow("maximize") << FLAG(Maximize);
    QTest::newRow("none") << FLAG(None);

#undef FLAG
}

void QuickTilingTest::testQuickMaximizing()
{
    using namespace KWayland::Client;

    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // Map the client.
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 50));
    QCOMPARE(c->quickTileMode(), QuickTileMode(QuickTileFlag::None));
    QCOMPARE(c->maximizeMode(), MaximizeRestore);

    // We have to receive a configure event upon becoming active.
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);

    QSignalSpy quickTileChangedSpy(c, &AbstractClient::quickTileModeChanged);
    QVERIFY(quickTileChangedSpy.isValid());
    QSignalSpy frameGeometryChangedSpy(c, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    QSignalSpy maximizeChangedSpy1(c, qOverload<AbstractClient *, MaximizeMode>(&AbstractClient::clientMaximizedStateChanged));
    QVERIFY(maximizeChangedSpy1.isValid());
    QSignalSpy maximizeChangedSpy2(c, qOverload<AbstractClient *, bool, bool>(&AbstractClient::clientMaximizedStateChanged));
    QVERIFY(maximizeChangedSpy2.isValid());

    c->setQuickTileMode(QuickTileFlag::Maximize, true);
    QCOMPARE(quickTileChangedSpy.count(), 1);

    // at this point the geometry did not yet change
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 50));
    // but quick tile mode already changed
    QCOMPARE(c->quickTileMode(), QuickTileFlag::Maximize);
    QCOMPARE(c->geometryRestore(), QRect(0, 0, 100, 50));

    // but we got requested a new geometry
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), QSize(1280, 1024));

    // attach a new image
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(1280, 1024), Qt::red);

    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), 1);
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(c->geometryRestore(), QRect(0, 0, 100, 50));

    // client is now set to maximised
    QCOMPARE(maximizeChangedSpy1.count(), 1);
    QCOMPARE(maximizeChangedSpy1.first().first().value<KWin::AbstractClient*>(), c);
    QCOMPARE(maximizeChangedSpy1.first().last().value<KWin::MaximizeMode>(), MaximizeFull);
    QCOMPARE(maximizeChangedSpy2.count(), 1);
    QCOMPARE(maximizeChangedSpy2.first().first().value<KWin::AbstractClient*>(), c);
    QCOMPARE(maximizeChangedSpy2.first().at(1).toBool(), true);
    QCOMPARE(maximizeChangedSpy2.first().at(2).toBool(), true);
    QCOMPARE(c->maximizeMode(), MaximizeFull);

    // go back to quick tile none
    QFETCH(QuickTileMode, mode);
    c->setQuickTileMode(mode, true);
    QCOMPARE(c->quickTileMode(), QuickTileMode(QuickTileFlag::None));
    QCOMPARE(quickTileChangedSpy.count(), 2);
    // geometry not yet changed
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(c->geometryRestore(), QRect(0, 0, 100, 50));
    // we got requested a new geometry
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 3);
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), QSize(100, 50));

    // render again
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), QSize(100, 50), Qt::yellow);

    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), 2);
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 50));
    QCOMPARE(c->geometryRestore(), QRect(0, 0, 100, 50));
    QCOMPARE(maximizeChangedSpy1.count(), 2);
    QCOMPARE(maximizeChangedSpy1.last().first().value<KWin::AbstractClient*>(), c);
    QCOMPARE(maximizeChangedSpy1.last().last().value<KWin::MaximizeMode>(), MaximizeRestore);
    QCOMPARE(maximizeChangedSpy2.count(), 2);
    QCOMPARE(maximizeChangedSpy2.last().first().value<KWin::AbstractClient*>(), c);
    QCOMPARE(maximizeChangedSpy2.last().at(1).toBool(), false);
    QCOMPARE(maximizeChangedSpy2.last().at(2).toBool(), false);
}

void QuickTilingTest::testQuickTilingKeyboardMove_data()
{
    QTest::addColumn<QPoint>("targetPos");
    QTest::addColumn<QuickTileMode>("expectedMode");

    QTest::newRow("topRight") << QPoint(2559, 24) << QuickTileMode(QuickTileFlag::Top | QuickTileFlag::Right);
    QTest::newRow("right") << QPoint(2559, 512) << QuickTileMode(QuickTileFlag::Right);
    QTest::newRow("bottomRight") << QPoint(2559, 1023) << QuickTileMode(QuickTileFlag::Bottom | QuickTileFlag::Right);
    QTest::newRow("bottomLeft") << QPoint(0, 1023) << QuickTileMode(QuickTileFlag::Bottom | QuickTileFlag::Left);
    QTest::newRow("Left") << QPoint(0, 512) << QuickTileMode(QuickTileFlag::Left);
    QTest::newRow("topLeft") << QPoint(0, 24) << QuickTileMode(QuickTileFlag::Top | QuickTileFlag::Left);
}

void QuickTilingTest::testQuickTilingKeyboardMove()
{
    using namespace KWayland::Client;

    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    QSignalSpy sizeChangeSpy(shellSurface.data(), &XdgShellSurface::sizeChanged);
    QVERIFY(sizeChangeSpy.isValid());
    // let's render
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 50));
    QCOMPARE(c->quickTileMode(), QuickTileMode(QuickTileFlag::None));
    QCOMPARE(c->maximizeMode(), MaximizeRestore);

    QSignalSpy quickTileChangedSpy(c, &AbstractClient::quickTileModeChanged);
    QVERIFY(quickTileChangedSpy.isValid());

    workspace()->performWindowOperation(c, Options::UnrestrictedMoveOp);
    QCOMPARE(c, workspace()->moveResizeClient());
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(49, 24));

    QFETCH(QPoint, targetPos);
    quint32 timestamp = 1;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    while (Cursors::self()->mouse()->pos().x() > targetPos.x()) {
        kwinApp()->platform()->keyboardKeyPressed(KEY_LEFT, timestamp++);
        kwinApp()->platform()->keyboardKeyReleased(KEY_LEFT, timestamp++);
    }
    while (Cursors::self()->mouse()->pos().x() < targetPos.x()) {
        kwinApp()->platform()->keyboardKeyPressed(KEY_RIGHT, timestamp++);
        kwinApp()->platform()->keyboardKeyReleased(KEY_RIGHT, timestamp++);
    }
    while (Cursors::self()->mouse()->pos().y() < targetPos.y()) {
        kwinApp()->platform()->keyboardKeyPressed(KEY_DOWN, timestamp++);
        kwinApp()->platform()->keyboardKeyReleased(KEY_DOWN, timestamp++);
    }
    while (Cursors::self()->mouse()->pos().y() > targetPos.y()) {
        kwinApp()->platform()->keyboardKeyPressed(KEY_UP, timestamp++);
        kwinApp()->platform()->keyboardKeyReleased(KEY_UP, timestamp++);
    }
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_ENTER, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_ENTER, timestamp++);
    QCOMPARE(Cursors::self()->mouse()->pos(), targetPos);
    QVERIFY(!workspace()->moveResizeClient());

    QCOMPARE(quickTileChangedSpy.count(), 1);
    QTEST(c->quickTileMode(), "expectedMode");
}

void QuickTilingTest::testQuickTilingPointerMove_data()
{
    QTest::addColumn<QPoint>("targetPos");
    QTest::addColumn<QuickTileMode>("expectedMode");

    QTest::newRow("topRight") << QPoint(2559, 24) << QuickTileMode(QuickTileFlag::Top | QuickTileFlag::Right);
    QTest::newRow("right") << QPoint(2559, 512) << QuickTileMode(QuickTileFlag::Right);
    QTest::newRow("bottomRight") << QPoint(2559, 1023) << QuickTileMode(QuickTileFlag::Bottom | QuickTileFlag::Right);
    QTest::newRow("bottomLeft") << QPoint(0, 1023) << QuickTileMode(QuickTileFlag::Bottom | QuickTileFlag::Left);
    QTest::newRow("Left") << QPoint(0, 512) << QuickTileMode(QuickTileFlag::Left);
    QTest::newRow("topLeft") << QPoint(0, 24) << QuickTileMode(QuickTileFlag::Top | QuickTileFlag::Left);
}

void QuickTilingTest::testQuickTilingPointerMove()
{
    using namespace KWayland::Client;

    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(
        surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QVERIFY(!shellSurface.isNull());

    // wait for the initial configure event
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);

    // let's render
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);

    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 50));
    QCOMPARE(c->quickTileMode(), QuickTileMode(QuickTileFlag::None));
    QCOMPARE(c->maximizeMode(), MaximizeRestore);

    // we have to receive a configure event when the client becomes active
    QVERIFY(configureRequestedSpy.wait());
    QTRY_COMPARE(configureRequestedSpy.count(), 2);

    QSignalSpy quickTileChangedSpy(c, &AbstractClient::quickTileModeChanged);
    QVERIFY(quickTileChangedSpy.isValid());

    workspace()->performWindowOperation(c, Options::UnrestrictedMoveOp);
    QCOMPARE(c, workspace()->moveResizeClient());
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(49, 24));
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 3);

    QFETCH(QPoint, targetPos);
    quint32 timestamp = 1;
    kwinApp()->platform()->pointerMotion(targetPos, timestamp++);
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QCOMPARE(Cursors::self()->mouse()->pos(), targetPos);
    QVERIFY(!workspace()->moveResizeClient());

    QCOMPARE(quickTileChangedSpy.count(), 1);
    QTEST(c->quickTileMode(), "expectedMode");
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 4);
    QCOMPARE(false, configureRequestedSpy.last().first().toSize().isEmpty());
}

void QuickTilingTest::testQuickTilingTouchMove_data()
{
    QTest::addColumn<QPoint>("targetPos");
    QTest::addColumn<QuickTileMode>("expectedMode");

    QTest::newRow("topRight") << QPoint(2559, 24) << QuickTileMode(QuickTileFlag::Top | QuickTileFlag::Right);
    QTest::newRow("right") << QPoint(2559, 512) << QuickTileMode(QuickTileFlag::Right);
    QTest::newRow("bottomRight") << QPoint(2559, 1023) << QuickTileMode(QuickTileFlag::Bottom | QuickTileFlag::Right);
    QTest::newRow("bottomLeft") << QPoint(0, 1023) << QuickTileMode(QuickTileFlag::Bottom | QuickTileFlag::Left);
    QTest::newRow("Left") << QPoint(0, 512) << QuickTileMode(QuickTileFlag::Left);
    QTest::newRow("topLeft") << QPoint(0, 24) << QuickTileMode(QuickTileFlag::Top | QuickTileFlag::Left);
}

void QuickTilingTest::testQuickTilingTouchMove()
{
    // test verifies that touch on decoration also allows quick tiling
    // see BUG: 390113
    using namespace KWayland::Client;

    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<ServerSideDecoration> deco(Test::waylandServerSideDecoration()->create(surface.data()));

    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(
        surface.data(), surface.data(), Test::CreationSetup::CreateOnly));
    QVERIFY(!shellSurface.isNull());

    // wait for the initial configure event
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);

    // let's render
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(1000, 50), Qt::blue);

    QVERIFY(c);
    QVERIFY(c->isDecorated());
    const auto decoration = c->decoration();
    QCOMPARE(workspace()->activeClient(), c);
    QCOMPARE(c->frameGeometry(), QRect(-decoration->borderLeft(), 0,
                                       1000 + decoration->borderLeft() + decoration->borderRight(),
                                       50 + decoration->borderTop() + decoration->borderBottom()));
    QCOMPARE(c->quickTileMode(), QuickTileMode(QuickTileFlag::None));
    QCOMPARE(c->maximizeMode(), MaximizeRestore);

    // we have to receive a configure event when the client becomes active
    QVERIFY(configureRequestedSpy.wait());
    QTRY_COMPARE(configureRequestedSpy.count(), 2);

    QSignalSpy quickTileChangedSpy(c, &AbstractClient::quickTileModeChanged);
    QVERIFY(quickTileChangedSpy.isValid());

    quint32 timestamp = 1;
    kwinApp()->platform()->touchDown(0, QPointF(c->frameGeometry().center().x(), c->frameGeometry().y() + decoration->borderTop() / 2), timestamp++);
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(c, workspace()->moveResizeClient());
    QCOMPARE(configureRequestedSpy.count(), 3);

    QFETCH(QPoint, targetPos);
    kwinApp()->platform()->touchMotion(0, targetPos, timestamp++);
    kwinApp()->platform()->touchUp(0, timestamp++);
    QVERIFY(!workspace()->moveResizeClient());


    // When there are no borders, there is no change to them when quick-tiling.
    // TODO: we should test both cases with fixed fake decoration for autotests.
    const bool hasBorders = Decoration::DecorationBridge::self()->settings()->borderSize() != KDecoration2::BorderSize::None;

    QCOMPARE(quickTileChangedSpy.count(), 1);
    QTEST(c->quickTileMode(), "expectedMode");
    QVERIFY(configureRequestedSpy.wait());
    QTRY_COMPARE(configureRequestedSpy.count(), hasBorders ? 5 : 4);
    QCOMPARE(false, configureRequestedSpy.last().first().toSize().isEmpty());
}

struct XcbConnectionDeleter
{
    static inline void cleanup(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

void QuickTilingTest::testX11QuickTiling_data()
{
    QTest::addColumn<QuickTileMode>("mode");
    QTest::addColumn<QRect>("expectedGeometry");
    QTest::addColumn<int>("screen");
    QTest::addColumn<QuickTileMode>("modeAfterToggle");

#define FLAG(name) QuickTileMode(QuickTileFlag::name)

    QTest::newRow("left")   << FLAG(Left)   << QRect(0, 0, 640, 1024) << 0 << QuickTileMode();
    QTest::newRow("top")    << FLAG(Top)    << QRect(0, 0, 1280, 512) << 1 << FLAG(Top);
    QTest::newRow("right")  << FLAG(Right)  << QRect(640, 0, 640, 1024) << 1 << FLAG(Left);
    QTest::newRow("bottom") << FLAG(Bottom) << QRect(0, 512, 1280, 512) << 1 << FLAG(Bottom);

    QTest::newRow("top left")     << (FLAG(Left)  | FLAG(Top))    << QRect(0, 0, 640, 512) << 0 << QuickTileMode();
    QTest::newRow("top right")    << (FLAG(Right) | FLAG(Top))    << QRect(640, 0, 640, 512) << 1 << (FLAG(Left) | FLAG(Top));
    QTest::newRow("bottom left")  << (FLAG(Left)  | FLAG(Bottom)) << QRect(0, 512, 640, 512) << 0 << QuickTileMode();
    QTest::newRow("bottom right") << (FLAG(Right) | FLAG(Bottom)) << QRect(640, 512, 640, 512) << 1 << (FLAG(Left) | FLAG(Bottom));

    QTest::newRow("maximize") << FLAG(Maximize) << QRect(0, 0, 1280, 1024) << 0 << QuickTileMode();

#undef FLAG
}
void QuickTilingTest::testX11QuickTiling()
{
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t w = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, w, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), w, &hints);
    xcb_map_window(c.data(), w);
    xcb_flush(c.data());

    // we should get a client for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    X11Client *client = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(client);
    QCOMPARE(client->window(), w);

    // now quick tile
    QSignalSpy quickTileChangedSpy(client, &AbstractClient::quickTileModeChanged);
    QVERIFY(quickTileChangedSpy.isValid());
    const QRect origGeo = client->frameGeometry();
    QFETCH(QuickTileMode, mode);
    client->setQuickTileMode(mode, true);
    QCOMPARE(client->quickTileMode(), mode);
    QTEST(client->frameGeometry(), "expectedGeometry");
    QCOMPARE(client->geometryRestore(), origGeo);
    QEXPECT_FAIL("maximize", "For maximize we get two changed signals", Continue);
    QCOMPARE(quickTileChangedSpy.count(), 1);

    // quick tile to same edge again should also act like send to screen
    QCOMPARE(client->screen(), 0);
    client->setQuickTileMode(mode, true);
    QTEST(client->screen(), "screen");
    QTEST(client->quickTileMode(), "modeAfterToggle");
    QCOMPARE(client->geometryRestore(), origGeo);

    // and destroy the window again
    xcb_unmap_window(c.data(), w);
    xcb_destroy_window(c.data(), w);
    xcb_flush(c.data());
    c.reset();

    QSignalSpy windowClosedSpy(client, &X11Client::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    QVERIFY(windowClosedSpy.wait());
}

void QuickTilingTest::testX11QuickTilingAfterVertMaximize_data()
{
    QTest::addColumn<QuickTileMode>("mode");
    QTest::addColumn<QRect>("expectedGeometry");

#define FLAG(name) QuickTileMode(QuickTileFlag::name)

    QTest::newRow("left")   << FLAG(Left)   << QRect(0, 0, 640, 1024);
    QTest::newRow("top")    << FLAG(Top)    << QRect(0, 0, 1280, 512);
    QTest::newRow("right")  << FLAG(Right)  << QRect(640, 0, 640, 1024);
    QTest::newRow("bottom") << FLAG(Bottom) << QRect(0, 512, 1280, 512);

    QTest::newRow("top left")     << (FLAG(Left)  | FLAG(Top))    << QRect(0, 0, 640, 512);
    QTest::newRow("top right")    << (FLAG(Right) | FLAG(Top))    << QRect(640, 0, 640, 512);
    QTest::newRow("bottom left")  << (FLAG(Left)  | FLAG(Bottom)) << QRect(0, 512, 640, 512);
    QTest::newRow("bottom right") << (FLAG(Right) | FLAG(Bottom)) << QRect(640, 512, 640, 512);

    QTest::newRow("maximize") << FLAG(Maximize) << QRect(0, 0, 1280, 1024);

#undef FLAG
}

void QuickTilingTest::testX11QuickTilingAfterVertMaximize()
{
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t w = xcb_generate_id(c.data());
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, w, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.data(), w, &hints);
    xcb_map_window(c.data(), w);
    xcb_flush(c.data());

    // we should get a client for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(windowCreatedSpy.isValid());
    QVERIFY(windowCreatedSpy.wait());
    X11Client *client = windowCreatedSpy.first().first().value<X11Client *>();
    QVERIFY(client);
    QCOMPARE(client->window(), w);

    const QRect origGeo = client->frameGeometry();
    QCOMPARE(client->maximizeMode(), MaximizeRestore);
    // vertically maximize the window
    client->maximize(client->maximizeMode() ^ MaximizeVertical);
    QCOMPARE(client->frameGeometry().width(), origGeo.width());
    QCOMPARE(client->height(), screens()->size(client->screen()).height());
    QCOMPARE(client->geometryRestore(), origGeo);

    // now quick tile
    QSignalSpy quickTileChangedSpy(client, &AbstractClient::quickTileModeChanged);
    QVERIFY(quickTileChangedSpy.isValid());
    QFETCH(QuickTileMode, mode);
    client->setQuickTileMode(mode, true);
    QCOMPARE(client->quickTileMode(), mode);
    QTEST(client->frameGeometry(), "expectedGeometry");
    QEXPECT_FAIL("", "We get two changed events", Continue);
    QCOMPARE(quickTileChangedSpy.count(), 1);

    // and destroy the window again
    xcb_unmap_window(c.data(), w);
    xcb_destroy_window(c.data(), w);
    xcb_flush(c.data());
    c.reset();

    QSignalSpy windowClosedSpy(client, &X11Client::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    QVERIFY(windowClosedSpy.wait());
}

void QuickTilingTest::testShortcut_data()
{
    QTest::addColumn<QStringList>("shortcutList");
    QTest::addColumn<QuickTileMode>("expectedMode");
    QTest::addColumn<QRect>("expectedGeometry");

#define FLAG(name) QuickTileMode(QuickTileFlag::name)
    QTest::newRow("top") << QStringList{QStringLiteral("Window Quick Tile Top")} << FLAG(Top) << QRect(0, 0, 1280, 512);
    QTest::newRow("bottom") << QStringList{QStringLiteral("Window Quick Tile Bottom")} << FLAG(Bottom) << QRect(0, 512, 1280, 512);
    QTest::newRow("top right") << QStringList{QStringLiteral("Window Quick Tile Top Right")} << (FLAG(Top) | FLAG(Right)) << QRect(640, 0, 640, 512);
    QTest::newRow("top left") << QStringList{QStringLiteral("Window Quick Tile Top Left")} << (FLAG(Top) | FLAG(Left)) << QRect(0, 0, 640, 512);
    QTest::newRow("bottom right") << QStringList{QStringLiteral("Window Quick Tile Bottom Right")} << (FLAG(Bottom) | FLAG(Right)) << QRect(640, 512, 640, 512);
    QTest::newRow("bottom left") << QStringList{QStringLiteral("Window Quick Tile Bottom Left")} << (FLAG(Bottom) | FLAG(Left)) << QRect(0, 512, 640, 512);
    QTest::newRow("left") << QStringList{QStringLiteral("Window Quick Tile Left")} << FLAG(Left) << QRect(0, 0, 640, 1024);
    QTest::newRow("right") << QStringList{QStringLiteral("Window Quick Tile Right")} << FLAG(Right) << QRect(640, 0, 640, 1024);

    // Test combined actions for corner tiling
    QTest::newRow("top left combined") << QStringList{QStringLiteral("Window Quick Tile Left"), QStringLiteral("Window Quick Tile Top")} << (FLAG(Top) | FLAG(Left)) << QRect(0, 0, 640, 512);
    QTest::newRow("top right combined") << QStringList{QStringLiteral("Window Quick Tile Right"), QStringLiteral("Window Quick Tile Top")} << (FLAG(Top) | FLAG(Right)) << QRect(640, 0, 640, 512);
    QTest::newRow("bottom left combined") << QStringList{QStringLiteral("Window Quick Tile Left"), QStringLiteral("Window Quick Tile Bottom")} << (FLAG(Bottom) | FLAG(Left)) << QRect(0, 512, 640, 512);
    QTest::newRow("bottom right combined") << QStringList{QStringLiteral("Window Quick Tile Right"), QStringLiteral("Window Quick Tile Bottom")} << (FLAG(Bottom) | FLAG(Right)) << QRect(640, 512, 640, 512);
#undef FLAG
}

void QuickTilingTest::testShortcut()
{
    using namespace KWayland::Client;

    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // Map the client.
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 50));
    QCOMPARE(c->quickTileMode(), QuickTileMode(QuickTileFlag::None));

    // We have to receive a configure event when the client becomes active.
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);

    QFETCH(QStringList, shortcutList);
    QFETCH(QRect, expectedGeometry);

    const int numberOfQuickTileActions = shortcutList.count();

    if (numberOfQuickTileActions > 1) {
        QTest::qWait(1001);
    }

    for (QString shortcut : shortcutList) {
        // invoke global shortcut through dbus
        auto msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.kglobalaccel"),
            QStringLiteral("/component/kwin"),
            QStringLiteral("org.kde.kglobalaccel.Component"),
            QStringLiteral("invokeShortcut"));
        msg.setArguments(QList<QVariant>{shortcut});
        QDBusConnection::sessionBus().asyncCall(msg);
    }

    QSignalSpy quickTileChangedSpy(c, &AbstractClient::quickTileModeChanged);
    QVERIFY(quickTileChangedSpy.isValid());
    QTRY_COMPARE(quickTileChangedSpy.count(), numberOfQuickTileActions);
    // at this point the geometry did not yet change
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 50));
    // but quick tile mode already changed
    QTEST(c->quickTileMode(), "expectedMode");

    // but we got requested a new geometry
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), expectedGeometry.size());

    // attach a new image
    QSignalSpy frameGeometryChangedSpy(c, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), expectedGeometry.size(), Qt::red);

    QVERIFY(frameGeometryChangedSpy.wait());
    QEXPECT_FAIL("maximize", "Geometry changed called twice for maximize", Continue);
    QCOMPARE(frameGeometryChangedSpy.count(), 1);
    QCOMPARE(c->frameGeometry(), expectedGeometry);
}

void QuickTilingTest::testScript_data()
{
    QTest::addColumn<QString>("action");
    QTest::addColumn<QuickTileMode>("expectedMode");
    QTest::addColumn<QRect>("expectedGeometry");

#define FLAG(name) QuickTileMode(QuickTileFlag::name)
    QTest::newRow("top") << QStringLiteral("Top") << FLAG(Top) << QRect(0, 0, 1280, 512);
    QTest::newRow("bottom") << QStringLiteral("Bottom") << FLAG(Bottom) << QRect(0, 512, 1280, 512);
    QTest::newRow("top right") << QStringLiteral("TopRight") << (FLAG(Top) | FLAG(Right)) << QRect(640, 0, 640, 512);
    QTest::newRow("top left") << QStringLiteral("TopLeft") << (FLAG(Top) | FLAG(Left)) << QRect(0, 0, 640, 512);
    QTest::newRow("bottom right") << QStringLiteral("BottomRight") << (FLAG(Bottom) | FLAG(Right)) << QRect(640, 512, 640, 512);
    QTest::newRow("bottom left") << QStringLiteral("BottomLeft") << (FLAG(Bottom) | FLAG(Left)) << QRect(0, 512, 640, 512);
    QTest::newRow("left") << QStringLiteral("Left") << FLAG(Left) << QRect(0, 0, 640, 1024);
    QTest::newRow("right") << QStringLiteral("Right") << FLAG(Right) << QRect(640, 0, 640, 1024);
#undef FLAG
}

void QuickTilingTest::testScript()
{
    using namespace KWayland::Client;

    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());

    // Map the client.
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);
    QCOMPARE(workspace()->activeClient(), c);
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 50));
    QCOMPARE(c->quickTileMode(), QuickTileMode(QuickTileFlag::None));

    // We have to receive a configure event upon the client becoming active.
    QSignalSpy configureRequestedSpy(shellSurface.data(), &XdgShellSurface::configureRequested);
    QVERIFY(configureRequestedSpy.isValid());
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 1);

    QSignalSpy quickTileChangedSpy(c, &AbstractClient::quickTileModeChanged);
    QVERIFY(quickTileChangedSpy.isValid());
    QSignalSpy frameGeometryChangedSpy(c, &AbstractClient::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());

    QVERIFY(Scripting::self());
    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    QTextStream out(&tmpFile);

    QFETCH(QString, action);
    out << "workspace.slotWindowQuickTile" << action << "()";
    out.flush();

    QFETCH(QuickTileMode, expectedMode);
    QFETCH(QRect, expectedGeometry);

    const int id = Scripting::self()->loadScript(tmpFile.fileName());
    QVERIFY(id != -1);
    QVERIFY(Scripting::self()->isScriptLoaded(tmpFile.fileName()));
    auto s = Scripting::self()->findScript(tmpFile.fileName());
    QVERIFY(s);
    QSignalSpy runningChangedSpy(s, &AbstractScript::runningChanged);
    QVERIFY(runningChangedSpy.isValid());
    s->run();

    QVERIFY(quickTileChangedSpy.wait());
    QCOMPARE(quickTileChangedSpy.count(), 1);

    QCOMPARE(runningChangedSpy.count(), 1);
    QCOMPARE(runningChangedSpy.first().first().toBool(), true);

    // at this point the geometry did not yet change
    QCOMPARE(c->frameGeometry(), QRect(0, 0, 100, 50));
    // but quick tile mode already changed
    QCOMPARE(c->quickTileMode(), expectedMode);

    // but we got requested a new geometry
    QVERIFY(configureRequestedSpy.wait());
    QCOMPARE(configureRequestedSpy.count(), 2);
    QCOMPARE(configureRequestedSpy.last().at(0).toSize(), expectedGeometry.size());

    // attach a new image
    shellSurface->ackConfigure(configureRequestedSpy.last().at(2).value<quint32>());
    Test::render(surface.data(), expectedGeometry.size(), Qt::red);

    QVERIFY(frameGeometryChangedSpy.wait());
    QEXPECT_FAIL("maximize", "Geometry changed called twice for maximize", Continue);
    QCOMPARE(frameGeometryChangedSpy.count(), 1);
    QCOMPARE(c->frameGeometry(), expectedGeometry);
}

}

WAYLANDTEST_MAIN(KWin::QuickTilingTest)
#include "quick_tiling_test.moc"
