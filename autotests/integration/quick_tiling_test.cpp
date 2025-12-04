/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "cursor.h"
#include "decorations/decorationbridge.h"
#include "decorations/settings.h"
#include "pointer_input.h"
#include "scripting/scripting.h"
#include "tiles/tilemanager.h"
#include "utils/common.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationSettings>

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/surface.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QTemporaryFile>
#include <QTextStream>

#include <linux/input.h>

#if KWIN_BUILD_X11
#include "x11window.h"

#include <netwm.h>
#include <xcb/xcb_icccm.h>
#endif

Q_DECLARE_METATYPE(KWin::QuickTileMode)
Q_DECLARE_METATYPE(KWin::MaximizeMode)

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_quick_tiling-0");

#if KWIN_BUILD_X11
static X11Window *createWindow(xcb_connection_t *connection, const Rect &geometry)
{
    xcb_window_t windowId = xcb_generate_id(connection);
    xcb_create_window(connection, XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      geometry.x(),
                      geometry.y(),
                      geometry.width(),
                      geometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);

    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, geometry.x(), geometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, geometry.width(), geometry.height());
    xcb_icccm_set_wm_normal_hints(connection, windowId, &hints);

    xcb_map_window(connection, windowId);
    xcb_flush(connection);

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    if (!windowCreatedSpy.wait()) {
        return nullptr;
    }
    return windowCreatedSpy.last().first().value<X11Window *>();
}
#endif

class QuickTilingTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testQuickTiling_data();
    void testQuickTiling();
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
    void testMultiScreen();
    void testMultiScreenX11();
    void testQuickTileAndMaximize();
    void testQuickTileAndMaximizeX11();
    void testQuickTileAndFullScreen();
    void testQuickTileAndFullScreenX11();
    void testPerDesktop();
    void testPerDesktopX11();
    void testMoveBetweenQuickTileAndCustomTileSameDesktop();
    void testMoveBetweenQuickTileAndCustomTileSameDesktopX11();
    void testMoveBetweenQuickTileAndCustomTileCrossDesktops();
    void testMoveBetweenQuickTileAndCustomTileCrossDesktopsX11();
    void testEvacuateFromRemovedDesktop();
    void testEvacuateFromRemovedDesktopX11();
    void testCloseTiledWindow();
    void testCloseTiledWindowX11();
    void testScript_data();
    void testScript();
    void testDontCrashWithMaximizeWindowRule();
};

void QuickTilingTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::MaximizeMode>("MaximizeMode");
    QVERIFY(waylandServer()->init(s_socketName));

    // set custom config which disables the Outline
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup group = config->group(QStringLiteral("Outline"));
    group.writeEntry(QStringLiteral("QmlPath"), QString("/does/not/exist.qml"));
    group.sync();

    kwinApp()->setConfig(config);

    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void QuickTilingTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::XdgDecorationV1));

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
    VirtualDesktopManager::self()->setCount(2);
    VirtualDesktopManager::self()->setCurrent(1);
}

void QuickTilingTest::cleanup()
{
    Test::destroyWaylandConnection();

    // discard window rules
    workspace()->rulebook()->load();
}

void QuickTilingTest::testQuickTiling_data()
{
    QTest::addColumn<QuickTileMode>("mode");
    QTest::addColumn<RectF>("expectedGeometry");
    QTest::addColumn<RectF>("secondScreen");
    QTest::addColumn<QuickTileMode>("expectedModeAfterToggle");

#define FLAG(name) QuickTileMode(QuickTileFlag::name)

    QTest::newRow("left") << FLAG(Left) << RectF(0, 0, 640, 1024) << RectF(1280, 0, 640, 1024) << FLAG(Right);
    QTest::newRow("top") << FLAG(Top) << RectF(0, 0, 1280, 512) << RectF(1280, 0, 1280, 512) << FLAG(Top);
    QTest::newRow("right") << FLAG(Right) << RectF(640, 0, 640, 1024) << RectF(1920, 0, 640, 1024) << FLAG(Right);
    QTest::newRow("bottom") << FLAG(Bottom) << RectF(0, 512, 1280, 512) << RectF(1280, 512, 1280, 512) << FLAG(Bottom);

    QTest::newRow("top left") << (FLAG(Left) | FLAG(Top)) << RectF(0, 0, 640, 512) << RectF(1280, 0, 640, 512) << (FLAG(Right) | FLAG(Top));
    QTest::newRow("top right") << (FLAG(Right) | FLAG(Top)) << RectF(640, 0, 640, 512) << RectF(1920, 0, 640, 512) << (FLAG(Right) | FLAG(Top));
    QTest::newRow("bottom left") << (FLAG(Left) | FLAG(Bottom)) << RectF(0, 512, 640, 512) << RectF(1280, 512, 640, 512) << (FLAG(Right) | FLAG(Bottom));
    QTest::newRow("bottom right") << (FLAG(Right) | FLAG(Bottom)) << RectF(640, 512, 640, 512) << RectF(1920, 512, 640, 512) << (FLAG(Right) | FLAG(Bottom));
#undef FLAG
}

void QuickTilingTest::testQuickTiling()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);

    // Map the window.
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 100, 50));
    QCOMPARE(window->quickTileMode(), QuickTileMode(QuickTileFlag::None));

    // We have to receive a configure event when the window becomes active.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    QSignalSpy quickTileChangedSpy(window, &Window::quickTileModeChanged);
    QSignalSpy tileChangedSpy(window, &Window::tileChanged);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);

    QFETCH(QuickTileMode, mode);
    QFETCH(QuickTileMode, expectedModeAfterToggle);
    QFETCH(RectF, expectedGeometry);
    const QuickTileMode oldQuickTileMode = window->quickTileMode();
    window->handleQuickTileShortcut(mode);

    // at this point the geometry did not yet change
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 100, 50));
    // Manually maximized window is always without a tile
    QCOMPARE(window->requestedQuickTileMode(), mode);
    // Actual quickTileMOde didn't change yet
    QCOMPARE(window->quickTileMode(), oldQuickTileMode);

    // but we got requested a new geometry
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), expectedGeometry.size());

    // attach a new image
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), expectedGeometry.size().toSize(), Qt::red);

    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(frameGeometryChangedSpy.count(), 1);
    QCOMPARE(window->frameGeometry(), expectedGeometry);
    QCOMPARE(quickTileChangedSpy.count(), 1);
    QCOMPARE(window->quickTileMode(), mode);

    // send window to other screen
    QList<LogicalOutput *> outputs = workspace()->outputs();
    QCOMPARE(window->output(), outputs[0]);

    window->sendToOutput(outputs[1]);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), expectedGeometry.size());
    // attach a new image
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), expectedGeometry.size().toSize(), Qt::red);
    QVERIFY(tileChangedSpy.wait());
    QCOMPARE(window->output(), outputs[1]);
    // quick tile should not be changed
    QCOMPARE(window->quickTileMode(), mode);
    QTEST(window->frameGeometry(), "secondScreen");
    Tile *tile = workspace()->tileManager(outputs[1])->quickTile(mode);
    QCOMPARE(window->tile(), tile);

    // now try to toggle again
    window->handleQuickTileShortcut(mode);
    QCOMPARE(window->requestedQuickTileMode(), expectedModeAfterToggle);
    if (expectedModeAfterToggle != mode) {
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        QCOMPARE(surfaceConfigureRequestedSpy.count(), 4);
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
        Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::red);
        QVERIFY(quickTileChangedSpy.wait());
        QCOMPARE(quickTileChangedSpy.count(), 2);
    }
    QCOMPARE(window->quickTileMode(), expectedModeAfterToggle);
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
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    // let's render
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 100, 50));
    QCOMPARE(window->quickTileMode(), QuickTileMode(QuickTileFlag::None));
    QCOMPARE(window->maximizeMode(), MaximizeRestore);

    // We have to receive a configure event when the window becomes active.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    QSignalSpy quickTileChangedSpy(window, &Window::quickTileModeChanged);

    workspace()->performWindowOperation(window, Options::UnrestrictedMoveOp);
    QCOMPARE(window, workspace()->moveResizeWindow());
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(50, 25));

    QFETCH(QPoint, targetPos);
    quint32 timestamp = 1;
    Test::keyboardKeyPressed(KEY_LEFTCTRL, timestamp++);
    while (Cursors::self()->mouse()->pos().x() > targetPos.x()) {
        Test::keyboardKeyPressed(KEY_LEFT, timestamp++);
        Test::keyboardKeyReleased(KEY_LEFT, timestamp++);
    }
    while (Cursors::self()->mouse()->pos().x() < targetPos.x()) {
        Test::keyboardKeyPressed(KEY_RIGHT, timestamp++);
        Test::keyboardKeyReleased(KEY_RIGHT, timestamp++);
    }
    while (Cursors::self()->mouse()->pos().y() < targetPos.y()) {
        Test::keyboardKeyPressed(KEY_DOWN, timestamp++);
        Test::keyboardKeyReleased(KEY_DOWN, timestamp++);
    }
    while (Cursors::self()->mouse()->pos().y() > targetPos.y()) {
        Test::keyboardKeyPressed(KEY_UP, timestamp++);
        Test::keyboardKeyReleased(KEY_UP, timestamp++);
    }
    Test::keyboardKeyReleased(KEY_LEFTCTRL, timestamp++);
    Test::keyboardKeyPressed(KEY_ENTER, timestamp++);
    Test::keyboardKeyReleased(KEY_ENTER, timestamp++);
    QCOMPARE(Cursors::self()->mouse()->pos(), targetPos);
    QVERIFY(!workspace()->moveResizeWindow());

    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::red);
    QVERIFY(quickTileChangedSpy.wait());
    QCOMPARE(quickTileChangedSpy.count(), 1);
    QTEST(window->quickTileMode(), "expectedMode");
}

