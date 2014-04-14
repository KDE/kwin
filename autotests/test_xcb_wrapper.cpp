/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "testutils.h"
// KWin
#include "../xcbutils.h"
// Qt
#include <QApplication>
#include <QtTest/QtTest>
// xcb
#include <xcb/xcb.h>

using namespace KWin;
using namespace KWin::Xcb;

class TestXcbWrapper : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();
    void defaultCtor();
    void normalCtor();
    void copyCtorEmpty();
    void copyCtorBeforeRetrieve();
    void copyCtorAfterRetrieve();
    void assignementEmpty();
    void assignmentBeforeRetrieve();
    void assignmentAfterRetrieve();
    void discard();
    void testQueryTree();
    void testCurrentInput();
    void testTransientFor();
private:
    void testEmpty(WindowGeometry &geometry);
    void testGeometry(WindowGeometry &geometry, const QRect &rect);
    xcb_window_t m_testWindow;
};

void TestXcbWrapper::init()
{
    m_testWindow = XCB_WINDOW_NONE;
}

void TestXcbWrapper::cleanup()
{
    if (m_testWindow != XCB_WINDOW_NONE) {
        xcb_destroy_window(connection(), m_testWindow);
    }
}

void TestXcbWrapper::testEmpty(WindowGeometry &geometry)
{
    QCOMPARE(geometry.window(), noneWindow());
    QVERIFY(!geometry.data());
    QCOMPARE(geometry.isNull(), true);
    QCOMPARE(geometry.rect(), QRect());
    QVERIFY(!geometry);
}

void TestXcbWrapper::testGeometry(WindowGeometry &geometry, const QRect &rect)
{
    QCOMPARE(geometry.window(), m_testWindow);
    // now lets retrieve some data
    QCOMPARE(geometry.rect(), rect);
    QVERIFY(geometry.isRetrieved());
    QCOMPARE(geometry.isNull(), false);
    QVERIFY(geometry);
    QVERIFY(geometry.data());
    QCOMPARE(geometry.data()->x, int16_t(rect.x()));
    QCOMPARE(geometry.data()->y, int16_t(rect.y()));
    QCOMPARE(geometry.data()->width, uint16_t(rect.width()));
    QCOMPARE(geometry.data()->height, uint16_t(rect.height()));
}

void TestXcbWrapper::defaultCtor()
{
    WindowGeometry geometry;
    testEmpty(geometry);
    QVERIFY(!geometry.isRetrieved());
}

void TestXcbWrapper::normalCtor()
{
    m_testWindow = createWindow();
    QVERIFY(m_testWindow != noneWindow());

    WindowGeometry geometry(m_testWindow);
    QVERIFY(!geometry.isRetrieved());
    testGeometry(geometry, QRect(0, 0, 10, 10));
}

void TestXcbWrapper::copyCtorEmpty()
{
    WindowGeometry geometry;
    WindowGeometry other(geometry);
    testEmpty(geometry);
    QVERIFY(geometry.isRetrieved());
    testEmpty(other);
    QVERIFY(!other.isRetrieved());
}

void TestXcbWrapper::copyCtorBeforeRetrieve()
{
    m_testWindow = createWindow();
    QVERIFY(m_testWindow != noneWindow());
    WindowGeometry geometry(m_testWindow);
    QVERIFY(!geometry.isRetrieved());
    WindowGeometry other(geometry);
    testEmpty(geometry);
    QVERIFY(geometry.isRetrieved());

    QVERIFY(!other.isRetrieved());
    testGeometry(other, QRect(0, 0, 10, 10));
}

void TestXcbWrapper::copyCtorAfterRetrieve()
{
    m_testWindow = createWindow();
    QVERIFY(m_testWindow != noneWindow());
    WindowGeometry geometry(m_testWindow);
    QVERIFY(geometry);
    QVERIFY(geometry.isRetrieved());
    QCOMPARE(geometry.rect(), QRect(0, 0, 10, 10));
    WindowGeometry other(geometry);
    testEmpty(geometry);
    QVERIFY(geometry.isRetrieved());

    QVERIFY(other.isRetrieved());
    testGeometry(other, QRect(0, 0, 10, 10));
}

void TestXcbWrapper::assignementEmpty()
{
    WindowGeometry geometry;
    WindowGeometry other;
    testEmpty(geometry);
    testEmpty(other);

    other = geometry;
    QVERIFY(geometry.isRetrieved());
    testEmpty(geometry);
    testEmpty(other);
    QVERIFY(!other.isRetrieved());
}

