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
#include <netwm.h>
// xcb
#include <xcb/xcb.h>

using namespace KWin;
using namespace KWin::Xcb;

class TestXcbWrapper : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
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
    void testPropertyByteArray();
    void testPropertyBool();
    void testAtom();
    void testMotifEmpty();
    void testMotif_data();
    void testMotif();
private:
    void testEmpty(WindowGeometry &geometry);
    void testGeometry(WindowGeometry &geometry, const QRect &rect);
    Window m_testWindow;
};

void TestXcbWrapper::initTestCase()
{
    qApp->setProperty("x11RootWindow", QVariant::fromValue<quint32>(QX11Info::appRootWindow()));
    qApp->setProperty("x11Connection", QVariant::fromValue<void*>(QX11Info::connection()));
}

void TestXcbWrapper::init()
{
    const uint32_t values[] = { true };
    m_testWindow.create(QRect(0, 0, 10, 10), XCB_WINDOW_CLASS_INPUT_ONLY, XCB_CW_OVERRIDE_REDIRECT, values);
    QVERIFY(m_testWindow.isValid());
}

void TestXcbWrapper::cleanup()
{
    m_testWindow.reset();
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
    QCOMPARE(geometry.window(), (xcb_window_t)m_testWindow);
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
    // test assignment to self
    geometry = geometry;
    other = other;
    testEmpty(geometry);
    testEmpty(other);
}

void TestXcbWrapper::assignmentBeforeRetrieve()
{
    WindowGeometry geometry(m_testWindow);
    WindowGeometry other = geometry;
    QVERIFY(geometry.isRetrieved());
    testEmpty(geometry);

    QVERIFY(!other.isRetrieved());
    testGeometry(other, QRect(0, 0, 10, 10));

    other = WindowGeometry(m_testWindow);
    QVERIFY(!other.isRetrieved());
    QCOMPARE(other.window(), (xcb_window_t)m_testWindow);
    other = WindowGeometry();
    testEmpty(geometry);
    // test assignment to self
    geometry = geometry;
    other = other;
    testEmpty(geometry);
}

void TestXcbWrapper::assignmentAfterRetrieve()
{
    WindowGeometry geometry(m_testWindow);
    QVERIFY(geometry);
    QVERIFY(geometry.isRetrieved());
    WindowGeometry other = geometry;
    testEmpty(geometry);

    QVERIFY(other.isRetrieved());
    testGeometry(other, QRect(0, 0, 10, 10));

    // test assignment to self
    geometry = geometry;
    other = other;
    testEmpty(geometry);
    testGeometry(other, QRect(0, 0, 10, 10));

    // set to empty again
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

    geometry = new WindowGeometry(m_testWindow);
    delete geometry;

    geometry = new WindowGeometry(m_testWindow);
    QVERIFY(geometry->data());
    delete geometry;
}

void TestXcbWrapper::testQueryTree()
{
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
    xcb_connection_t *c = QX11Info::connection();
    m_testWindow.map();
    QX11Info::setAppTime(QX11Info::getTimestamp());

    // let's set the input focus
    m_testWindow.focus(XCB_INPUT_FOCUS_PARENT, QX11Info::appTime());
    xcb_flush(c);

    CurrentInput input;
    QCOMPARE(input.window(), (xcb_window_t)m_testWindow);

    // creating a copy should make the input object have no window any more
    CurrentInput input2(input);
    QCOMPARE(input2.window(), (xcb_window_t)m_testWindow);
    QCOMPARE(input.window(), xcb_window_t(XCB_WINDOW_NONE));
}