void QuickTilingTest::testQuickTilingPointerMove_data()
{
    QTest::addColumn<QPoint>("pointerPos");
    QTest::addColumn<QSize>("tileSize");
    QTest::addColumn<QuickTileMode>("expectedMode");

    QTest::newRow("topRight") << QPoint(2559, 24) << QSize(640, 512) << QuickTileMode(QuickTileFlag::Top | QuickTileFlag::Right);
    QTest::newRow("right") << QPoint(2559, 512) << QSize(640, 1024) << QuickTileMode(QuickTileFlag::Right);
    QTest::newRow("bottomRight") << QPoint(2559, 1023) << QSize(640, 512) << QuickTileMode(QuickTileFlag::Bottom | QuickTileFlag::Right);
    QTest::newRow("bottomLeft") << QPoint(0, 1023) << QSize(640, 512) << QuickTileMode(QuickTileFlag::Bottom | QuickTileFlag::Left);
    QTest::newRow("Left") << QPoint(0, 512) << QSize(640, 1024) << QuickTileMode(QuickTileFlag::Left);
    QTest::newRow("topLeft") << QPoint(0, 24) << QSize(640, 512) << QuickTileMode(QuickTileFlag::Top | QuickTileFlag::Left);
}

void QuickTilingTest::testQuickTilingPointerMove()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));

    // let's render
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 100, 50));
    QCOMPARE(window->quickTileMode(), QuickTileMode(QuickTileFlag::None));
    QCOMPARE(window->maximizeMode(), MaximizeRestore);

    // we have to receive a configure event when the window becomes active
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    // verify that basic quick tile mode works as expected, i.e. the window is going to be
    // tiled if the user drags it to a screen edge or a corner
    QSignalSpy quickTileChangedSpy(window, &Window::quickTileModeChanged);
    workspace()->performWindowOperation(window, Options::UnrestrictedMoveOp);
    QCOMPARE(window, workspace()->moveResizeWindow());
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(50, 25));

    QFETCH(QPoint, pointerPos);
    QFETCH(QSize, tileSize);
    quint32 timestamp = 1;
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    Test::pointerMotion(pointerPos, timestamp++);
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QTEST(window->requestedQuickTileMode(), "expectedMode");
    const QPoint tileOutputPositon = workspace()->outputAt(pointerPos)->geometry().topLeft();
    QCOMPARE(window->geometryRestore(), RectF(tileOutputPositon, QSize(100, 50)));
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), tileSize);

    // attach a new image
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), tileSize, Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry().size(), tileSize);
    QCOMPARE(quickTileChangedSpy.count(), 1);
    QTEST(window->quickTileMode(), "expectedMode");

    // verify that geometry restore is correct after user untiles the window, but changes
    // their mind and tiles the window again while still holding left button
    workspace()->performWindowOperation(window, Options::UnrestrictedMoveOp);
    QCOMPARE(window, workspace()->moveResizeWindow());

    Test::pointerButtonPressed(BTN_LEFT, timestamp++); // untile the window
    Test::pointerMotion(QPoint(1280, 1024) / 2, timestamp++);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileMode(QuickTileFlag::None));
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(100, 50));

    // attach a new image
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(100, 50), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry().size(), QSize(100, 50));
    QCOMPARE(quickTileChangedSpy.count(), 2);
    QCOMPARE(window->quickTileMode(), QuickTileMode(QuickTileFlag::None));

    Test::pointerMotion(pointerPos, timestamp++); // tile the window again
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QTEST(window->requestedQuickTileMode(), "expectedMode");
    QCOMPARE(window->geometryRestore(), RectF(tileOutputPositon, QSize(100, 50)));
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 4);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), tileSize);

    // attach a new image
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(100, 50), Qt::red);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry().size(), QSize(100, 50));
    QCOMPARE(quickTileChangedSpy.count(), 3);
    QTEST(window->quickTileMode(), "expectedMode");
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
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    std::unique_ptr<Test::XdgToplevelDecorationV1> deco(Test::createXdgToplevelDecorationV1(shellSurface.get()));

    QSignalSpy decorationConfigureRequestedSpy(deco.get(), &Test::XdgToplevelDecorationV1::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);

    // wait for the initial configure event
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    // let's render
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(1000, 50), Qt::blue);

    QVERIFY(window);
    QVERIFY(window->isDecorated());
    const auto decoration = window->decoration();
    QCOMPARE(workspace()->activeWindow(), window);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 1000 + decoration->borderLeft() + decoration->borderRight(), 50 + decoration->borderTop() + decoration->borderBottom()));
    QCOMPARE(window->quickTileMode(), QuickTileMode(QuickTileFlag::None));
    QCOMPARE(window->maximizeMode(), MaximizeRestore);

    // we have to receive a configure event when the window becomes active
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QTRY_COMPARE(surfaceConfigureRequestedSpy.count(), 2);

    QSignalSpy quickTileChangedSpy(window, &Window::quickTileModeChanged);

    // Note that interactive move will be started with a delay.
    quint32 timestamp = 1;
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    Test::touchDown(0, QPointF(window->frameGeometry().center().x(), window->frameGeometry().y() + decoration->borderTop() / 2), timestamp++);
    QVERIFY(interactiveMoveResizeStartedSpy.wait());
    QCOMPARE(window, workspace()->moveResizeWindow());

    QFETCH(QPoint, targetPos);
    Test::touchMotion(0, targetPos, timestamp++);
    Test::touchUp(0, timestamp++);
    QVERIFY(!workspace()->moveResizeWindow());

    // When there are no borders, there is no change to them when quick-tiling.
    // TODO: we should test both cases with fixed fake decoration for autotests.
    const bool hasBorders = Workspace::self()->decorationBridge()->settings()->borderSize() != KDecoration3::BorderSize::None;

    QTEST(window->requestedQuickTileMode(), "expectedMode");
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QTRY_COMPARE(surfaceConfigureRequestedSpy.count(), hasBorders ? 4 : 3);
    QCOMPARE(false, toplevelConfigureRequestedSpy.last().first().toSize().isEmpty());

    // attach a new image
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::red);
    QVERIFY(quickTileChangedSpy.wait());
    QCOMPARE(quickTileChangedSpy.count(), 1);
    QTEST(window->quickTileMode(), "expectedMode");
}

void QuickTilingTest::testX11QuickTiling_data()
{
    QTest::addColumn<QuickTileMode>("mode");
    QTest::addColumn<RectF>("expectedGeometry");
    QTest::addColumn<int>("screenId");
    QTest::addColumn<QuickTileMode>("modeAfterToggle");

#define FLAG(name) QuickTileMode(QuickTileFlag::name)

    QTest::newRow("left") << FLAG(Left) << RectF(0, 0, 640, 1024) << 0 << FLAG(Left);
    QTest::newRow("top") << FLAG(Top) << RectF(0, 0, 1280, 512) << 0 << FLAG(Top);
    QTest::newRow("right") << FLAG(Right) << RectF(640, 0, 640, 1024) << 1 << FLAG(Left);
    QTest::newRow("bottom") << FLAG(Bottom) << RectF(0, 512, 1280, 512) << 0 << FLAG(Bottom);

    QTest::newRow("top left") << (FLAG(Left) | FLAG(Top)) << RectF(0, 0, 640, 512) << 0 << (FLAG(Left) | FLAG(Top));
    QTest::newRow("top right") << (FLAG(Right) | FLAG(Top)) << RectF(640, 0, 640, 512) << 1 << (FLAG(Left) | FLAG(Top));
    QTest::newRow("bottom left") << (FLAG(Left) | FLAG(Bottom)) << RectF(0, 512, 640, 512) << 0 << (FLAG(Left) | FLAG(Bottom));
    QTest::newRow("bottom right") << (FLAG(Right) | FLAG(Bottom)) << RectF(640, 512, 640, 512) << 1 << (FLAG(Left) | FLAG(Bottom));

#undef FLAG
}
void QuickTilingTest::testX11QuickTiling()
{
#if KWIN_BUILD_X11
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    const Rect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints{};
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);

    // now quick tile
    QSignalSpy quickTileChangedSpy(window, &Window::quickTileModeChanged);
    const RectF origGeo = window->frameGeometry();
    QFETCH(QuickTileMode, mode);
    window->handleQuickTileShortcut(mode);
    QCOMPARE(quickTileChangedSpy.count(), 1);
    QCOMPARE(window->quickTileMode(), mode);
    QTEST(window->frameGeometry(), "expectedGeometry");
    QCOMPARE(window->geometryRestore(), origGeo);

    // quick tile to same edge again should also act like send to screen
    // if screen is on the same edge
    const auto outputs = workspace()->outputs();
    QCOMPARE(window->output(), outputs[0]);
    window->handleQuickTileShortcut(mode);
    QFETCH(int, screenId);
    QCOMPARE(window->output(), outputs[screenId]);
    QTEST(window->quickTileMode(), "modeAfterToggle");
    QCOMPARE(window->geometryRestore(), origGeo);

    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    c.reset();

    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    QVERIFY(windowClosedSpy.wait());
#endif
}

void QuickTilingTest::testX11QuickTilingAfterVertMaximize_data()
{
    QTest::addColumn<QuickTileMode>("mode");
    QTest::addColumn<RectF>("expectedGeometry");

#define FLAG(name) QuickTileMode(QuickTileFlag::name)

    QTest::newRow("left") << FLAG(Left) << RectF(0, 0, 640, 1024);
    QTest::newRow("top") << FLAG(Top) << RectF(0, 0, 1280, 512);
    QTest::newRow("right") << FLAG(Right) << RectF(640, 0, 640, 1024);
    QTest::newRow("bottom") << FLAG(Bottom) << RectF(0, 512, 1280, 512);

    QTest::newRow("top left") << (FLAG(Left) | FLAG(Top)) << RectF(0, 0, 640, 512);
    QTest::newRow("top right") << (FLAG(Right) | FLAG(Top)) << RectF(640, 0, 640, 512);
    QTest::newRow("bottom left") << (FLAG(Left) | FLAG(Bottom)) << RectF(0, 512, 640, 512);
    QTest::newRow("bottom right") << (FLAG(Right) | FLAG(Bottom)) << RectF(640, 512, 640, 512);

#undef FLAG
}

