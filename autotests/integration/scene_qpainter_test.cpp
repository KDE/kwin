/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "composite.h"
#include "effectloader.h"
#include "x11client.h"
#include "cursor.h"
#include "effects.h"
#include "platform.h"
#include "wayland_server.h"
#include "effect_builtins.h"
#include "workspace.h"

#include <KConfigGroup>

#include <KWayland/Client/seat.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/pointer.h>
#include <KWaylandServer/buffer_interface.h>
#include <KWaylandServer/surface_interface.h>

#include <QPainter>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

using namespace KWin;
static const QString s_socketName = QStringLiteral("wayland_test_kwin_scene_qpainter-0");

class SceneQPainterTest : public QObject
{
Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void testStartFrame();
    void testCursorMoving();
    void testWindow();
    void testWindowScaled();
    void testCompositorRestart();
    void testX11Window();
};

void SceneQPainterTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void SceneQPainterTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient*>();
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

    if (!QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("icons/DMZ-White/index.theme")).isEmpty()) {
        qputenv("XCURSOR_THEME", QByteArrayLiteral("DMZ-White"));
    } else {
        // might be vanilla-dmz (e.g. Arch, FreeBSD)
        qputenv("XCURSOR_THEME", QByteArrayLiteral("Vanilla-DMZ"));
    }
    qputenv("XCURSOR_SIZE", QByteArrayLiteral("24"));
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("Q"));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QVERIFY(Compositor::self());
}

void SceneQPainterTest::testStartFrame()
{
    // this test verifies that the initial rendering is correct
    Compositor::self()->addRepaintFull();
    auto scene = Compositor::self()->scene();
    QVERIFY(scene);
    QCOMPARE(kwinApp()->platform()->selectedCompositor(), QPainterCompositing);
    QSignalSpy frameRenderedSpy(scene, &Scene::frameRendered);
    QVERIFY(frameRenderedSpy.isValid());
    QVERIFY(frameRenderedSpy.wait());
    // now let's render a reference image for comparison
    QImage referenceImage(QSize(1280, 1024), QImage::Format_RGB32);
    referenceImage.fill(Qt::black);
    QPainter p(&referenceImage);

    auto cursor = KWin::Cursors::self()->mouse();
    const QImage cursorImage = cursor->image();
    QVERIFY(!cursorImage.isNull());
    p.drawImage(cursor->pos() - cursor->hotspot(), cursorImage);
    QCOMPARE(referenceImage, *scene->qpainterRenderBuffer());
}

void SceneQPainterTest::testCursorMoving()
{
    // this test verifies that rendering is correct also after moving the cursor a few times
    auto scene = Compositor::self()->scene();
    QVERIFY(scene);
    QSignalSpy frameRenderedSpy(scene, &Scene::frameRendered);
    QVERIFY(frameRenderedSpy.isValid());
    KWin::Cursors::self()->mouse()->setPos(0, 0);
    QVERIFY(frameRenderedSpy.wait());
    KWin::Cursors::self()->mouse()->setPos(10, 0);
    QVERIFY(frameRenderedSpy.wait());
    KWin::Cursors::self()->mouse()->setPos(10, 12);
    QVERIFY(frameRenderedSpy.wait());
    KWin::Cursors::self()->mouse()->setPos(12, 14);
    QVERIFY(frameRenderedSpy.wait());
    KWin::Cursors::self()->mouse()->setPos(50, 60);
    QVERIFY(frameRenderedSpy.wait());
    KWin::Cursors::self()->mouse()->setPos(45, 45);
    QVERIFY(frameRenderedSpy.wait());
    // now let's render a reference image for comparison
    QImage referenceImage(QSize(1280, 1024), QImage::Format_RGB32);
    referenceImage.fill(Qt::black);
    QPainter p(&referenceImage);

    auto cursor = Cursors::self()->currentCursor();
    const QImage cursorImage = cursor->image();
    QVERIFY(!cursorImage.isNull());
    p.drawImage(QPoint(45, 45) - cursor->hotspot(), cursorImage);
    QCOMPARE(referenceImage, *scene->qpainterRenderBuffer());
}