void TestXcbWrapper::testTransientFor()
{
    TransientFor transient(m_testWindow);
    QCOMPARE(transient.window(), (xcb_window_t)m_testWindow);
    // our m_testWindow doesn't have a transient for hint
    xcb_window_t compareWindow = XCB_WINDOW_NONE;
    QVERIFY(!transient.getTransientFor(&compareWindow));
    QCOMPARE(compareWindow, xcb_window_t(XCB_WINDOW_NONE));
    bool ok = true;
    QCOMPARE(transient.value<xcb_window_t>(32, XCB_ATOM_WINDOW, XCB_WINDOW_NONE, &ok), xcb_window_t(XCB_WINDOW_NONE));
    QVERIFY(!ok);
    ok = true;
    QCOMPARE(transient.value<xcb_window_t>(XCB_WINDOW_NONE, &ok), xcb_window_t(XCB_WINDOW_NONE));
    QVERIFY(!ok);

    // Create a Window with a transient for hint
    Window transientWindow(createWindow());
    xcb_window_t testWindowId = m_testWindow;
    transientWindow.changeProperty(XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW, 32, 1, &testWindowId);

    // let's get another transient object
    TransientFor realTransient(transientWindow);
    QVERIFY(realTransient.getTransientFor(&compareWindow));
    QCOMPARE(compareWindow, (xcb_window_t)m_testWindow);
    ok = false;
    QCOMPARE(realTransient.value<xcb_window_t>(32, XCB_ATOM_WINDOW, XCB_WINDOW_NONE, &ok), (xcb_window_t)m_testWindow);
    QVERIFY(ok);
    ok = false;
    QCOMPARE(realTransient.value<xcb_window_t>(XCB_WINDOW_NONE, &ok), (xcb_window_t)m_testWindow);
    QVERIFY(ok);
    ok = false;
    QCOMPARE(realTransient.value<xcb_window_t>(), (xcb_window_t)m_testWindow);
    QCOMPARE(realTransient.value<xcb_window_t*>(nullptr, &ok)[0], (xcb_window_t)m_testWindow);
    QVERIFY(ok);
    QCOMPARE(realTransient.value<xcb_window_t*>()[0], (xcb_window_t)m_testWindow);

    // test for a not existing window
    TransientFor doesntExist(XCB_WINDOW_NONE);
    QVERIFY(!doesntExist.getTransientFor(&compareWindow));
}

void TestXcbWrapper::testPropertyByteArray()
{
    Window testWindow(createWindow());
    Property prop(false, testWindow, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 100000);
    QCOMPARE(prop.toByteArray(), QByteArray());
    bool ok = true;
    QCOMPARE(prop.toByteArray(&ok), QByteArray());
    QVERIFY(!ok);
    ok = true;
    QVERIFY(!prop.value<const char*>());
    QCOMPARE(prop.value<const char*>("bar", &ok), "bar");
    QVERIFY(!ok);
    QCOMPARE(QByteArray(StringProperty(testWindow, XCB_ATOM_WM_NAME)), QByteArray());

    testWindow.changeProperty(XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 3, "foo");
    prop = Property(false, testWindow, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 100000);
    QCOMPARE(prop.toByteArray(), QByteArrayLiteral("foo"));
    QCOMPARE(prop.toByteArray(&ok), QByteArrayLiteral("foo"));
    QVERIFY(ok);
    QCOMPARE(prop.value<const char*>(nullptr, &ok), "foo");
    QVERIFY(ok);
    QCOMPARE(QByteArray(StringProperty(testWindow, XCB_ATOM_WM_NAME)), QByteArrayLiteral("foo"));

    // verify incorrect format and type
    QCOMPARE(prop.toByteArray(32), QByteArray());
    QCOMPARE(prop.toByteArray(8, XCB_ATOM_CARDINAL), QByteArray());

    // verify empty property
    testWindow.changeProperty(XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 0, nullptr);
    prop = Property(false, testWindow, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 100000);
    QCOMPARE(prop.toByteArray(), QByteArray());
    QCOMPARE(prop.toByteArray(&ok), QByteArray());
    //valid bytearray
    QVERIFY(ok);
    //The bytearray should be empty
    QVERIFY(prop.toByteArray().isEmpty());
    //The bytearray should be not null
    QVERIFY(!prop.toByteArray().isNull());
    QVERIFY(!prop.value<const char*>());
    QCOMPARE(QByteArray(StringProperty(testWindow, XCB_ATOM_WM_NAME)), QByteArray());

    // verify non existing property
    Xcb::Atom invalid(QByteArrayLiteral("INVALID_ATOM"));
    prop = Property(false, testWindow, invalid, XCB_ATOM_STRING, 0, 100000);
    QCOMPARE(prop.toByteArray(), QByteArray());
    QCOMPARE(prop.toByteArray(&ok), QByteArray());
    //invalid bytearray
    QVERIFY(!ok);
    //The bytearray should be empty
    QVERIFY(prop.toByteArray().isEmpty());
    //The bytearray should be not null
    QVERIFY(prop.toByteArray().isNull());
    QVERIFY(!prop.value<const char*>());
    QCOMPARE(QByteArray(StringProperty(testWindow, XCB_ATOM_WM_NAME)), QByteArray());
}

