/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "cursor.h"
#include "deleted.h"
#include "screenedge.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"
#include <kwineffects.h>

#include <netwm.h>
#include <xcb/xcb_icccm.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_screenedge_client_show-0");

class ScreenEdgeClientShowTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void testScreenEdgeShowHideX11_data();
    void testScreenEdgeShowHideX11();
    void testScreenEdgeShowX11Touch_data();
    void testScreenEdgeShowX11Touch();
};

void ScreenEdgeClientShowTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::Deleted *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    // set custom config which disable touch edge
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup group = config->group("TabBox");
    group.writeEntry(QStringLiteral("TouchBorderActivate"), "9");
    group.sync();

    kwinApp()->setConfig(config);

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void ScreenEdgeClientShowTest::init()
{
    workspace()->setActiveOutput(QPoint(640, 512));
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
    QVERIFY(waylandServer()->windows().isEmpty());
}

struct XcbConnectionDeleter
{
    void operator()(xcb_connection_t *pointer)
    {
        xcb_disconnect(pointer);
    }
};

void ScreenEdgeClientShowTest::testScreenEdgeShowHideX11_data()
{
    QTest::addColumn<QRect>("windowGeometry");
    QTest::addColumn<QRect>("resizedWindowGeometry");
    QTest::addColumn<quint32>("location");
    QTest::addColumn<QPoint>("triggerPos");

    QTest::newRow("bottom/left") << QRect(50, 1004, 1180, 20) << QRect(150, 1004, 1000, 20) << 2u << QPoint(100, 1023);
    QTest::newRow("bottom/right") << QRect(1330, 1004, 1180, 20) << QRect(1410, 1004, 1000, 20) << 2u << QPoint(1400, 1023);
    QTest::newRow("top/left") << QRect(50, 0, 1180, 20) << QRect(150, 0, 1000, 20) << 0u << QPoint(100, 0);
    QTest::newRow("top/right") << QRect(1330, 0, 1180, 20) << QRect(1410, 0, 1000, 20) << 0u << QPoint(1400, 0);
    QTest::newRow("left") << QRect(0, 10, 20, 1000) << QRect(0, 70, 20, 800) << 3u << QPoint(0, 50);
    QTest::newRow("right") << QRect(2540, 10, 20, 1000) << QRect(2540, 70, 20, 800) << 1u << QPoint(2559, 60);
}

void ScreenEdgeClientShowTest::testScreenEdgeShowHideX11()
{
    // this test creates a window which borders the screen and sets the screenedge show hint
    // that should trigger a show of the window whenever the cursor is pushed against the screen edge

    // create the test window
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.get()));
    // atom for the screenedge show hide functionality
    Xcb::Atom atom(QByteArrayLiteral("_KDE_NET_WM_SCREEN_EDGE_SHOW"), false, c.get());

    xcb_window_t windowId = xcb_generate_id(c.get());
    QFETCH(QRect, windowGeometry);
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
    NETWinInfo info(c.get(), windowId, rootWindow(), NET::WMAllProperties, NET::WM2AllProperties);
    info.setWindowType(NET::Dock);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window);
    QVERIFY(!window->isDecorated());
    QCOMPARE(window->frameGeometry(), windowGeometry);
    QVERIFY(!window->hasStrut());
    QVERIFY(!window->isHiddenInternal());

    QSignalSpy effectsWindowAdded(effects, &EffectsHandler::windowAdded);
    QVERIFY(effectsWindowAdded.wait());

    // now try to hide
    QFETCH(quint32, location);
    xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atom, XCB_ATOM_CARDINAL, 32, 1, &location);
    xcb_flush(c.get());

    QSignalSpy effectsWindowHiddenSpy(effects, &EffectsHandler::windowHidden);
    QSignalSpy clientHiddenSpy(window, &X11Window::windowHidden);
    QVERIFY(clientHiddenSpy.wait());
    QVERIFY(window->isHiddenInternal());
    QCOMPARE(effectsWindowHiddenSpy.count(), 1);

    // now trigger the edge
    QSignalSpy effectsWindowShownSpy(effects, &EffectsHandler::windowShown);
    QFETCH(QPoint, triggerPos);
    Cursors::self()->mouse()->setPos(triggerPos);
    QVERIFY(!window->isHiddenInternal());
    QCOMPARE(effectsWindowShownSpy.count(), 1);

    // go into event loop to trigger xcb_flush
    QTest::qWait(1);

    // hide window again
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
    xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atom, XCB_ATOM_CARDINAL, 32, 1, &location);
    xcb_flush(c.get());
    QVERIFY(clientHiddenSpy.wait());
    QVERIFY(window->isHiddenInternal());
    QFETCH(QRect, resizedWindowGeometry);
    // resizewhile hidden
    window->moveResize(resizedWindowGeometry);
    // triggerPos shouldn't be valid anymore
    Cursors::self()->mouse()->setPos(triggerPos);
    QVERIFY(window->isHiddenInternal());

    // destroy window again
    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(windowClosedSpy.wait());
}

