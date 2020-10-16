/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "x11client.h"
#include "composite.h"
#include "cursor.h"
#include "effects.h"
#include "effectloader.h"
#include "platform.h"
#include "wayland_server.h"
#include "workspace.h"
#include "effect_builtins.h"

#include <KConfigGroup>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/slide.h>

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
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<KWin::Effect*>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    // disable all effects - we don't want to have it interact with the rendering
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    ScriptedEffectLoader loader;
    const auto builtinNames = BuiltInEffects::availableEffectNames() << loader.listOfKnownEffects();
    for (QString name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }

    config->sync();
    kwinApp()->setConfig(config);

    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));
    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", "1");
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QVERIFY(Compositor::self());

    auto scene = KWin::Compositor::self()->scene();
    QVERIFY(scene);
    QCOMPARE(scene->compositingType(), KWin::OpenGL2Compositing);
}

void WobblyWindowsShadeTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Decoration));
}

void WobblyWindowsShadeTest::cleanup()
{
    Test::destroyWaylandConnection();

    auto effectsImpl = static_cast<EffectsHandlerImpl *>(effects);
    effectsImpl->unloadAllEffects();
    QVERIFY(effectsImpl->loadedEffects().isEmpty());
}

struct XcbConnectionDeleter
{
    static inline void cleanup(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

void WobblyWindowsShadeTest::testShadeMove()
{
    // this test simulates the condition from BUG 390953
    EffectsHandlerImpl *e = static_cast<EffectsHandlerImpl*>(effects);
    QVERIFY(e->loadEffect(BuiltInEffects::nameForEffect(BuiltInEffect::WobblyWindows)));
    QVERIFY(e->isEffectLoaded(BuiltInEffects::nameForEffect(BuiltInEffect::WobblyWindows)));


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
    QVERIFY(client->isDecorated());
    QVERIFY(client->isShadeable());
    QVERIFY(!client->isShade());
    QVERIFY(client->isActive());

    QSignalSpy windowShownSpy(client, &AbstractClient::windowShown);
    QVERIFY(windowShownSpy.isValid());
    QVERIFY(windowShownSpy.wait());

    // now shade the window
    workspace()->slotWindowShade();
    QVERIFY(client->isShade());

    QSignalSpy windowStartUserMovedResizedSpy(e, &EffectsHandler::windowStartUserMovedResized);
    QVERIFY(windowStartUserMovedResizedSpy.isValid());

    // begin move
    QVERIFY(workspace()->moveResizeClient() == nullptr);
    QCOMPARE(client->isMove(), false);
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeClient(), client);
    QCOMPARE(client->isMove(), true);
    QCOMPARE(windowStartUserMovedResizedSpy.count(), 1);

    // wait for frame rendered
    QTest::qWait(100);

    // send some key events, not going through input redirection
    client->keyPressEvent(Qt::Key_Right);
    client->updateMoveResize(KWin::Cursors::self()->mouse()->pos());

    // wait for frame rendered
    QTest::qWait(100);

    client->keyPressEvent(Qt::Key_Right);
    client->updateMoveResize(KWin::Cursors::self()->mouse()->pos());

    // wait for frame rendered
    QTest::qWait(100);

    client->keyPressEvent(Qt::Key_Down | Qt::ALT);
    client->updateMoveResize(KWin::Cursors::self()->mouse()->pos());

    // wait for frame rendered
    QTest::qWait(100);

    // let's end
    client->keyPressEvent(Qt::Key_Enter);

    // wait for frame rendered
    QTest::qWait(100);
}

WAYLANDTEST_MAIN(WobblyWindowsShadeTest)
#include "wobbly_shade_test.moc"