void TestXcbWrapper::assignmentBeforeRetrieve()
{
    m_testWindow = createWindow();
    QVERIFY(m_testWindow != noneWindow());
    WindowGeometry geometry(m_testWindow);
    WindowGeometry other = geometry;
    QVERIFY(geometry.isRetrieved());
    testEmpty(geometry);

    QVERIFY(!other.isRetrieved());
    testGeometry(other, QRect(0, 0, 10, 10));

    other = WindowGeometry(m_testWindow);
    QVERIFY(!other.isRetrieved());
    QCOMPARE(other.window(), m_testWindow);
    other = WindowGeometry();
    testEmpty(geometry);
}

void TestXcbWrapper::assignmentAfterRetrieve()
{
    m_testWindow = createWindow();
    QVERIFY(m_testWindow != noneWindow());
    WindowGeometry geometry(m_testWindow);
    QVERIFY(geometry);
    QVERIFY(geometry.isRetrieved());
    WindowGeometry other = geometry;
    testEmpty(geometry);

    QVERIFY(other.isRetrieved());
    testGeometry(other, QRect(0, 0, 10, 10));

    other = WindowGeometry();
    testEmpty(other);
}

void TestXcbWrapper::discard()
{
    // discard of reply cannot be tested properly as we cannot check whether the reply has been discarded
    // therefore it's more or less just a test to ensure that it doesn't crash and the code paths
    // are taken.
    WindowGeometry *geometry = new WindowGeometry();
    delete geometry;

    m_testWindow = createWindow();
    geometry = new WindowGeometry(m_testWindow);
    delete geometry;

    geometry = new WindowGeometry(m_testWindow);
    QVERIFY(geometry->data());
    delete geometry;
}

void TestXcbWrapper::testQueryTree()
{
    m_testWindow = createWindow();
    QVERIFY(m_testWindow != noneWindow());
    Tree tree(m_testWindow);
    // should have root as parent
    QCOMPARE(tree.parent(), static_cast<xcb_window_t>(QX11Info::appRootWindow()));
    // shouldn't have any children
    QCOMPARE(tree->children_len, uint16_t(0));
    QVERIFY(!tree.children());

    // query for root
    Tree root(QX11Info::appRootWindow());
    // shouldn't have a parent
    QCOMPARE(root.parent(), xcb_window_t(XCB_WINDOW_NONE));
    QVERIFY(root->children_len > 0);
    xcb_window_t *children = root.children();
    bool found = false;
    for (int i = 0; i < xcb_query_tree_children_length(root.data()); ++i) {
        if (children[i] == tree.window()) {
            found = true;
            break;
        }
    }
    QVERIFY(found);

    // query for not existing window
    Tree doesntExist(XCB_WINDOW_NONE);
    QCOMPARE(doesntExist.parent(), xcb_window_t(XCB_WINDOW_NONE));
    QVERIFY(doesntExist.isNull());
    QVERIFY(doesntExist.isRetrieved());
}

void TestXcbWrapper::testCurrentInput()
{
    m_testWindow = createWindow();
    xcb_connection_t *c = QX11Info::connection();
    xcb_map_window(c, m_testWindow);
    QX11Info::setAppTime(QX11Info::getTimestamp());

    // let's set the input focus
    xcb_set_input_focus(c, XCB_INPUT_FOCUS_PARENT, m_testWindow, QX11Info::appTime());
    xcb_flush(c);

    CurrentInput input;
    QCOMPARE(input.window(), m_testWindow);

    // creating a copy should make the input object have no window any more
    CurrentInput input2(input);
    QCOMPARE(input2.window(), m_testWindow);
    QCOMPARE(input.window(), xcb_window_t(XCB_WINDOW_NONE));
}

void TestXcbWrapper::testTransientFor()
{
    m_testWindow = createWindow();
    TransientFor transient(m_testWindow);
    QCOMPARE(transient.window(), m_testWindow);
    // our m_testWindow doesn't have a transient for hint
    xcb_window_t compareWindow = XCB_WINDOW_NONE;
    QVERIFY(!transient.getTransientFor(&compareWindow));
    QCOMPARE(compareWindow, xcb_window_t(XCB_WINDOW_NONE));

    // Create a Window with a transient for hint
    Window transientWindow(createWindow());
    transientWindow.changeProperty(XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW, 32, 1, &m_testWindow);

    // let's get another transient object
    TransientFor realTransient(transientWindow);
    QVERIFY(realTransient.getTransientFor(&compareWindow));
    QCOMPARE(compareWindow, m_testWindow);

    // test for a not existing window
    TransientFor doesntExist(XCB_WINDOW_NONE);
    QVERIFY(!doesntExist.getTransientFor(&compareWindow));
}

KWIN_TEST_MAIN(TestXcbWrapper)
#include "test_xcb_wrapper.moc"