void QuickTilingTest::testX11QuickTilingAfterVertMaximize()
{
#if KWIN_BUILD_X11
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    const Rect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints{};
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);

    const RectF origGeo = window->frameGeometry();
    QCOMPARE(window->maximizeMode(), MaximizeRestore);
    // vertically maximize the window
    window->maximize(window->maximizeMode() ^ MaximizeVertical);
    QCOMPARE(window->frameGeometry().width(), origGeo.width());
    QCOMPARE(window->height(), window->output()->geometry().height());
    QCOMPARE(window->geometryRestore(), origGeo);

    // now quick tile
    QSignalSpy quickTileChangedSpy(window, &Window::quickTileModeChanged);
    QFETCH(QuickTileMode, mode);
    window->setQuickTileModeAtCurrentPosition(mode);
    QCOMPARE(window->quickTileMode(), mode);
    QCOMPARE(quickTileChangedSpy.count(), 1);
    QTEST(window->frameGeometry(), "expectedGeometry");

    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    c.reset();

    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    QVERIFY(windowClosedSpy.wait());
#endif
}

void QuickTilingTest::testShortcut_data()
{
    const auto N = QuickTileMode(QuickTileFlag::None);
    const auto L = QuickTileMode(QuickTileFlag::Left);
    const auto R = QuickTileMode(QuickTileFlag::Right);
    const auto T = QuickTileMode(QuickTileFlag::Top);
    const auto B = QuickTileMode(QuickTileFlag::Bottom);
    const auto TL = T | L;
    const auto TR = T | R;
    const auto BL = B | L;
    const auto BR = B | R;

    // The transition table for quick tile mode:
    // _            mode1       mode2       mode3...
    // oldMode1     newMode1    newMode2    newMode3...
    // oldMode2     newMode1    newMode2    newMode3...
    const QuickTileMode quickTileTransition[][9] = {
        {N, L, R, T, B, TL, TR, BL, BR}, // transition from N
        {L, L, R, TL, BL, TL, TR, BL, BR}, // transition from L
        {R, L, R, TR, BR, TL, TR, BL, BR},
        {T, TL, TR, T, B, TL, TR, BL, BR},
        {B, BL, BR, T, B, TL, TR, BL, BR},
        {TL, TL, T, TL, L, TL, TR, BL, BR},
        {TR, T, TR, TR, R, TL, TR, BL, BR},
        {BL, BL, B, L, BL, TL, TR, BL, BR},
        {BR, B, BR, R, BR, TL, TR, BL, BR},
    };

    const QHash<QuickTileMode, RectF> geometries = {
        {N, RectF()},
        {L, RectF(0, 0, 640, 1024)},
        {R, RectF(640, 0, 640, 1024)},
        {T, RectF(0, 0, 1280, 512)},
        {B, RectF(0, 512, 1280, 512)},
        {TL, RectF(0, 0, 640, 512)},
        {TR, RectF(640, 0, 640, 512)},
        {BL, RectF(0, 512, 640, 512)},
        {BR, RectF(640, 512, 640, 512)},
    };

    const QHash<QuickTileMode, QString> shortcuts = {
        {L, QStringLiteral("Window Quick Tile Left")},
        {R, QStringLiteral("Window Quick Tile Right")},
        {T, QStringLiteral("Window Quick Tile Top")},
        {B, QStringLiteral("Window Quick Tile Bottom")},
        {TL, QStringLiteral("Window Quick Tile Top Left")},
        {TR, QStringLiteral("Window Quick Tile Top Right")},
        {BL, QStringLiteral("Window Quick Tile Bottom Left")},
        {BR, QStringLiteral("Window Quick Tile Bottom Right")},
    };

    const QHash<QuickTileMode, QString> names = {
        {N, QStringLiteral("None")},
        {L, QStringLiteral("Left")},
        {R, QStringLiteral("Right")},
        {T, QStringLiteral("Top")},
        {B, QStringLiteral("Bottom")},
        {TL, QStringLiteral("TopLeft")},
        {TR, QStringLiteral("TopRight")},
        {BL, QStringLiteral("BottomLeft")},
        {BR, QStringLiteral("BottomRight")},
    };

    QTest::addColumn<QuickTileMode>("oldMode");
    QTest::addColumn<QString>("shortcut");
    QTest::addColumn<QuickTileMode>("expectedMode");
    QTest::addColumn<RectF>("expectedGeometry");

    for (size_t row = 0; row < sizeof(quickTileTransition) / sizeof(quickTileTransition[0]); ++row) {
        for (size_t col = 1; col < sizeof(quickTileTransition[0]) / sizeof(quickTileTransition[0][0]); ++col) {
            const auto oldMode = quickTileTransition[row][0];
            auto newMode = quickTileTransition[row][col];
            const auto action = quickTileTransition[0][col];
            const auto shortcut = shortcuts[action];
            auto geometry = geometries[newMode];

            // We have another screen to the right, so when pressing right on the right edge, it goes to left
            // edge of the next screen.
            if (oldMode == newMode) {
                if (action & QuickTileFlag::Right) {
                    newMode.setFlag(QuickTileFlag::Right, false);
                    newMode.setFlag(QuickTileFlag::Left, true);
                    geometry.moveLeft(1280);
                }
            }

            QTest::newRow(QStringLiteral("%1 -> %2 = %3").arg(names[oldMode]).arg(shortcut).arg(names[newMode]).toLatin1().constData())
                << oldMode << shortcut << newMode << geometry;
        }
    }
}

void QuickTilingTest::testShortcut()
{
#if !KWIN_BUILD_GLOBALSHORTCUTS
    QSKIP("Can't test shortcuts without shortcuts");
    return;
#endif

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);

    // Map the window.
    const auto initialGeometry = Rect(0, 0, 100, 50);
    auto window = Test::renderAndWaitForShown(surface.get(), initialGeometry.size(), Qt::blue);
    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);
    QCOMPARE(window->frameGeometry(), initialGeometry);
    QCOMPARE(window->quickTileMode(), QuickTileMode(QuickTileFlag::None));

    // We have to receive a configure event when the window becomes active.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    QFETCH(QuickTileMode, oldMode);
    QFETCH(QString, shortcut);
    QFETCH(QuickTileMode, expectedMode);
    QFETCH(RectF, expectedGeometry);

    if (expectedMode == QuickTileMode(QuickTileFlag::None)) {
        expectedGeometry = initialGeometry;
    }

    QSignalSpy quickTileChangedSpy(window, &Window::quickTileModeChanged);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);

    int numberOfQuickTileActions = 1;

    if (oldMode != QuickTileMode(QuickTileFlag::None)) {
        window->handleQuickTileShortcut(oldMode);
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
        Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::red);
        QVERIFY(quickTileChangedSpy.wait());
        ++numberOfQuickTileActions;
    }

    // invoke global shortcut through dbus
    auto msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.kglobalaccel"),
        QStringLiteral("/component/kwin"),
        QStringLiteral("org.kde.kglobalaccel.Component"),
        QStringLiteral("invokeShortcut"));
    msg.setArguments(QList<QVariant>{shortcut});
    QDBusConnection::sessionBus().asyncCall(msg);

    if (oldMode == expectedMode) {
        QVERIFY(!surfaceConfigureRequestedSpy.wait(10));
    } else {
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
        Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::red);
        QVERIFY(quickTileChangedSpy.wait());

        QCOMPARE(surfaceConfigureRequestedSpy.count(), numberOfQuickTileActions + 1);
        QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), expectedGeometry.size());
        QCOMPARE(frameGeometryChangedSpy.count(), numberOfQuickTileActions);
        QTRY_COMPARE(quickTileChangedSpy.count(), numberOfQuickTileActions);
    }

    // geometry already changed
    QCOMPARE(window->frameGeometry(), expectedGeometry);
    // quick tile mode already changed
    QCOMPARE(window->quickTileMode(), expectedMode);

    QEXPECT_FAIL("maximize", "Geometry changed called twice for maximize", Continue);
    QCOMPARE(window->frameGeometry(), expectedGeometry);
}

