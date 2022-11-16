/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "composite.h"
#include "core/outputbackend.h"
#include "core/renderbackend.h"
#include "cursor.h"
#include "effectloader.h"
#include "effects.h"
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
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::Effect *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

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
    QVERIFY(applicationStartedSpy.wait());
    QVERIFY(Compositor::self());

    QCOMPARE(Compositor::self()->backend()->compositingType(), KWin::OpenGLCompositing);
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
    void operator()(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

void WobblyWindowsShadeTest::testShadeMove()
{
    // this test simulates the condition from BUG 390953
    EffectsHandlerImpl *e = static_cast<EffectsHandlerImpl *>(effects);
    QVERIFY(e->loadEffect(QStringLiteral("wobblywindows")));
    QVERIFY(e->isEffectLoaded(QStringLiteral("wobblywindows")));

    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
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

    QSignalSpy windowShownSpy(window, &Window::windowShown);
    QVERIFY(windowShownSpy.wait());

    // now shade the window
    workspace()->slotWindowShade();
    QVERIFY(window->isShade());

    QSignalSpy windowStartUserMovedResizedSpy(e, &EffectsHandler::windowStartUserMovedResized);

    // begin move
    QVERIFY(workspace()->moveResizeWindow() == nullptr);
    QCOMPARE(window->isInteractiveMove(), false);
    workspace()->slotWindowMove();
    QCOMPARE(workspace()->moveResizeWindow(), window);
    QCOMPARE(window->isInteractiveMove(), true);
    QCOMPARE(windowStartUserMovedResizedSpy.count(), 1);

    // wait for frame rendered
    QTest::qWait(100);

    // send some key events, not going through input redirection
    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());

    // wait for frame rendered
    QTest::qWait(100);

    window->keyPressEvent(Qt::Key_Right);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());

    // wait for frame rendered
    QTest::qWait(100);

    window->keyPressEvent(Qt::Key_Down | Qt::ALT);
    window->updateInteractiveMoveResize(KWin::Cursors::self()->mouse()->pos());

    // wait for frame rendered
    QTest::qWait(100);

    // let's end
    window->keyPressEvent(Qt::Key_Enter);

    // wait for frame rendered
    QTest::qWait(100);
}

WAYLANDTEST_MAIN(WobblyWindowsShadeTest)
#include "wobbly_shade_test.moc"