void TestXcbWrapper::testPropertyBool()
{
    Window testWindow(createWindow());
    Atom blockCompositing(QByteArrayLiteral("_KDE_NET_WM_BLOCK_COMPOSITING"));
    QVERIFY(blockCompositing != XCB_ATOM_NONE);
    NETWinInfo info(QX11Info::connection(), testWindow, QX11Info::appRootWindow(), NET::Properties(), NET::WM2BlockCompositing);

    Property prop(false, testWindow, blockCompositing, XCB_ATOM_CARDINAL, 0, 100000);
    bool ok = true;
    QVERIFY(!prop.toBool());
    QVERIFY(!prop.toBool(&ok));
    QVERIFY(!ok);

    info.setBlockingCompositing(true);
    xcb_flush(QX11Info::connection());
    prop = Property(false, testWindow, blockCompositing, XCB_ATOM_CARDINAL, 0, 100000);
    QVERIFY(prop.toBool());
    QVERIFY(prop.toBool(&ok));
    QVERIFY(ok);

    // incorrect type and format
    QVERIFY(!prop.toBool(8));
    QVERIFY(!prop.toBool(32, blockCompositing));
    QVERIFY(!prop.toBool(32, blockCompositing, &ok));
    QVERIFY(!ok);

    // incorrect value:
    uint32_t d[] = {1, 0};
    testWindow.changeProperty(blockCompositing, XCB_ATOM_CARDINAL, 32, 2, d);
    prop = Property(false, testWindow, blockCompositing, XCB_ATOM_CARDINAL, 0, 100000);
    QVERIFY(!prop.toBool());
    ok = true;
    QVERIFY(!prop.toBool(&ok));
    QVERIFY(!ok);
}

void TestXcbWrapper::testAtom()
{
    Atom atom(QByteArrayLiteral("WM_CLIENT_MACHINE"));
    QCOMPARE(atom.name(), QByteArrayLiteral("WM_CLIENT_MACHINE"));
    QVERIFY(atom == XCB_ATOM_WM_CLIENT_MACHINE);
    QVERIFY(atom.isValid());

    // test the const paths
    const Atom &atom2(atom);
    QVERIFY(atom2.isValid());
    QVERIFY(atom2 == XCB_ATOM_WM_CLIENT_MACHINE);
    QCOMPARE(atom2.name(), QByteArrayLiteral("WM_CLIENT_MACHINE"));

    //destroy before retrieved
    Atom atom3(QByteArrayLiteral("WM_CLIENT_MACHINE"));
    QCOMPARE(atom3.name(), QByteArrayLiteral("WM_CLIENT_MACHINE"));
}

void TestXcbWrapper::testMotifEmpty()
{
    Atom atom(QByteArrayLiteral("_MOTIF_WM_HINTS"));
    MotifHints hints(atom);
    // pre init
    QCOMPARE(hints.hasDecoration(), false);
    QCOMPARE(hints.noBorder(), false);
    QCOMPARE(hints.resize(), true);
    QCOMPARE(hints.move(), true);
    QCOMPARE(hints.minimize(), true);
    QCOMPARE(hints.maximize(), true);
    QCOMPARE(hints.close(), true);
    // post init, pre read
    hints.init(m_testWindow);
    QCOMPARE(hints.hasDecoration(), false);
    QCOMPARE(hints.noBorder(), false);
    QCOMPARE(hints.resize(), true);
    QCOMPARE(hints.move(), true);
    QCOMPARE(hints.minimize(), true);
    QCOMPARE(hints.maximize(), true);
    QCOMPARE(hints.close(), true);
    // post read
    hints.read();
    QCOMPARE(hints.hasDecoration(), false);
    QCOMPARE(hints.noBorder(), false);
    QCOMPARE(hints.resize(), true);
    QCOMPARE(hints.move(), true);
    QCOMPARE(hints.minimize(), true);
    QCOMPARE(hints.maximize(), true);
    QCOMPARE(hints.close(), true);
}