void QuickTilingTest::testMultiScreen()
{
    // This test verifies that a window can be moved between screens by continuously pressing Meta+arrow.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::blue);

    // We have to receive a configure event when the window becomes active.
    QSignalSpy tileChangedSpy(window, &Window::tileChanged);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    TileManager *firstTileManager = workspace()->tileManager(workspace()->outputs().at(0));
    TileManager *secondTileManager = workspace()->tileManager(workspace()->outputs().at(1));

    const struct
    {
        QuickTileMode shortcut;
        QuickTileMode previous;
        Tile *previousTile;
        QuickTileMode next;
        Tile *nextTile;
        RectF geometry;
    } steps[] = {
        // Not tiled -> tiled on the left half of the first screen
        {
            .shortcut = QuickTileFlag::Left,
            .previous = QuickTileFlag::None,
            .previousTile = nullptr,
            .next = QuickTileFlag::Left,
            .nextTile = firstTileManager->quickTile(QuickTileFlag::Left),
            .geometry = RectF(0, 0, 640, 1024),
        },
        // Tiled on the left half of the first screen -> tiled on the right half of the first screen
        {
            .shortcut = QuickTileFlag::Right,
            .previous = QuickTileFlag::Left,
            .previousTile = firstTileManager->quickTile(QuickTileFlag::Left),
            .next = QuickTileFlag::Right,
            .nextTile = firstTileManager->quickTile(QuickTileFlag::Right),
            .geometry = RectF(640, 0, 640, 1024),
        },
        // Tiled on the right half of the first screen -> tiled on the left half of the second screen
        {
            .shortcut = QuickTileFlag::Right,
            .previous = QuickTileFlag::Right,
            .previousTile = firstTileManager->quickTile(QuickTileFlag::Right),
            .next = QuickTileFlag::Left,
            .nextTile = secondTileManager->quickTile(QuickTileFlag::Left),
            .geometry = RectF(1280, 0, 640, 1024),
        },
        // Tiled on the left half of the second screen -> tiled on the right half of the second screen
        {
            .shortcut = QuickTileFlag::Right,
            .previous = QuickTileFlag::Left,
            .previousTile = secondTileManager->quickTile(QuickTileFlag::Left),
            .next = QuickTileFlag::Right,
            .nextTile = secondTileManager->quickTile(QuickTileFlag::Right),
            .geometry = RectF(1920, 0, 640, 1024),
        },
        // Tiled on the right half of the second screen -> tiled on the left half of the second screen
        {
            .shortcut = QuickTileFlag::Left,
            .previous = QuickTileFlag::Right,
            .previousTile = secondTileManager->quickTile(QuickTileFlag::Right),
            .next = QuickTileFlag::Left,
            .nextTile = secondTileManager->quickTile(QuickTileFlag::Left),
            .geometry = RectF(1280, 0, 640, 1024),
        },
        // Tiled on the left half of the second screen -> tiled on the right half of the first screen
        {
            .shortcut = QuickTileFlag::Left,
            .previous = QuickTileFlag::Left,
            .previousTile = secondTileManager->quickTile(QuickTileFlag::Left),
            .next = QuickTileFlag::Right,
            .nextTile = firstTileManager->quickTile(QuickTileFlag::Right),
            .geometry = RectF(640, 0, 640, 1024),
        },
        // Tiled on the right half of the first screen -> tiled on the left half of the first screen
        {
            .shortcut = QuickTileFlag::Left,
            .previous = QuickTileFlag::Right,
            .previousTile = firstTileManager->quickTile(QuickTileFlag::Right),
            .next = QuickTileFlag::Left,
            .nextTile = firstTileManager->quickTile(QuickTileFlag::Left),
            .geometry = RectF(0, 0, 640, 1024),
        },
    };

    for (const auto &step : steps) {
        window->handleQuickTileShortcut(step.shortcut);

        QCOMPARE(window->quickTileMode(), step.previous);
        QCOMPARE(window->requestedQuickTileMode(), step.next);

        QCOMPARE(window->tile(), step.previousTile);
        QVERIFY(!step.previousTile || !step.previousTile->windows().contains(window));
        QCOMPARE(window->requestedTile(), step.nextTile);
        QVERIFY(step.nextTile->windows().contains(window));

        QCOMPARE(window->moveResizeGeometry(), step.geometry);
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
        Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::blue);
        QVERIFY(tileChangedSpy.wait());
        QCOMPARE(window->quickTileMode(), step.next);
        QCOMPARE(window->requestedQuickTileMode(), step.next);
        QCOMPARE(window->tile(), step.nextTile);
        QCOMPARE(window->requestedTile(), step.nextTile);
        QCOMPARE(window->frameGeometry(), step.geometry);
        QCOMPARE(window->moveResizeGeometry(), step.geometry);
    }
}

void QuickTilingTest::testMultiScreenX11()
{
#if KWIN_BUILD_X11
    // This test verifies that an X11 window can be moved between screens by continuously pressing Meta+arrow.

    Test::XcbConnectionPtr connection = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(connection.get()));
    X11Window *window = createWindow(connection.get(), Rect(0, 0, 100, 200));

    TileManager *firstTileManager = workspace()->tileManager(workspace()->outputs().at(0));
    TileManager *secondTileManager = workspace()->tileManager(workspace()->outputs().at(1));

    const struct
    {
        QuickTileMode shortcut;
        QuickTileMode previous;
        Tile *previousTile;
        QuickTileMode next;
        Tile *nextTile;
        RectF geometry;
    } steps[] = {
        // Not tiled -> tiled on the left half of the first screen
        {
            .shortcut = QuickTileFlag::Left,
            .previous = QuickTileFlag::None,
            .previousTile = nullptr,
            .next = QuickTileFlag::Left,
            .nextTile = firstTileManager->quickTile(QuickTileFlag::Left),
            .geometry = RectF(0, 0, 640, 1024),
        },
        // Tiled on the left half of the first screen -> tiled on the right half of the first screen
        {
            .shortcut = QuickTileFlag::Right,
            .previous = QuickTileFlag::Left,
            .previousTile = firstTileManager->quickTile(QuickTileFlag::Left),
            .next = QuickTileFlag::Right,
            .nextTile = firstTileManager->quickTile(QuickTileFlag::Right),
            .geometry = RectF(640, 0, 640, 1024),
        },
        // Tiled on the right half of the first screen -> tiled on the left half of the second screen
        {
            .shortcut = QuickTileFlag::Right,
            .previous = QuickTileFlag::Right,
            .previousTile = firstTileManager->quickTile(QuickTileFlag::Right),
            .next = QuickTileFlag::Left,
            .nextTile = secondTileManager->quickTile(QuickTileFlag::Left),
            .geometry = RectF(1280, 0, 640, 1024),
        },
        // Tiled on the left half of the second screen -> tiled on the right half of the second screen
        {
            .shortcut = QuickTileFlag::Right,
            .previous = QuickTileFlag::Left,
            .previousTile = secondTileManager->quickTile(QuickTileFlag::Left),
            .next = QuickTileFlag::Right,
            .nextTile = secondTileManager->quickTile(QuickTileFlag::Right),
            .geometry = RectF(1920, 0, 640, 1024),
        },
        // Tiled on the right half of the second screen -> tiled on the left half of the second screen
        {
            .shortcut = QuickTileFlag::Left,
            .previous = QuickTileFlag::Right,
            .previousTile = secondTileManager->quickTile(QuickTileFlag::Right),
            .next = QuickTileFlag::Left,
            .nextTile = secondTileManager->quickTile(QuickTileFlag::Left),
            .geometry = RectF(1280, 0, 640, 1024),
        },
        // Tiled on the left half of the second screen -> tiled on the right half of the first screen
        {
            .shortcut = QuickTileFlag::Left,
            .previous = QuickTileFlag::Left,
            .previousTile = secondTileManager->quickTile(QuickTileFlag::Left),
            .next = QuickTileFlag::Right,
            .nextTile = firstTileManager->quickTile(QuickTileFlag::Right),
            .geometry = RectF(640, 0, 640, 1024),
        },
        // Tiled on the right half of the first screen -> tiled on the left half of the first screen
        {
            .shortcut = QuickTileFlag::Left,
            .previous = QuickTileFlag::Right,
            .previousTile = firstTileManager->quickTile(QuickTileFlag::Right),
            .next = QuickTileFlag::Left,
            .nextTile = firstTileManager->quickTile(QuickTileFlag::Left),
            .geometry = RectF(0, 0, 640, 1024),
        },
    };

    for (const auto &step : steps) {
        QCOMPARE(window->quickTileMode(), step.previous);
        QCOMPARE(window->requestedQuickTileMode(), step.previous);
        QCOMPARE(window->tile(), step.previousTile);
        QCOMPARE(window->requestedTile(), step.previousTile);

        window->handleQuickTileShortcut(step.shortcut);

        QVERIFY(!step.previousTile || !step.previousTile->windows().contains(window));
        QVERIFY(step.nextTile->windows().contains(window));

        QCOMPARE(window->moveResizeGeometry(), step.geometry);
        QCOMPARE(window->quickTileMode(), step.next);
        QCOMPARE(window->requestedQuickTileMode(), step.next);
        QCOMPARE(window->tile(), step.nextTile);
        QCOMPARE(window->requestedTile(), step.nextTile);
        QCOMPARE(window->frameGeometry(), step.geometry);
        QCOMPARE(window->moveResizeGeometry(), step.geometry);
    }
#endif
}

void QuickTilingTest::testQuickTileAndMaximize()
{
    // This test verifies that quick tile and maximize mode are mutually exclusive.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::blue);

    // We have to receive a configure event when the window becomes active.
    QSignalSpy tileChangedSpy(window, &Window::tileChanged);
    QSignalSpy maximizedChanged(window, &Window::maximizedChanged);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    QuickTileMode previousQuickTileMode = QuickTileFlag::None;
    MaximizeMode previousMaximizeMode = MaximizeRestore;

    auto quickTile = [&]() {
        window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Right);
        QCOMPARE(window->geometryRestore(), RectF(0, 0, 100, 100));
        QCOMPARE(window->quickTileMode(), previousQuickTileMode);
        QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Right);
        QCOMPARE(window->maximizeMode(), previousMaximizeMode);
        QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
        Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::blue);
        QVERIFY(tileChangedSpy.wait());
        QCOMPARE(window->quickTileMode(), QuickTileFlag::Right);
        QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Right);
        QCOMPARE(window->maximizeMode(), MaximizeRestore);
        QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
        QCOMPARE(window->frameGeometry(), RectF(640, 0, 640, 1024));
        QCOMPARE(window->moveResizeGeometry(), RectF(640, 0, 640, 1024));

        previousMaximizeMode = window->maximizeMode();
        previousQuickTileMode = window->quickTileMode();
    };

    auto maximize = [&]() {
        window->maximize(MaximizeFull);
        QCOMPARE(window->geometryRestore(), RectF(0, 0, 100, 100));
        QCOMPARE(window->quickTileMode(), previousQuickTileMode);
        QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
        QCOMPARE(window->maximizeMode(), previousMaximizeMode);
        QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
        Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::blue);
        QVERIFY(maximizedChanged.wait());
        QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
        QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
        QCOMPARE(window->maximizeMode(), MaximizeFull);
        QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
        QCOMPARE(window->frameGeometry(), RectF(0, 0, 1280, 1024));
        QCOMPARE(window->moveResizeGeometry(), RectF(0, 0, 1280, 1024));

        previousMaximizeMode = window->maximizeMode();
        previousQuickTileMode = window->quickTileMode();
    };

    auto restore = [&]() {
        window->maximize(MaximizeRestore);
        QCOMPARE(window->geometryRestore(), RectF(0, 0, 100, 100));
        QCOMPARE(window->quickTileMode(), previousQuickTileMode);
        QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
        QCOMPARE(window->maximizeMode(), previousMaximizeMode);
        QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
        Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::blue);
        QVERIFY(maximizedChanged.wait());
        QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
        QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
        QCOMPARE(window->maximizeMode(), MaximizeRestore);
        QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
        QCOMPARE(window->frameGeometry(), RectF(0, 0, 100, 100));
        QCOMPARE(window->moveResizeGeometry(), RectF(0, 0, 100, 100));

        previousMaximizeMode = window->maximizeMode();
        previousQuickTileMode = window->quickTileMode();
    };

    quickTile();
    maximize();
    restore();

    quickTile();
    maximize();
    restore();

    quickTile();
    maximize();
    quickTile();
    maximize();
}