void SceneQPainterTest::testWindow()
{
    KWin::Cursors::self()->mouse()->setPos(45, 45);
    // this test verifies that a window is rendered correctly
    using namespace KWayland::Client;
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandPointer());
    QScopedPointer<Surface> s(Test::createSurface());
    QScopedPointer<XdgShellSurface> ss(Test::createXdgShellStableSurface(s.data()));
    QScopedPointer<Pointer> p(Test::waylandSeat()->createPointer());

    auto scene = KWin::Compositor::self()->scene();
    QVERIFY(scene);
    QSignalSpy frameRenderedSpy(scene, &Scene::frameRendered);
    QVERIFY(frameRenderedSpy.isValid());

    // now let's map the window
    QVERIFY(Test::renderAndWaitForShown(s.data(), QSize(200, 300), Qt::blue));
    // which should trigger a frame
    if (frameRenderedSpy.isEmpty()) {
        QVERIFY(frameRenderedSpy.wait());
    }
    // we didn't set a cursor image on the surface yet, so it should be just black + window and previous cursor
    QImage referenceImage(QSize(1280, 1024), QImage::Format_RGB32);
    referenceImage.fill(Qt::black);
    QPainter painter(&referenceImage);
    painter.fillRect(0, 0, 200, 300, Qt::blue);

    // now let's set a cursor image
    QScopedPointer<Surface> cs(Test::createSurface());
    QVERIFY(!cs.isNull());
    Test::render(cs.data(), QSize(10, 10), Qt::red);
    p->setCursor(cs.data(), QPoint(5, 5));
    QVERIFY(frameRenderedSpy.wait());
    painter.fillRect(KWin::Cursors::self()->mouse()->pos().x() - 5, KWin::Cursors::self()->mouse()->pos().y() - 5, 10, 10, Qt::red);
    QCOMPARE(referenceImage, *scene->qpainterRenderBuffer());
    // let's move the cursor again
    KWin::Cursors::self()->mouse()->setPos(10, 10);
    QVERIFY(frameRenderedSpy.wait());
    painter.fillRect(0, 0, 200, 300, Qt::blue);
    painter.fillRect(5, 5, 10, 10, Qt::red);
    QCOMPARE(referenceImage, *scene->qpainterRenderBuffer());
}

void SceneQPainterTest::testWindowScaled()
{
    KWin::Cursors::self()->mouse()->setPos(10, 10);
    // this test verifies that a window is rendered correctly
    using namespace KWayland::Client;
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandPointer());
    QScopedPointer<Surface> s(Test::createSurface());
    QScopedPointer<XdgShellSurface> ss(Test::createXdgShellStableSurface(s.data()));
    QScopedPointer<Pointer> p(Test::waylandSeat()->createPointer());
    QSignalSpy pointerEnteredSpy(p.data(), &Pointer::entered);
    QVERIFY(pointerEnteredSpy.isValid());

    auto scene = KWin::Compositor::self()->scene();
    QVERIFY(scene);
    QSignalSpy frameRenderedSpy(scene, &Scene::frameRendered);
    QVERIFY(frameRenderedSpy.isValid());

    // now let's set a cursor image
    QScopedPointer<Surface> cs(Test::createSurface());
    QVERIFY(!cs.isNull());
    Test::render(cs.data(), QSize(10, 10), Qt::red);

    // now let's map the window
    s->setScale(2);

    //draw a blue square@400x600 with red rectangle@200x200 in the middle
    const QSize size(400,600);
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::blue);
    QPainter surfacePainter(&img);
    surfacePainter.fillRect(200,300,200,200, Qt::red);

    //add buffer
    Test::render(s.data(), img);
    QVERIFY(pointerEnteredSpy.wait());
    p->setCursor(cs.data(), QPoint(5, 5));

    // which should trigger a frame
    QVERIFY(frameRenderedSpy.wait());
    QImage referenceImage(QSize(1280, 1024), QImage::Format_RGB32);
    referenceImage.fill(Qt::black);
    QPainter painter(&referenceImage);
    painter.fillRect(0, 0, 200, 300, Qt::blue);
    painter.fillRect(100, 150, 100, 100, Qt::red);
    painter.fillRect(5, 5, 10, 10, Qt::red); //cursor

    QCOMPARE(referenceImage, *scene->qpainterRenderBuffer());
}

