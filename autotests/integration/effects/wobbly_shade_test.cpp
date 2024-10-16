/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "cursor.h"
#include "effect/effecthandler.h"
#include "effect/effectloader.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

#include <KConfigGroup>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/slide.h>
#include <KWayland/Client/surface.h>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

using namespace KWin;
static const QString s_socketName = QStringLiteral("wayland_test_effects_wobbly_shade-0");

class WobblyWindowsShadeTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testShadeMove();
};

void WobblyWindowsShadeTest::initTestCase()
{
    if (!Test::renderNodeAvailable()) {
        QSKIP("no render node available");
        return;
    }
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::Effect *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    // disable all effects - we don't want to have it interact with the rendering
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    const auto builtinNames = EffectLoader().listOfKnownEffects();
    for (QString name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }

    config->sync();
    kwinApp()->setConfig(config);

    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));
    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", "1");
    kwinApp()->start();
}

void WobblyWindowsShadeTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void WobblyWindowsShadeTest::cleanup()
{
    Test::destroyWaylandConnection();

    effects->unloadAllEffects();
    QVERIFY(effects->loadedEffects().isEmpty());
}

void WobblyWindowsShadeTest::testShadeMove()
{
    // this test simulates the condition from BUG 390953
    QVERIFY(effects->loadEffect(QStringLiteral("wobblywindows")));
    QVERIFY(effects->isEffectLoaded(QStringLiteral("wobblywindows")));

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
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
    QVERIFY(window->isDecorated());
    QVERIFY(window->isShadeable());
    QVERIFY(!window->isShade());
    QVERIFY(window->isActive());

    QSignalSpy readyForPaintingChangedSpy(window, &Window::readyForPaintingChanged);
    QVERIFY(readyForPaintingChangedSpy.wait());

    // now shade the window
    workspace()->slotWindowShade();
    QVERIFY(window->isShade());

    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);

    // begin move
    QVERIFY(workspace()->moveResizeWindow() == nullptr);
    QCOMPARE(window->isInteractiveMove(), false);
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(window->isInteractiveMove(), true);
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);

    // wait for frame rendered
    QTest::qWait(100);

    // send some key events, not going through input redirection
    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());

    // wait for frame rendered
    QTest::qWait(100);

    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());

    // wait for frame rendered
    QTest::qWait(100);

    window->keyPressEvent(Qt::Key_Down | Qt::ALT);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos(), Qt::KeyboardModifiers());

    // wait for frame rendered
    QTest::qWait(100);

    // let's end
    window->keyPressEvent(Qt::Key_Enter);

    // wait for frame rendered
    QTest::qWait(100);
}

WAYLANDTEST_MAIN(WobblyWindowsShadeTest)
#include "wobbly_shade_test.moc"