void QuickTilingTest::testQuickTileAndMaximizeX11()
{
#if KWIN_BUILD_X11
    // This test verifies that quick tile and maximize mode are mutually exclusive.

    Test::XcbConnectionPtr connection = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(connection.get()));
    X11Window *window = createWindow(connection.get(), Rect(0, 0, 100, 200));

    QuickTileMode previousQuickTileMode = QuickTileFlag::None;
    MaximizeMode previousMaximizeMode = MaximizeRestore;
    RectF originalGeometry = window->frameGeometry();

    auto quickTile = [&]() {
        QCOMPARE(window->geometryRestore(), originalGeometry);
        QCOMPARE(window->quickTileMode(), previousQuickTileMode);
        QCOMPARE(window->requestedQuickTileMode(), previousQuickTileMode);
        QCOMPARE(window->maximizeMode(), previousMaximizeMode);
        QCOMPARE(window->requestedMaximizeMode(), previousMaximizeMode);

        window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Right);

        QCOMPARE(window->quickTileMode(), QuickTileFlag::Right);
        QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Right);
        QCOMPARE(window->maximizeMode(), MaximizeRestore);
        QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
        QCOMPARE(window->frameGeometry(), RectF(640, 0, 640, 1024));
        QCOMPARE(window->moveResizeGeometry(), RectF(640, 0, 640, 1024));

        previousMaximizeMode = window->maximizeMode();
        previousQuickTileMode = window->quickTileMode();
    };

    auto maximize = [&]() {
        QCOMPARE(window->geometryRestore(), originalGeometry);
        QCOMPARE(window->quickTileMode(), previousQuickTileMode);
        QCOMPARE(window->requestedQuickTileMode(), previousQuickTileMode);
        QCOMPARE(window->maximizeMode(), previousMaximizeMode);
        QCOMPARE(window->requestedMaximizeMode(), previousMaximizeMode);

        window->maximize(MaximizeFull);

        QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
        QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
        QCOMPARE(window->maximizeMode(), MaximizeFull);
        QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
        QCOMPARE(window->frameGeometry(), RectF(0, 0, 1280, 1024));
        QCOMPARE(window->moveResizeGeometry(), RectF(0, 0, 1280, 1024));

        previousMaximizeMode = window->maximizeMode();
        previousQuickTileMode = window->quickTileMode();
    };

    auto restore = [&]() {
        QCOMPARE(window->geometryRestore(), originalGeometry);
        QCOMPARE(window->quickTileMode(), previousQuickTileMode);
        QCOMPARE(window->requestedQuickTileMode(), previousQuickTileMode);
        QCOMPARE(window->maximizeMode(), previousMaximizeMode);
        QCOMPARE(window->requestedMaximizeMode(), previousMaximizeMode);

        window->maximize(MaximizeRestore);

        QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
        QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
        QCOMPARE(window->maximizeMode(), MaximizeRestore);
        QCOMPARE(window->requestedMaximizeMode(), MaximizeRestore);
        QCOMPARE(window->frameGeometry(), originalGeometry);
        QCOMPARE(window->moveResizeGeometry(), originalGeometry);

        previousMaximizeMode = window->maximizeMode();
        previousQuickTileMode = window->quickTileMode();
    };

    quickTile();
    maximize();
    restore();

    quickTile();
    maximize();
    restore();

    quickTile();
    maximize();
    quickTile();
    maximize();
#endif
}

void QuickTilingTest::testQuickTileAndFullScreen()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::blue);

    // We have to receive a configure event when the window becomes active.
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    auto ackConfigure = [&]() {
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
        Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::blue);
        QVERIFY(frameGeometryChangedSpy.wait());
    };

    // tile the window in the left half of the screen on the first virtual desktop
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Left);
    QCOMPARE(window->geometryRestore(), RectF(0, 0, 100, 100));
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    ackConfigure();
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 640, 1024));

    // make the window fullscreen
    window->setFullScreen(true);
    QCOMPARE(window->fullscreenGeometryRestore(), RectF(0, 0, 640, 1024));
    QCOMPARE(window->isFullScreen(), false);
    QCOMPARE(window->isRequestedFullScreen(), true);
    QCOMPARE(window->geometryRestore(), RectF(0, 0, 100, 100));
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    ackConfigure();
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->isRequestedFullScreen(), true);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 1280, 1024));

    // leave fullscreen mode
    window->setFullScreen(false);
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->isRequestedFullScreen(), false);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    ackConfigure();
    QCOMPARE(window->isFullScreen(), false);
    QCOMPARE(window->isRequestedFullScreen(), false);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 640, 1024));

    // untile the window
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::None);
    QCOMPARE(window->isFullScreen(), false);
    QCOMPARE(window->isRequestedFullScreen(), false);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    ackConfigure();
    QCOMPARE(window->isFullScreen(), false);
    QCOMPARE(window->isRequestedFullScreen(), false);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 100, 100));

    // make the window fullscreen
    window->setFullScreen(true);
    QCOMPARE(window->fullscreenGeometryRestore(), RectF(0, 0, 100, 100));
    QCOMPARE(window->isFullScreen(), false);
    QCOMPARE(window->isRequestedFullScreen(), true);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    ackConfigure();
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->isRequestedFullScreen(), true);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 1280, 1024));

    // attempt to tile the window
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Left);
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->isRequestedFullScreen(), true);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 1280, 1024));
}

void QuickTilingTest::testQuickTileAndFullScreenX11()
{
#if KWIN_BUILD_X11
    Test::XcbConnectionPtr connection = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(connection.get()));
    X11Window *window = createWindow(connection.get(), Rect(0, 0, 100, 200));

    const RectF originalGeometry = window->frameGeometry();

    // tile the window in the left half of the screen on the first virtual desktop
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Left);
    QCOMPARE(window->geometryRestore(), originalGeometry);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 640, 1024));

    // make the window fullscreen
    window->setFullScreen(true);
    QCOMPARE(window->fullscreenGeometryRestore(), RectF(0, 0, 640, 1024));
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->isRequestedFullScreen(), true);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 1280, 1024));

    // leave fullscreen mode
    window->setFullScreen(false);
    QCOMPARE(window->isFullScreen(), false);
    QCOMPARE(window->isRequestedFullScreen(), false);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 640, 1024));

    // untile the window
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::None);
    QCOMPARE(window->isFullScreen(), false);
    QCOMPARE(window->isRequestedFullScreen(), false);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->frameGeometry(), originalGeometry);

    // make the window fullscreen
    window->setFullScreen(true);
    QCOMPARE(window->fullscreenGeometryRestore(), originalGeometry);
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->isRequestedFullScreen(), true);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 1280, 1024));

    // attempt to tile the window
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Left);
    QCOMPARE(window->isFullScreen(), true);
    QCOMPARE(window->isRequestedFullScreen(), true);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 1280, 1024));
#endif
}

void QuickTilingTest::testPerDesktop()
{
    // This test verifies that a window can be tiled differently depending on the virtual desktop.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::blue);

    // We have to receive a configure event when the window becomes active.
    QSignalSpy tileChangedSpy(window, &Window::tileChanged);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    auto ackConfigure = [&]() {
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
        Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::blue);
        QVERIFY(tileChangedSpy.wait());
    };

    // tile the window in the left half of the screen on the first virtual desktop
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Left);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    ackConfigure();
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 640, 1024));

    // switch to the second virtual desktop, the window will still remain tiled, although invisible
    VirtualDesktopManager::self()->setCurrent(2);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);

    // nothing will happen if the window is untiled on the second virtual desktop
    VirtualDesktopManager::self()->setCurrent(2);
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::None);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);

    // tile the window in the right half of the screen on the second virtual desktop
    window->setOnAllDesktops(true);
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Right);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Right);
    ackConfigure();
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->frameGeometry(), RectF(640, 0, 640, 1024));

    // when we return back to the first virtual desktop, the window will be tiled in the left half of the screen
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    ackConfigure();
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 640, 1024));

    // and if we go back to the second virtual desktop, the window will be tiled in the right half of the screen
    VirtualDesktopManager::self()->setCurrent(2);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Right);
    ackConfigure();
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->frameGeometry(), RectF(640, 0, 640, 1024));

    // untile the window on the second virtual desktop
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::None);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    ackConfigure();
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 100, 100));

    // go back to the first virtual desktop, the window will be tiled
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    ackConfigure();
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 640, 1024));

    // go to the second virtual desktop, the window will be untiled
    VirtualDesktopManager::self()->setCurrent(2);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    ackConfigure();
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 100, 100));
}

