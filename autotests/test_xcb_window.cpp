/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "testutils.h"
// KWin
#include "../xcbutils.h"
// Qt
#include <QApplication>
#include <QtTest>
#include <QX11Info>
// xcb
#include <xcb/xcb.h>

using namespace KWin;

class TestXcbWindow : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void defaultCtor();
    void ctor();
    void classCtor();
    void create();
    void mapUnmap();
    void geometry();
    void destroy();
    void destroyNotManaged();
};

void TestXcbWindow::initTestCase()
{
    qApp->setProperty("x11RootWindow", QVariant::fromValue<quint32>(QX11Info::appRootWindow()));
    qApp->setProperty("x11Connection", QVariant::fromValue<void*>(QX11Info::connection()));
}

void TestXcbWindow::defaultCtor()
{
    Xcb::Window window;
    QCOMPARE(window.isValid(), false);
    xcb_window_t wId = window;
    QCOMPARE(wId, noneWindow());

    xcb_window_t nativeWindow = createWindow();
    Xcb::Window window2(nativeWindow);
    QCOMPARE(window2.isValid(), true);
    wId = window2;
    QCOMPARE(wId, nativeWindow);
}

void TestXcbWindow::ctor()
{
    const QRect geometry(0, 0, 10, 10);
    const uint32_t values[] = {true};
    Xcb::Window window(geometry, XCB_CW_OVERRIDE_REDIRECT, values);
    QCOMPARE(window.isValid(), true);
    QVERIFY(window != XCB_WINDOW_NONE);
    Xcb::WindowGeometry windowGeometry(window);
    QCOMPARE(windowGeometry.isNull(), false);
    QCOMPARE(windowGeometry.rect(), geometry);
}

void TestXcbWindow::classCtor()
{
    const QRect geometry(0, 0, 10, 10);
    const uint32_t values[] = {true};
    Xcb::Window window(geometry, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_CW_OVERRIDE_REDIRECT, values);
    QCOMPARE(window.isValid(), true);
    QVERIFY(window != XCB_WINDOW_NONE);
    Xcb::WindowGeometry windowGeometry(window);
    QCOMPARE(windowGeometry.isNull(), false);
    QCOMPARE(windowGeometry.rect(), geometry);

    Xcb::WindowAttributes attribs(window);
    QCOMPARE(attribs.isNull(), false);
    QVERIFY(attribs->_class == XCB_WINDOW_CLASS_INPUT_ONLY);
}

void TestXcbWindow::create()
{
    Xcb::Window window;
    QCOMPARE(window.isValid(), false);
    xcb_window_t wId = window;
    QCOMPARE(wId, noneWindow());

    const QRect geometry(0, 0, 10, 10);
    const uint32_t values[] = {true};
    window.create(geometry, XCB_CW_OVERRIDE_REDIRECT, values);
    QCOMPARE(window.isValid(), true);
    QVERIFY(window != XCB_WINDOW_NONE);
    // and reset again
    window.reset();
    QCOMPARE(window.isValid(), false);
    QVERIFY(window == XCB_WINDOW_NONE);
}

void TestXcbWindow::mapUnmap()
{
    const QRect geometry(0, 0, 10, 10);
    const uint32_t values[] = {true};
    Xcb::Window window(geometry, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_CW_OVERRIDE_REDIRECT, values);
    Xcb::WindowAttributes attribs(window);
    QCOMPARE(attribs.isNull(), false);
    QVERIFY(attribs->map_state == XCB_MAP_STATE_UNMAPPED);

    window.map();
    Xcb::WindowAttributes attribs2(window);
    QCOMPARE(attribs2.isNull(), false);
    QVERIFY(attribs2->map_state != XCB_MAP_STATE_UNMAPPED);

    window.unmap();
    Xcb::WindowAttributes attribs3(window);
    QCOMPARE(attribs3.isNull(), false);
    QVERIFY(attribs3->map_state == XCB_MAP_STATE_UNMAPPED);

    // map, unmap shouldn't fail for an invalid window, it's just ignored
    window.reset();
    window.map();
    window.unmap();
}

void TestXcbWindow::geometry()
{
    const QRect geometry(0, 0, 10, 10);
    const uint32_t values[] = {true};
    Xcb::Window window(geometry, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_CW_OVERRIDE_REDIRECT, values);
    Xcb::WindowGeometry windowGeometry(window);
    QCOMPARE(windowGeometry.isNull(), false);
    QCOMPARE(windowGeometry.rect(), geometry);

    const QRect geometry2(10, 20, 100, 200);
    window.setGeometry(geometry2);
    Xcb::WindowGeometry windowGeometry2(window);
    QCOMPARE(windowGeometry2.isNull(), false);
    QCOMPARE(windowGeometry2.rect(), geometry2);

    // setting a geometry on an invalid window should be ignored
    window.reset();
    window.setGeometry(geometry2);
    Xcb::WindowGeometry windowGeometry3(window);
    QCOMPARE(windowGeometry3.isNull(), true);
}

void TestXcbWindow::destroy()
{
    const QRect geometry(0, 0, 10, 10);
    const uint32_t values[] = {true};
    Xcb::Window window(geometry, XCB_CW_OVERRIDE_REDIRECT, values);
    QCOMPARE(window.isValid(), true);
    xcb_window_t wId = window;

    window.create(geometry, XCB_CW_OVERRIDE_REDIRECT, values);
    // wId should now be invalid
    xcb_generic_error_t *error = nullptr;
    ScopedCPointer<xcb_get_window_attributes_reply_t> attribs(xcb_get_window_attributes_reply(
        connection(),
        xcb_get_window_attributes(connection(), wId),
        &error));
    QVERIFY(attribs.isNull());
    QCOMPARE(error->error_code, uint8_t(3));
    QCOMPARE(error->resource_id, wId);
    free(error);

    // test the same for the dtor
    {
        Xcb::Window scopedWindow(geometry, XCB_CW_OVERRIDE_REDIRECT, values);
        QVERIFY(scopedWindow.isValid());
        wId = scopedWindow;
    }
    error = nullptr;
    ScopedCPointer<xcb_get_window_attributes_reply_t> attribs2(xcb_get_window_attributes_reply(
        connection(),
        xcb_get_window_attributes(connection(), wId),
        &error));
    QVERIFY(attribs2.isNull());
    QCOMPARE(error->error_code, uint8_t(3));
    QCOMPARE(error->resource_id, wId);
    free(error);
}

void TestXcbWindow::destroyNotManaged()
{
    Xcb::Window window;
    // just destroy the non-existing window
    window.reset();

    // now let's add a window
    window.reset(createWindow(), false);
    xcb_window_t w = window;
    window.reset();
    Xcb::WindowAttributes attribs(w);
    QVERIFY(attribs);
}

Q_CONSTRUCTOR_FUNCTION(forceXcb)
QTEST_MAIN(TestXcbWindow)
#include "test_xcb_window.moc"
