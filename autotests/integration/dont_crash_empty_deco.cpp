/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "composite.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "core/renderbackend.h"
#include "cursor.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"
#include <kwineffects.h>

#include <KDecoration2/Decoration>

#include <linux/input.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_dont_crash_empty_decoration-0");

class DontCrashEmptyDecorationTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void testBug361551();
};

void DontCrashEmptyDecorationTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    // this test needs to enforce OpenGL compositing to get into the crashy condition
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);

    QCOMPARE(Compositor::self()->backend()->compositingType(), KWin::OpenGLCompositing);
}

void DontCrashEmptyDecorationTest::init()
{
    workspace()->setActiveOutput(QPoint(640, 512));
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void DontCrashEmptyDecorationTest::testBug361551()
{
    // this test verifies that resizing an X11 window to an invalid size does not result in crash on unmap
    // when the DecorationRenderer gets copied to the Deleted
    // there a repaint is scheduled and the resulting texture is invalid if the window size is invalid

    // create an xcb window
    xcb_connection_t *c = xcb_connect(nullptr, nullptr);
    QVERIFY(!xcb_connection_has_error(c));

    xcb_window_t windowId = xcb_generate_id(c);
    xcb_create_window(c, XCB_COPY_FROM_PARENT, windowId, rootWindow(), 0, 0, 10, 10, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_map_window(c, windowId);
    xcb_flush(c);

    // we should get a window for it
    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    X11Window *window = windowCreatedSpy.first().first().value<X11Window *>();
    QVERIFY(window);
    QCOMPARE(window->window(), windowId);
    QVERIFY(window->isDecorated());

    // let's set a stupid geometry
    window->moveResize({0, 0, 0, 0});
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 0, 0));

    // and destroy the window again
    xcb_unmap_window(c, windowId);
    xcb_destroy_window(c, windowId);
    xcb_flush(c);
    xcb_disconnect(c);

    QSignalSpy windowClosedSpy(window, &X11Window::windowClosed);
    QVERIFY(windowClosedSpy.wait());
}

}

WAYLANDTEST_MAIN(KWin::DontCrashEmptyDecorationTest)
#include "dont_crash_empty_deco.moc"