void QuickTilingTest::testPerDesktopX11()
{
#if KWIN_BUILD_X11
    // This test verifies that an X11 window can be tiled differently depending on the virtual desktop.

    Test::XcbConnectionPtr connection = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(connection.get()));
    X11Window *window = createWindow(connection.get(), Rect(0, 0, 100, 200));

    QSignalSpy tileChangedSpy(window, &Window::tileChanged);
    const RectF originalGeometry = window->frameGeometry();

    // tile the window in the left half of the screen on the first virtual desktop
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Left);
    QCOMPARE(tileChangedSpy.count(), 1);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 640, 1024));

    // switch to the second virtual desktop, the window will still remain tiled, although invisible
    VirtualDesktopManager::self()->setCurrent(2);
    QCOMPARE(tileChangedSpy.count(), 1);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(tileChangedSpy.count(), 1);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);

    // nothing will happen if the window is untiled on the second virtual desktop
    VirtualDesktopManager::self()->setCurrent(2);
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::None);
    QCOMPARE(tileChangedSpy.count(), 1);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);

    // tile the window in the right half of the screen on the second virtual desktop
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    window->setOnAllDesktops(true);
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Right);
    QCOMPARE(tileChangedSpy.count(), 2);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->frameGeometry(), RectF(640, 0, 640, 1024));

    // when we return back to the first virtual desktop, the window will be tiled in the left half of the screen
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(tileChangedSpy.count(), 3);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 640, 1024));

    // and if we go back to the second virtual desktop, the window will be tiled in the right half of the screen
    VirtualDesktopManager::self()->setCurrent(2);
    QCOMPARE(tileChangedSpy.count(), 4);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->frameGeometry(), RectF(640, 0, 640, 1024));

    // untile the window on the second virtual desktop
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::None);
    QCOMPARE(tileChangedSpy.count(), 5);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->frameGeometry(), originalGeometry);

    // go back to the first virtual desktop, the window will be tiled
    VirtualDesktopManager::self()->setCurrent(1);
    QCOMPARE(tileChangedSpy.count(), 6);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Left);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 640, 1024));

    // go to the second virtual desktop, the window will be untiled
    VirtualDesktopManager::self()->setCurrent(2);
    QCOMPARE(tileChangedSpy.count(), 7);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->frameGeometry(), originalGeometry);
#endif
}

void QuickTilingTest::testMoveBetweenQuickTileAndCustomTileSameDesktop()
{
    // This test checks that a window can be moved between quick tiles and custom tiles on the same virtual desktop.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::blue);

    // We have to receive a configure event when the window becomes active.
    QSignalSpy tileChangedSpy(window, &Window::tileChanged);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    auto ackConfigure = [&]() {
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
        Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::blue);
        QVERIFY(tileChangedSpy.wait());
    };

    const RectF originalGeometry = window->frameGeometry();
    const auto outputs = workspace()->outputs();
    for (LogicalOutput *first : outputs) {
        for (LogicalOutput *second : outputs) {
            const QPointF customPoint = first->geometry().center();
            const QPointF quickPoint = second->geometry().center();
            Tile *customTile = workspace()->rootTile(first)->pick(customPoint);
            Tile *quickTile = workspace()->tileManager(second)->quickTile(QuickTileFlag::Left);

            window->setQuickTileMode(QuickTileFlag::Left, quickPoint);
            QCOMPARE(window->tile(), nullptr);
            QVERIFY(!customTile->windows().contains(window));
            QCOMPARE(window->requestedTile(), quickTile);
            QVERIFY(quickTile->windows().contains(window));
            ackConfigure();
            QCOMPARE(window->tile(), quickTile);
            QCOMPARE(window->requestedTile(), quickTile);
            QCOMPARE(window->frameGeometry(), quickTile->windowGeometry());

            window->setQuickTileMode(QuickTileFlag::Custom, customPoint);
            QCOMPARE(window->tile(), quickTile);
            QVERIFY(!quickTile->windows().contains(window));
            QCOMPARE(window->requestedTile(), customTile);
            QVERIFY(customTile->windows().contains(window));
            ackConfigure();
            QCOMPARE(window->tile(), customTile);
            QCOMPARE(window->requestedTile(), customTile);
            QCOMPARE(window->frameGeometry(), customTile->windowGeometry());

            window->setQuickTileMode(QuickTileFlag::Left, quickPoint);
            QCOMPARE(window->tile(), customTile);
            QVERIFY(!customTile->windows().contains(window));
            QCOMPARE(window->requestedTile(), quickTile);
            QVERIFY(quickTile->windows().contains(window));
            ackConfigure();
            QCOMPARE(window->tile(), quickTile);
            QCOMPARE(window->requestedTile(), quickTile);
            QCOMPARE(window->frameGeometry(), quickTile->windowGeometry());

            window->setQuickTileMode(QuickTileFlag::Custom, customPoint);
            QCOMPARE(window->tile(), quickTile);
            QVERIFY(!quickTile->windows().contains(window));
            QCOMPARE(window->requestedTile(), customTile);
            QVERIFY(customTile->windows().contains(window));
            ackConfigure();
            QCOMPARE(window->tile(), customTile);
            QCOMPARE(window->requestedTile(), customTile);
            QCOMPARE(window->frameGeometry(), customTile->windowGeometry());

            window->setQuickTileModeAtCurrentPosition(QuickTileFlag::None);
            QCOMPARE(window->tile(), customTile);
            QVERIFY(!customTile->windows().contains(window));
            QCOMPARE(window->requestedTile(), nullptr);
            QVERIFY(!quickTile->windows().contains(window));
            ackConfigure();
            QCOMPARE(window->tile(), nullptr);
            QCOMPARE(window->requestedTile(), nullptr);
            QCOMPARE(window->frameGeometry(), originalGeometry);
        }
    }
}

void QuickTilingTest::testMoveBetweenQuickTileAndCustomTileSameDesktopX11()
{
#if KWIN_BUILD_X11
    // This test checks that an X11 window can be moved between quick tiles and custom tiles on the same virtual desktop.

    Test::XcbConnectionPtr connection = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(connection.get()));
    X11Window *window = createWindow(connection.get(), Rect(0, 0, 100, 200));

    QSignalSpy tileChangedSpy(window, &Window::tileChanged);
    const RectF originalGeometry = window->frameGeometry();

    const auto outputs = workspace()->outputs();
    for (LogicalOutput *first : outputs) {
        for (LogicalOutput *second : outputs) {
            const QPointF customPoint = first->geometry().center();
            const QPointF quickPoint = second->geometry().center();
            Tile *customTile = workspace()->rootTile(first)->pick(customPoint);
            Tile *quickTile = workspace()->tileManager(second)->quickTile(QuickTileFlag::Left);

            {
                QCOMPARE(window->tile(), nullptr);
                QCOMPARE(window->requestedTile(), nullptr);
                QVERIFY(!customTile->windows().contains(window));
                QVERIFY(!quickTile->windows().contains(window));

                window->setQuickTileMode(QuickTileFlag::Left, quickPoint);

                QCOMPARE(window->tile(), quickTile);
                QCOMPARE(window->requestedTile(), quickTile);
                QVERIFY(!customTile->windows().contains(window));
                QVERIFY(quickTile->windows().contains(window));
                QCOMPARE(window->frameGeometry(), quickTile->windowGeometry());
            }

            {
                QCOMPARE(window->tile(), quickTile);
                QCOMPARE(window->requestedTile(), quickTile);
                QVERIFY(!customTile->windows().contains(window));
                QVERIFY(quickTile->windows().contains(window));

                window->setQuickTileMode(QuickTileFlag::Custom, customPoint);

                QCOMPARE(window->tile(), customTile);
                QCOMPARE(window->requestedTile(), customTile);
                QVERIFY(customTile->windows().contains(window));
                QVERIFY(!quickTile->windows().contains(window));
                QCOMPARE(window->frameGeometry(), customTile->windowGeometry());
            }

            {
                QCOMPARE(window->tile(), customTile);
                QCOMPARE(window->requestedTile(), customTile);
                QVERIFY(customTile->windows().contains(window));
                QVERIFY(!quickTile->windows().contains(window));

                window->setQuickTileMode(QuickTileFlag::Left, quickPoint);

                QCOMPARE(window->tile(), quickTile);
                QCOMPARE(window->requestedTile(), quickTile);
                QVERIFY(!customTile->windows().contains(window));
                QVERIFY(quickTile->windows().contains(window));
                QCOMPARE(window->frameGeometry(), quickTile->windowGeometry());
            }

            {
                QCOMPARE(window->tile(), quickTile);
                QCOMPARE(window->requestedTile(), quickTile);
                QVERIFY(!customTile->windows().contains(window));
                QVERIFY(quickTile->windows().contains(window));

                window->setQuickTileMode(QuickTileFlag::Custom, customPoint);

                QCOMPARE(window->tile(), customTile);
                QCOMPARE(window->requestedTile(), customTile);
                QVERIFY(customTile->windows().contains(window));
                QVERIFY(!quickTile->windows().contains(window));
                QCOMPARE(window->frameGeometry(), customTile->windowGeometry());
            }

            {
                QCOMPARE(window->tile(), customTile);
                QCOMPARE(window->requestedTile(), customTile);
                QVERIFY(customTile->windows().contains(window));
                QVERIFY(!quickTile->windows().contains(window));

                window->setQuickTileModeAtCurrentPosition(QuickTileFlag::None);

                QCOMPARE(window->tile(), nullptr);
                QCOMPARE(window->requestedTile(), nullptr);
                QVERIFY(!customTile->windows().contains(window));
                QVERIFY(!quickTile->windows().contains(window));
                QCOMPARE(window->frameGeometry(), originalGeometry);
            }
        }
    }
#endif
}

