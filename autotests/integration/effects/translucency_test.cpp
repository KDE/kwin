/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "compositor.h"
#include "effect/effecthandler.h"
#include "effect/effectloader.h"
#include "pointer_input.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

#include <KConfigGroup>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

using namespace KWin;
static const QString s_socketName = QStringLiteral("wayland_test_effects_translucency-0");

class TranslucencyTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testMoveAfterDesktopChange();
    void testDialogClose();

private:
    Effect *m_translucencyEffect = nullptr;
};

void TranslucencyTest::initTestCase()
{
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());
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
    config->group(QStringLiteral("Outline")).writeEntry(QStringLiteral("QmlPath"), QString("/does/not/exist.qml"));
    config->group(QStringLiteral("Effect-translucency")).writeEntry(QStringLiteral("Dialogs"), 90);

    config->sync();
    kwinApp()->setConfig(config);

    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", "1");
    kwinApp()->start();
    QVERIFY(Compositor::self());
}

void TranslucencyTest::init()
{
    // find the effectsloader
    auto effectloader = effects->findChild<AbstractEffectLoader *>();
    QVERIFY(effectloader);
    QSignalSpy effectLoadedSpy(effectloader, &AbstractEffectLoader::effectLoaded);

    QVERIFY(!effects->isEffectLoaded(QStringLiteral("translucency")));
    QVERIFY(effects->loadEffect(QStringLiteral("translucency")));
    QVERIFY(effects->isEffectLoaded(QStringLiteral("translucency")));

    QCOMPARE(effectLoadedSpy.count(), 1);
    m_translucencyEffect = effectLoadedSpy.first().first().value<Effect *>();
    QVERIFY(m_translucencyEffect);
}

void TranslucencyTest::cleanup()
{
    if (effects->isEffectLoaded(QStringLiteral("translucency"))) {
        effects->unloadEffect(QStringLiteral("translucency"));
    }
    QVERIFY(!effects->isEffectLoaded(QStringLiteral("translucency")));
    m_translucencyEffect = nullptr;
}

void TranslucencyTest::testMoveAfterDesktopChange()
{
    // test tries to simulate the condition of bug 366081
    QVERIFY(!m_translucencyEffect->isActive());

    QSignalSpy windowAddedSpy(effects, &EffectsHandler::windowAdded);

    // create an xcb window
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

    QMetaObject::invokeMethod(window, "setReadyForPainting");
    QVERIFY(window->readyForPainting());

    QCOMPARE(windowAddedSpy.count(), 1);
    QVERIFY(!m_translucencyEffect->isActive());
    // let's send the window to desktop 2
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    vds->setCount(2);
    const QList<VirtualDesktop *> desktops = vds->desktops();
    workspace()->sendWindowToDesktops(window, {desktops[1]}, false);
    vds->setCurrent(desktops[1]);
    QVERIFY(!m_translucencyEffect->isActive());
    KWin::input()->pointer()->warp(window->frameGeometry().center());
    workspace()->performWindowOperation(window, Options::MoveOp);
    QVERIFY(m_translucencyEffect->isActive());
    QTest::qWait(200);
    QVERIFY(m_translucencyEffect->isActive());
    // now end move resize
    window->endInteractiveMoveResize();
    QVERIFY(m_translucencyEffect->isActive());
    QTest::qWait(500);
    QTRY_VERIFY(!m_translucencyEffect->isActive());

    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowClosedSpy(window, &X11Window::closed);
    QVERIFY(windowClosedSpy.wait());
    xcb_destroy_window(c.get(), windowId);
    c.reset();
}

void TranslucencyTest::testDialogClose()
{
    // this test simulates the condition of BUG 342716
    // with translucency settings for window type dialog the effect never ends when the window gets destroyed
    QVERIFY(!m_translucencyEffect->isActive());
    QSignalSpy windowAddedSpy(effects, &EffectsHandler::windowAdded);

    // create an xcb window
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
    NETWinInfo winInfo(c.get(), windowId, rootWindow(), NET::Properties(), NET::Properties2());
    winInfo.setWindowType(NET::Dialog);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QVERIFY(window->isDecorated());
    QVERIFY(window->isDialog());

    QCOMPARE(windowAddedSpy.count(), 1);
    QTRY_VERIFY(m_translucencyEffect->isActive());
    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowClosedSpy(window, &X11Window::closed);

    QSignalSpy windowDeletedSpy(effects, &EffectsHandler::windowDeleted);
    QVERIFY(windowClosedSpy.wait());
    if (windowDeletedSpy.isEmpty()) {
        QVERIFY(windowDeletedSpy.wait());
    }
    QCOMPARE(windowDeletedSpy.count(), 1);
    QTRY_VERIFY(!m_translucencyEffect->isActive());
    xcb_destroy_window(c.get(), windowId);
    c.reset();
}

WAYLANDTEST_MAIN(TranslucencyTest)
#include "translucency_test.moc"
