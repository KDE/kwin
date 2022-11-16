/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "composite.h"
#include "core/outputbackend.h"
#include "cursor.h"
#include "effectloader.h"
#include "effects.h"
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
    config->group("Outline").writeEntry(QStringLiteral("QmlPath"), QString("/does/not/exist.qml"));
    config->group("Effect-kwin4_effect_translucency").writeEntry(QStringLiteral("Dialogs"), 90);

    config->sync();
    kwinApp()->setConfig(config);

    qputenv("KWIN_EFFECTS_FORCE_ANIMATIONS", "1");
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QVERIFY(Compositor::self());
}

void TranslucencyTest::init()
{
    // load the translucency effect
    EffectsHandlerImpl *e = static_cast<EffectsHandlerImpl *>(effects);
    // find the effectsloader
    auto effectloader = e->findChild<AbstractEffectLoader *>();
    QVERIFY(effectloader);
    QSignalSpy effectLoadedSpy(effectloader, &AbstractEffectLoader::effectLoaded);

    QVERIFY(!e->isEffectLoaded(QStringLiteral("kwin4_effect_translucency")));
    QVERIFY(e->loadEffect(QStringLiteral("kwin4_effect_translucency")));
    QVERIFY(e->isEffectLoaded(QStringLiteral("kwin4_effect_translucency")));

    QCOMPARE(effectLoadedSpy.count(), 1);
    m_translucencyEffect = effectLoadedSpy.first().first().value<Effect *>();
    QVERIFY(m_translucencyEffect);
}

void TranslucencyTest::cleanup()
{
    EffectsHandlerImpl *e = static_cast<EffectsHandlerImpl *>(effects);
    if (e->isEffectLoaded(QStringLiteral("kwin4_effect_translucency"))) {
        e->unloadEffect(QStringLiteral("kwin4_effect_translucency"));
    }
    QVERIFY(!e->isEffectLoaded(QStringLiteral("kwin4_effect_translucency")));
    m_translucencyEffect = nullptr;
}

struct XcbConnectionDeleter
{
    void operator()(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

void TranslucencyTest::testMoveAfterDesktopChange()
{
    // test tries to simulate the condition of bug 366081
    QVERIFY(!m_translucencyEffect->isActive());

    QSignalSpy windowAddedSpy(effects, &EffectsHandler::windowAdded);

    // create an xcb window
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

    QVERIFY(windowAddedSpy.wait());
    QVERIFY(!m_translucencyEffect->isActive());
    // let's send the window to desktop 2
    effects->setNumberOfDesktops(2);
    QCOMPARE(effects->numberOfDesktops(), 2);
    workspace()->sendWindowToDesktop(window, 2, false);
    effects->setCurrentDesktop(2);
    QVERIFY(!m_translucencyEffect->isActive());
    KWin::Cursors::self()->mouse()->setPos(window->frameGeometry().center());
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

    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
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

    QVERIFY(windowAddedSpy.wait());
    QTRY_VERIFY(m_translucencyEffect->isActive());
    // and destroy the window again
    xcb_unmap_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);

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