void TestXcbWrapper::testMotif_data()
{
    QTest::addColumn<quint32>("flags");
    QTest::addColumn<quint32>("functions");
    QTest::addColumn<quint32>("decorations");

    QTest::addColumn<bool>("expectedHasDecoration");
    QTest::addColumn<bool>("expectedNoBorder");
    QTest::addColumn<bool>("expectedResize");
    QTest::addColumn<bool>("expectedMove");
    QTest::addColumn<bool>("expectedMinimize");
    QTest::addColumn<bool>("expectedMaximize");
    QTest::addColumn<bool>("expectedClose");

    QTest::newRow("none")     << 0u <<  0u << 0u << false << false << true  << true  << true  << true  << true;
    QTest::newRow("noborder") << 2u <<  5u << 0u << true  << true  << true  << true  << true  << true  << true;
    QTest::newRow("border")   << 2u <<  5u << 1u << true  << false << true  << true  << true  << true  << true;
    QTest::newRow("resize")   << 1u <<  2u << 1u << false << false << true  << false << false << false << false;
    QTest::newRow("move")     << 1u <<  4u << 1u << false << false << false << true  << false << false << false;
    QTest::newRow("minimize") << 1u <<  8u << 1u << false << false << false << false << true  << false << false;
    QTest::newRow("maximize") << 1u << 16u << 1u << false << false << false << false << false << true  << false;
    QTest::newRow("close")    << 1u << 32u << 1u << false << false << false << false << false << false << true;

    QTest::newRow("resize/all")   << 1u <<  3u << 1u << false << false << false << true  << true  << true  << true;
    QTest::newRow("move/all")     << 1u <<  5u << 1u << false << false << true  << false << true  << true  << true;
    QTest::newRow("minimize/all") << 1u <<  9u << 1u << false << false << true  << true  << false << true  << true;
    QTest::newRow("maximize/all") << 1u << 17u << 1u << false << false << true  << true  << true  << false << true;
    QTest::newRow("close/all")    << 1u << 33u << 1u << false << false << true  << true  << true  << true  << false;

    QTest::newRow("all") << 1u << 62u << 1u << false << false << true << true << true << true << true;
    QTest::newRow("all/all") << 1u << 63u << 1u << false << false << false << false << false << false << false;
    QTest::newRow("all/all/deco") << 3u << 63u << 1u << true << false << false << false << false << false << false;
}

void TestXcbWrapper::testMotif()
{
    Atom atom(QByteArrayLiteral("_MOTIF_WM_HINTS"));
    QFETCH(quint32, flags);
    QFETCH(quint32, functions);
    QFETCH(quint32, decorations);
    quint32 data[] = {
        flags,
        functions,
        decorations,
        0,
        0
    };
    xcb_change_property(QX11Info::connection(), XCB_PROP_MODE_REPLACE, m_testWindow, atom, atom, 32, 5, data);
    xcb_flush(QX11Info::connection());
    MotifHints hints(atom);
    hints.init(m_testWindow);
    hints.read();
    QTEST(hints.hasDecoration(), "expectedHasDecoration");
    QTEST(hints.noBorder(), "expectedNoBorder");
    QTEST(hints.resize(), "expectedResize");
    QTEST(hints.move(), "expectedMove");
    QTEST(hints.minimize(), "expectedMinimize");
    QTEST(hints.maximize(), "expectedMaximize");
    QTEST(hints.close(), "expectedClose");
}

Q_CONSTRUCTOR_FUNCTION(forceXcb)
QTEST_MAIN(TestXcbWrapper)
#include "test_xcb_wrapper.moc"