void QuickTilingTest::testMoveBetweenQuickTileAndCustomTileCrossDesktops()
{
    auto vds = VirtualDesktopManager::self();
    const auto desktops = vds->desktops();
    const auto outputs = workspace()->outputs();

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::blue);
    window->setOnAllDesktops(true);

    // We have to receive a configure event when the window becomes active.
    QSignalSpy tileChangedSpy(window, &Window::tileChanged);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    auto ackConfigure = [&]() {
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
        Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::blue);
        QVERIFY(tileChangedSpy.wait());
    };

    auto applyTileLayout = [](CustomTile *tile, qreal left, qreal right) {
        const auto previousKiddos = tile->childTiles();
        for (Tile *kiddo : previousKiddos) {
            tile->destroyChild(kiddo);
        }

        tile->split(Tile::LayoutDirection::Horizontal);
        tile->childTiles().at(0)->setRelativeGeometry(RectF(0, 0, left, 1.0));

        QCOMPARE(tile->childTiles().at(0)->relativeGeometry(), RectF(0, 0, left, 1));
        QCOMPARE(tile->childTiles().at(1)->relativeGeometry(), RectF(left, 0, right, 1));
    };
    applyTileLayout(workspace()->rootTile(outputs.at(0), desktops.at(0)), 0.4, 0.6);
    applyTileLayout(workspace()->rootTile(outputs.at(0), desktops.at(1)), 0.35, 0.65);
    applyTileLayout(workspace()->rootTile(outputs.at(1), desktops.at(0)), 0.3, 0.7);
    applyTileLayout(workspace()->rootTile(outputs.at(1), desktops.at(1)), 0.25, 0.75);

    const RectF originalGeometry = window->frameGeometry();
    for (VirtualDesktop *customTileDesktop : desktops) {
        for (VirtualDesktop *quickTileDesktop : desktops) {
            if (customTileDesktop == quickTileDesktop) {
                continue;
            }

            for (LogicalOutput *customTileOutput : outputs) {
                for (LogicalOutput *quickTileOutput : outputs) {
                    Tile *quickTile = workspace()->tileManager(quickTileOutput)->quickRootTile(quickTileDesktop)->tileForMode(QuickTileFlag::Left);
                    Tile *customTile = workspace()->rootTile(customTileOutput, customTileDesktop)->childTile(1);

                    // put the window in a custom tile on the first virtual desktop
                    vds->setCurrent(customTileDesktop);
                    customTile->manage(window);
                    QCOMPARE(window->tile(), nullptr);
                    QCOMPARE(window->requestedTile(), customTile);
                    QCOMPARE(window->frameGeometry(), originalGeometry);
                    ackConfigure();
                    QCOMPARE(window->tile(), customTile);
                    QCOMPARE(window->requestedTile(), customTile);
                    QCOMPARE(window->frameGeometry(), customTile->windowGeometry());

                    // switch to the second virtual desktop, the window will be untiled
                    vds->setCurrent(quickTileDesktop);
                    QCOMPARE(window->tile(), customTile);
                    QCOMPARE(window->requestedTile(), nullptr);
                    QCOMPARE(window->frameGeometry(), customTile->windowGeometry());
                    ackConfigure();
                    QCOMPARE(window->tile(), nullptr);
                    QCOMPARE(window->requestedTile(), nullptr);
                    QCOMPARE(window->frameGeometry(), originalGeometry);

                    // put the window in a quick tile on the second virtual desktop
                    quickTile->manage(window);
                    QCOMPARE(window->tile(), nullptr);
                    QCOMPARE(window->requestedTile(), quickTile);
                    QCOMPARE(window->frameGeometry(), originalGeometry);
                    ackConfigure();
                    QCOMPARE(window->tile(), quickTile);
                    QCOMPARE(window->requestedTile(), quickTile);
                    QCOMPARE(window->frameGeometry(), quickTile->windowGeometry());

                    // switch to the first virtual desktop
                    vds->setCurrent(customTileDesktop);
                    QCOMPARE(window->tile(), quickTile);
                    QCOMPARE(window->requestedTile(), customTile);
                    QCOMPARE(window->frameGeometry(), quickTile->windowGeometry());
                    ackConfigure();
                    QCOMPARE(window->tile(), customTile);
                    QCOMPARE(window->requestedTile(), customTile);
                    QCOMPARE(window->frameGeometry(), customTile->windowGeometry());

                    // switch to the second virtual desktop
                    vds->setCurrent(quickTileDesktop);
                    QCOMPARE(window->tile(), customTile);
                    QCOMPARE(window->requestedTile(), quickTile);
                    QCOMPARE(window->frameGeometry(), customTile->windowGeometry());
                    ackConfigure();
                    QCOMPARE(window->tile(), quickTile);
                    QCOMPARE(window->requestedTile(), quickTile);
                    QCOMPARE(window->frameGeometry(), quickTile->windowGeometry());

                    // remove the window from the quick tile on the second virtual desktop
                    quickTile->unmanage(window);
                    QCOMPARE(window->tile(), quickTile);
                    QCOMPARE(window->requestedTile(), nullptr);
                    QCOMPARE(window->frameGeometry(), quickTile->windowGeometry());
                    ackConfigure();
                    QCOMPARE(window->tile(), nullptr);
                    QCOMPARE(window->requestedTile(), nullptr);
                    QCOMPARE(window->frameGeometry(), originalGeometry);

                    // switch to the first virtual desktop
                    vds->setCurrent(customTileDesktop);
                    QCOMPARE(window->tile(), nullptr);
                    QCOMPARE(window->requestedTile(), customTile);
                    QCOMPARE(window->frameGeometry(), originalGeometry);
                    ackConfigure();
                    QCOMPARE(window->tile(), customTile);
                    QCOMPARE(window->requestedTile(), customTile);
                    QCOMPARE(window->frameGeometry(), customTile->windowGeometry());

                    // remove the window from the custom tile on the first virtual desktop
                    customTile->unmanage(window);
                    QCOMPARE(window->tile(), customTile);
                    QCOMPARE(window->requestedTile(), nullptr);
                    QCOMPARE(window->frameGeometry(), customTile->windowGeometry());
                    ackConfigure();
                    QCOMPARE(window->tile(), nullptr);
                    QCOMPARE(window->requestedTile(), nullptr);
                    QCOMPARE(window->frameGeometry(), originalGeometry);
                }
            }
        }
    }
}

void QuickTilingTest::testMoveBetweenQuickTileAndCustomTileCrossDesktopsX11()
{
#if KWIN_BUILD_X11
    auto vds = VirtualDesktopManager::self();
    const auto desktops = vds->desktops();
    const auto outputs = workspace()->outputs();

    Test::XcbConnectionPtr connection = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(connection.get()));
    X11Window *window = createWindow(connection.get(), Rect(0, 0, 100, 200));
    window->setOnAllDesktops(true);

    auto applyTileLayout = [](CustomTile *tile, qreal left, qreal right) {
        const auto previousKiddos = tile->childTiles();
        for (Tile *kiddo : previousKiddos) {
            tile->destroyChild(kiddo);
        }

        tile->split(Tile::LayoutDirection::Horizontal);
        tile->childTiles().at(0)->setRelativeGeometry(RectF(0, 0, left, 1.0));

        QCOMPARE(tile->childTiles().at(0)->relativeGeometry(), RectF(0, 0, left, 1));
        QCOMPARE(tile->childTiles().at(1)->relativeGeometry(), RectF(left, 0, right, 1));
    };
    applyTileLayout(workspace()->rootTile(outputs.at(0), desktops.at(0)), 0.4, 0.6);
    applyTileLayout(workspace()->rootTile(outputs.at(0), desktops.at(1)), 0.35, 0.65);
    applyTileLayout(workspace()->rootTile(outputs.at(1), desktops.at(0)), 0.3, 0.7);
    applyTileLayout(workspace()->rootTile(outputs.at(1), desktops.at(1)), 0.25, 0.75);

    const RectF originalGeometry = window->frameGeometry();
    for (VirtualDesktop *customTileDesktop : desktops) {
        for (VirtualDesktop *quickTileDesktop : desktops) {
            if (customTileDesktop == quickTileDesktop) {
                continue;
            }

            for (LogicalOutput *customTileOutput : outputs) {
                for (LogicalOutput *quickTileOutput : outputs) {
                    Tile *quickTile = workspace()->tileManager(quickTileOutput)->quickRootTile(quickTileDesktop)->tileForMode(QuickTileFlag::Left);
                    Tile *customTile = workspace()->rootTile(customTileOutput, customTileDesktop)->childTile(1);

                    // put the window in a custom tile on the first virtual desktop
                    {
                        vds->setCurrent(customTileDesktop);
                        QCOMPARE(window->tile(), nullptr);
                        QCOMPARE(window->requestedTile(), nullptr);
                        QCOMPARE(window->frameGeometry(), originalGeometry);

                        customTile->manage(window);

                        QCOMPARE(window->tile(), customTile);
                        QCOMPARE(window->requestedTile(), customTile);
                        QCOMPARE(window->frameGeometry(), customTile->windowGeometry());
                    }

                    // switch to the second virtual desktop, the window will be untiled
                    {
                        QCOMPARE(window->tile(), customTile);
                        QCOMPARE(window->requestedTile(), customTile);
                        QCOMPARE(window->frameGeometry(), customTile->windowGeometry());

                        vds->setCurrent(quickTileDesktop);

                        QCOMPARE(window->tile(), nullptr);
                        QCOMPARE(window->requestedTile(), nullptr);
                        QCOMPARE(window->frameGeometry(), originalGeometry);
                    }

                    // put the window in a quick tile on the second virtual desktop
                    {
                        QCOMPARE(window->tile(), nullptr);
                        QCOMPARE(window->requestedTile(), nullptr);
                        QCOMPARE(window->frameGeometry(), originalGeometry);

                        quickTile->manage(window);

                        QCOMPARE(window->tile(), quickTile);
                        QCOMPARE(window->requestedTile(), quickTile);
                        QCOMPARE(window->frameGeometry(), quickTile->windowGeometry());
                    }

                    // switch to the first virtual desktop
                    {
                        QCOMPARE(window->tile(), quickTile);
                        QCOMPARE(window->requestedTile(), quickTile);
                        QCOMPARE(window->frameGeometry(), quickTile->windowGeometry());

                        vds->setCurrent(customTileDesktop);

                        QCOMPARE(window->tile(), customTile);
                        QCOMPARE(window->requestedTile(), customTile);
                        QCOMPARE(window->frameGeometry(), customTile->windowGeometry());
                    }

                    // switch to the second virtual desktop
                    {
                        QCOMPARE(window->tile(), customTile);
                        QCOMPARE(window->requestedTile(), customTile);
                        QCOMPARE(window->frameGeometry(), customTile->windowGeometry());

                        vds->setCurrent(quickTileDesktop);

                        QCOMPARE(window->tile(), quickTile);
                        QCOMPARE(window->requestedTile(), quickTile);
                        QCOMPARE(window->frameGeometry(), quickTile->windowGeometry());
                    }

                    // remove the window from the quick tile on the second virtual desktop
                    {
                        QCOMPARE(window->tile(), quickTile);
                        QCOMPARE(window->requestedTile(), quickTile);
                        QCOMPARE(window->frameGeometry(), quickTile->windowGeometry());

                        quickTile->unmanage(window);

                        QCOMPARE(window->tile(), nullptr);
                        QCOMPARE(window->requestedTile(), nullptr);
                        QCOMPARE(window->frameGeometry(), originalGeometry);
                    }

                    // switch to the first virtual desktop
                    {
                        QCOMPARE(window->tile(), nullptr);
                        QCOMPARE(window->requestedTile(), nullptr);
                        QCOMPARE(window->frameGeometry(), originalGeometry);

                        vds->setCurrent(customTileDesktop);

                        QCOMPARE(window->tile(), customTile);
                        QCOMPARE(window->requestedTile(), customTile);
                        QCOMPARE(window->frameGeometry(), customTile->windowGeometry());
                    }

                    // remove the window from the custom tile on the first virtual desktop
                    {
                        QCOMPARE(window->tile(), customTile);
                        QCOMPARE(window->requestedTile(), customTile);
                        QCOMPARE(window->frameGeometry(), customTile->windowGeometry());

                        customTile->unmanage(window);

                        QCOMPARE(window->tile(), nullptr);
                        QCOMPARE(window->requestedTile(), nullptr);
                        QCOMPARE(window->frameGeometry(), originalGeometry);
                    }
                }
            }
        }
    }