void SceneQPainterTest::testCompositorRestart()
{
    // this test verifies that the compositor/SceneQPainter survive a restart of the compositor and still render correctly
    KWin::Cursors::self()->mouse()->setPos(400, 400);

    // first create a window
    using namespace KWayland::Client;
    QVERIFY(Test::setupWaylandConnection());
    QScopedPointer<Surface> s(Test::createSurface());
    QScopedPointer<XdgShellSurface> ss(Test::createXdgShellStableSurface(s.data()));
    QVERIFY(Test::renderAndWaitForShown(s.data(), QSize(200, 300), Qt::blue));

    // now let's try to reinitialize the compositing scene
    auto oldScene = KWin::Compositor::self()->scene();
    QVERIFY(oldScene);
    QSignalSpy sceneCreatedSpy(KWin::Compositor::self(), &KWin::Compositor::sceneCreated);
    QVERIFY(sceneCreatedSpy.isValid());
    KWin::Compositor::self()->reinitialize();
    if (sceneCreatedSpy.isEmpty()) {
        QVERIFY(sceneCreatedSpy.wait());
    }
    QCOMPARE(sceneCreatedSpy.count(), 1);
    auto scene = KWin::Compositor::self()->scene();
    QVERIFY(scene);

    // this should directly trigger a frame
    KWin::Compositor::self()->addRepaintFull();
    QSignalSpy frameRenderedSpy(scene, &Scene::frameRendered);
    QVERIFY(frameRenderedSpy.isValid());
    QVERIFY(frameRenderedSpy.wait());

    // render reference image
    QImage referenceImage(QSize(1280, 1024), QImage::Format_RGB32);
    referenceImage.fill(Qt::black);
    QPainter painter(&referenceImage);
    painter.fillRect(0, 0, 200, 300, Qt::blue);

    auto cursor = Cursors::self()->mouse();
    const QImage cursorImage = cursor->image();
    QVERIFY(!cursorImage.isNull());
    painter.drawImage(QPoint(400, 400) - cursor->hotspot(), cursorImage);
    QCOMPARE(referenceImage, *scene->qpainterRenderBuffer());
}

struct XcbConnectionDeleter
{
    static inline void cleanup(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

void SceneQPainterTest::testX11Window()
{
    // this test verifies the condition of BUG: 382748

    // create X11 window
    QSignalSpy windowAddedSpy(effects, &EffectsHandler::windowAdded);
    QVERIFY(windowAddedSpy.isValid());

    // create an xcb window
    QScopedPointer<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.data()));
    const QRect windowGeometry(0, 0, 100, 200);
    xcb_window_t w = xcb_generate_id(c.data());
    uint32_t value = kwinApp()->x11DefaultScreen()->white_pixel;
    xcb_create_window(c.data(), XCB_COPY_FROM_PARENT, w, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL, &value);
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
    QCOMPARE(client->clientSize(), QSize(100, 200));
    if (!client->surface()) {
        // wait for surface
        QSignalSpy surfaceChangedSpy(client, &Toplevel::surfaceChanged);
        QVERIFY(surfaceChangedSpy.isValid());
        QVERIFY(surfaceChangedSpy.wait());
    }
    QVERIFY(client->surface());
    QTRY_VERIFY(client->surface()->buffer());
    QTRY_COMPARE(client->surface()->buffer()->data().size(), client->size());
    QImage compareImage(client->clientSize(), QImage::Format_RGB32);
    compareImage.fill(Qt::white);
    QCOMPARE(client->surface()->buffer()->data().copy(QRect(client->clientPos(), client->clientSize())), compareImage);

    // enough time for rendering the window
    QTest::qWait(100);

    auto scene = KWin::Compositor::self()->scene();
    QVERIFY(scene);

    // this should directly trigger a frame
    KWin::Compositor::self()->addRepaintFull();
    QSignalSpy frameRenderedSpy(scene, &Scene::frameRendered);
    QVERIFY(frameRenderedSpy.isValid());
    QVERIFY(frameRenderedSpy.wait());

    const QPoint startPos = client->pos() + client->clientPos();
    auto image = scene->qpainterRenderBuffer();
    QCOMPARE(image->copy(QRect(startPos, client->clientSize())), compareImage);

    // and destroy the window again
    xcb_unmap_window(c.data(), w);
    xcb_flush(c.data());

    QSignalSpy windowClosedSpy(client, &X11Client::windowClosed);
    QVERIFY(windowClosedSpy.isValid());
    QVERIFY(windowClosedSpy.wait());
    xcb_destroy_window(c.data(), w);
    c.reset();
}

WAYLANDTEST_MAIN(SceneQPainterTest)
#include "scene_qpainter_test.moc"