void ScreenEdgeClientShowTest::testScreenEdgeShowX11Touch_data()
{
    QTest::addColumn<QRect>("windowGeometry");
    QTest::addColumn<quint32>("location");
    QTest::addColumn<QPoint>("touchDownPos");
    QTest::addColumn<QPoint>("targetPos");

    QTest::newRow("bottom/left") << QRect(50, 1004, 1180, 20) << 2u << QPoint(100, 1023) << QPoint(100, 540);
    QTest::newRow("bottom/right") << QRect(1330, 1004, 1180, 20) << 2u << QPoint(1400, 1023) << QPoint(1400, 520);
    QTest::newRow("top/left") << QRect(50, 0, 1180, 20) << 0u << QPoint(100, 0) << QPoint(100, 350);
    QTest::newRow("top/right") << QRect(1330, 0, 1180, 20) << 0u << QPoint(1400, 0) << QPoint(1400, 400);
    QTest::newRow("left") << QRect(0, 10, 20, 1000) << 3u << QPoint(0, 50) << QPoint(400, 50);
    QTest::newRow("right") << QRect(2540, 10, 20, 1000) << 1u << QPoint(2559, 60) << QPoint(2200, 60);
}

void ScreenEdgeClientShowTest::testScreenEdgeShowX11Touch()
{
    // this test creates a window which borders the screen and sets the screenedge show hint
    // that should trigger a show of the window whenever the touch screen swipe gesture is triggered

    // create the test window
    std::unique_ptr<xcb_connection_t, XcbConnectionDeleter> c(xcb_connect(nullptr, nullptr));
    QVERIFY(!xcb_connection_has_error(c.get()));
    // atom for the screenedge show hide functionality
    Xcb::Atom atom(QByteArrayLiteral("_KDE_NET_WM_SCREEN_EDGE_SHOW"), false, c.get());

    xcb_window_t windowId = xcb_generate_id(c.get());
    QFETCH(QRect, windowGeometry);
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
    NETWinInfo info(c.get(), windowId, rootWindow(), NET::WMAllProperties, NET::WM2AllProperties);
    info.setWindowType(NET::Dock);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.last().first().value<X11Window *>();
    QVERIFY(window);
    QVERIFY(!window->isDecorated());
    QCOMPARE(window->frameGeometry(), windowGeometry);
    QVERIFY(!window->hasStrut());
    QVERIFY(!window->isHiddenInternal());

    QSignalSpy effectsWindowAdded(effects, &EffectsHandler::windowAdded);
    QVERIFY(effectsWindowAdded.wait());

    // now try to hide
    QFETCH(quint32, location);
    xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atom, XCB_ATOM_CARDINAL, 32, 1, &location);
    xcb_flush(c.get());

    QSignalSpy effectsWindowHiddenSpy(effects, &EffectsHandler::windowHidden);
    QSignalSpy clientHiddenSpy(window, &X11Window::windowHidden);
    QVERIFY(clientHiddenSpy.wait());
    QVERIFY(window->isHiddenInternal());
    QCOMPARE(effectsWindowHiddenSpy.count(), 1);

    // now trigger the edge
    QSignalSpy effectsWindowShownSpy(effects, &EffectsHandler::windowShown);
    quint32 timestamp = 0;
    QFETCH(QPoint, touchDownPos);
    QFETCH(QPoint, targetPos);
    Test::touchDown(0, touchDownPos, timestamp++);
    Test::touchMotion(0, targetPos, timestamp++);
    Test::touchUp(0, timestamp++);
    QVERIFY(effectsWindowShownSpy.wait());
    QVERIFY(!window->isHiddenInternal());
    QCOMPARE(effectsWindowShownSpy.count(), 1);

    // destroy window again
    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    xcb_unmap_window(c.get(), windowId);
    xcb_destroy_window(c.get(), windowId);
    xcb_flush(c.get());
    QVERIFY(windowClosedSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::ScreenEdgeClientShowTest)
#include "screenedge_client_show_test.moc"