#endif
}

void QuickTilingTest::testEvacuateFromRemovedDesktop()
{
    // This test verifies that a window is properly evacuated from a removed virtual desktop.

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::blue);

    // We have to receive a configure event when the window becomes active.
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    auto ackConfigure = [&]() {
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
        Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::blue);
        QVERIFY(frameGeometryChangedSpy.wait());
    };

    const RectF originalGeometry = window->frameGeometry();

    // tile the window in the right half of the screen
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Right);
    QCOMPARE(window->geometryRestore(), originalGeometry);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Right);
    ackConfigure();
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->frameGeometry(), RectF(640, 0, 640, 1024));

    // remove the current virtual desktop
    VirtualDesktopManager::self()->removeVirtualDesktop(VirtualDesktopManager::self()->currentDesktop());
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None); // technically, it should be "Right" but the tile object is gone
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    ackConfigure();
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->frameGeometry(), originalGeometry);
}

void QuickTilingTest::testEvacuateFromRemovedDesktopX11()
{
#if KWIN_BUILD_X11
    // This test verifies that an X11 window is properly evacuated from a removed virtual desktop.

    Test::XcbConnectionPtr connection = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(connection.get()));
    X11Window *window = createWindow(connection.get(), Rect(0, 0, 100, 200));

    const RectF originalGeometry = window->frameGeometry();

    // tile the window in the right half of the screen
    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Right);
    QCOMPARE(window->quickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::Right);
    QCOMPARE(window->frameGeometry(), RectF(640, 0, 640, 1024));

    // remove the current virtual desktop
    VirtualDesktopManager::self()->removeVirtualDesktop(VirtualDesktopManager::self()->currentDesktop());
    QCOMPARE(window->quickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileFlag::None);
    QCOMPARE(window->frameGeometry(), originalGeometry);
#endif
}

void QuickTilingTest::testCloseTiledWindow()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::blue);

    // We have to receive a configure event when the window becomes active.
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    auto ackConfigure = [&]() {
        QVERIFY(surfaceConfigureRequestedSpy.wait());
        shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
        Test::render(surface.get(), toplevelConfigureRequestedSpy.last().at(0).toSize(), Qt::blue);
        QVERIFY(frameGeometryChangedSpy.wait());
    };

    Tile *tile = workspace()->tileManager(workspace()->activeOutput())->quickTile(QuickTileFlag::Right);

    const RectF originalGeometry = window->frameGeometry();
    tile->manage(window);
    QCOMPARE(window->geometryRestore(), originalGeometry);
    QCOMPARE(window->tile(), nullptr);
    QCOMPARE(window->requestedTile(), tile);
    ackConfigure();
    QCOMPARE(window->tile(), tile);
    QCOMPARE(window->requestedTile(), tile);
    QCOMPARE(window->frameGeometry(), tile->windowGeometry());

    window->ref();
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
    QVERIFY(!tile->windows().contains(window));
    QCOMPARE(window->tile(), tile);
    QCOMPARE(window->requestedTile(), tile);
    QCOMPARE(window->frameGeometry(), tile->windowGeometry());
    window->unref();
}

void QuickTilingTest::testCloseTiledWindowX11()
{
#if KWIN_BUILD_X11
    Test::XcbConnectionPtr connection = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(connection.get()));
    X11Window *window = createWindow(connection.get(), Rect(0, 0, 100, 200));

    Tile *tile = workspace()->tileManager(workspace()->activeOutput())->quickTile(QuickTileFlag::Right);

    tile->manage(window);
    QCOMPARE(window->tile(), tile);
    QCOMPARE(window->requestedTile(), tile);
    QCOMPARE(window->frameGeometry(), tile->windowGeometry());

    window->ref();
    connection.reset();
    QVERIFY(Test::waitForWindowClosed(window));
    QVERIFY(!tile->windows().contains(window));
    QCOMPARE(window->tile(), tile);
    QCOMPARE(window->requestedTile(), tile);
    QCOMPARE(window->frameGeometry(), tile->windowGeometry());
    window->unref();
#endif
}

void QuickTilingTest::testScript_data()
{
    QTest::addColumn<QString>("action");
    QTest::addColumn<QuickTileMode>("expectedMode");
    QTest::addColumn<RectF>("expectedGeometry");

#define FLAG(name) QuickTileMode(QuickTileFlag::name)
    QTest::newRow("top") << QStringLiteral("Top") << FLAG(Top) << RectF(0, 0, 1280, 512);
    QTest::newRow("bottom") << QStringLiteral("Bottom") << FLAG(Bottom) << RectF(0, 512, 1280, 512);
    QTest::newRow("top right") << QStringLiteral("TopRight") << (FLAG(Top) | FLAG(Right)) << RectF(640, 0, 640, 512);
    QTest::newRow("top left") << QStringLiteral("TopLeft") << (FLAG(Top) | FLAG(Left)) << RectF(0, 0, 640, 512);
    QTest::newRow("bottom right") << QStringLiteral("BottomRight") << (FLAG(Bottom) | FLAG(Right)) << RectF(640, 512, 640, 512);
    QTest::newRow("bottom left") << QStringLiteral("BottomLeft") << (FLAG(Bottom) | FLAG(Left)) << RectF(0, 512, 640, 512);
    QTest::newRow("left") << QStringLiteral("Left") << FLAG(Left) << RectF(0, 0, 640, 1024);
    QTest::newRow("right") << QStringLiteral("Right") << FLAG(Right) << RectF(640, 0, 640, 1024);
#undef FLAG
}

void QuickTilingTest::testScript()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);

    // Map the window.
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 100, 50));
    QCOMPARE(window->quickTileMode(), QuickTileMode(QuickTileFlag::None));

    // We have to receive a configure event upon the window becoming active.
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);

    QSignalSpy quickTileChangedSpy(window, &Window::quickTileModeChanged);
    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);

    QVERIFY(Scripting::self());
    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    QTextStream out(&tmpFile);

    QFETCH(QString, action);
    out << "workspace.slotWindowQuickTile" << action << "()";
    out.flush();

    QFETCH(QuickTileMode, expectedMode);
    QFETCH(RectF, expectedGeometry);

    const int id = Scripting::self()->loadScript(tmpFile.fileName());
    QVERIFY(id != -1);
    QVERIFY(Scripting::self()->isScriptLoaded(tmpFile.fileName()));
    auto s = Scripting::self()->findScript(tmpFile.fileName());
    QVERIFY(s);
    QSignalSpy runningChangedSpy(s, &AbstractScript::runningChanged);
    s->run();

    QVERIFY(runningChangedSpy.wait());
    QCOMPARE(runningChangedSpy.count(), 1);
    QCOMPARE(runningChangedSpy.first().first().toBool(), true);

    // at this point the geometry did not yet change
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 100, 50));
    // but requested quick tile mode already changed
    QCOMPARE(window->requestedQuickTileMode(), expectedMode);

    // but we got requested a new geometry
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), expectedGeometry.size());

    // attach a new image
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), expectedGeometry.size().toSize(), Qt::red);

    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(quickTileChangedSpy.count(), 1);
    QCOMPARE(window->quickTileMode(), expectedMode);
    QEXPECT_FAIL("maximize", "Geometry changed called twice for maximize", Continue);
    QCOMPARE(frameGeometryChangedSpy.count(), 1);
    QCOMPARE(window->frameGeometry(), expectedGeometry);
}

void QuickTilingTest::testDontCrashWithMaximizeWindowRule()
{
    // this test verifies that a force-maximize window rule doesn't cause
    // setQuickTileMode to loop forever

    workspace()->rulebook()->setConfig(KSharedConfig::openConfig(QFINDTESTDATA("./data/rules/force-maximize"), KConfig::SimpleConfig));
    workspace()->slotReconfigure();

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(1280, 800), Qt::blue);
    QVERIFY(window);

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(workspace()->activeWindow(), window);
    QCOMPARE(window->frameGeometry(), RectF(0, 0, 1280, 800));
    QCOMPARE(window->requestedQuickTileMode(), QuickTileMode(QuickTileFlag::None));
    QCOMPARE(window->requestedMaximizeMode(), MaximizeMode::MaximizeFull);

    window->setQuickTileModeAtCurrentPosition(QuickTileFlag::Right);
    QCOMPARE(window->requestedQuickTileMode(), QuickTileMode(QuickTileFlag::None));
    QCOMPARE(window->requestedMaximizeMode(), MaximizeMode::MaximizeFull);
}
}

WAYLANDTEST_MAIN(KWin::QuickTilingTest)
#include "quick_tiling_test.moc"
