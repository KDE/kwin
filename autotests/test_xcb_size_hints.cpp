/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "testutils.h"
// KWin
#include "utils/xcbutils.h"
// Qt
#include <QApplication>
#include <QtTest>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <private/qtx11extras_p.h>
#else
#include <QX11Info>
#endif
#include <netwm.h>
// xcb
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

using namespace KWin;

class TestXcbSizeHints : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testSizeHints_data();
    void testSizeHints();
    void testSizeHintsEmpty();
    void testSizeHintsNotSet();
    void geometryHintsBeforeInit();
    void geometryHintsBeforeRead();

private:
    Xcb::Window m_testWindow;
};

void TestXcbSizeHints::initTestCase()
{
    qApp->setProperty("x11RootWindow", QVariant::fromValue<quint32>(QX11Info::appRootWindow()));
    qApp->setProperty("x11Connection", QVariant::fromValue<void *>(QX11Info::connection()));
}

void TestXcbSizeHints::init()
{
    const uint32_t values[] = {true};
    m_testWindow.create(QRect(0, 0, 10, 10), XCB_WINDOW_CLASS_INPUT_ONLY, XCB_CW_OVERRIDE_REDIRECT, values);
    QVERIFY(m_testWindow.isValid());
}

void TestXcbSizeHints::cleanup()
{
    m_testWindow.reset();
}

void TestXcbSizeHints::testSizeHints_data()
{
    // set
    QTest::addColumn<QPoint>("userPos");
    QTest::addColumn<QSize>("userSize");
    QTest::addColumn<QSize>("minSize");
    QTest::addColumn<QSize>("maxSize");
    QTest::addColumn<QSize>("resizeInc");
    QTest::addColumn<QSize>("minAspect");
    QTest::addColumn<QSize>("maxAspect");
    QTest::addColumn<QSize>("baseSize");
    QTest::addColumn<qint32>("gravity");
    // read for SizeHints
    QTest::addColumn<qint32>("expectedFlags");
    QTest::addColumn<qint32>("expectedPad0");
    QTest::addColumn<qint32>("expectedPad1");
    QTest::addColumn<qint32>("expectedPad2");
    QTest::addColumn<qint32>("expectedPad3");
    QTest::addColumn<qint32>("expectedMinWidth");
    QTest::addColumn<qint32>("expectedMinHeight");
    QTest::addColumn<qint32>("expectedMaxWidth");
    QTest::addColumn<qint32>("expectedMaxHeight");
    QTest::addColumn<qint32>("expectedWidthInc");
    QTest::addColumn<qint32>("expectedHeightInc");
    QTest::addColumn<qint32>("expectedMinAspectNum");
    QTest::addColumn<qint32>("expectedMinAspectDen");
    QTest::addColumn<qint32>("expectedMaxAspectNum");
    QTest::addColumn<qint32>("expectedMaxAspectDen");
    QTest::addColumn<qint32>("expectedBaseWidth");
    QTest::addColumn<qint32>("expectedBaseHeight");
    // read for GeometryHints
    QTest::addColumn<QSize>("expectedMinSize");
    QTest::addColumn<QSize>("expectedMaxSize");
    QTest::addColumn<QSize>("expectedResizeIncrements");
    QTest::addColumn<QSize>("expectedMinAspect");
    QTest::addColumn<QSize>("expectedMaxAspect");
    QTest::addColumn<QSize>("expectedBaseSize");
    QTest::addColumn<qint32>("expectedGravity");

    QTest::newRow("userPos") << QPoint(1, 2) << QSize() << QSize() << QSize() << QSize() << QSize() << QSize() << QSize() << 0
                             << 1 << 1 << 2 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0
                             << QSize(0, 0) << QSize(INT_MAX, INT_MAX) << QSize(1, 1) << QSize(1, INT_MAX) << QSize(INT_MAX, 1) << QSize(0, 0) << qint32(XCB_GRAVITY_NORTH_WEST);
    QTest::newRow("userSize") << QPoint() << QSize(1, 2) << QSize() << QSize() << QSize() << QSize() << QSize() << QSize() << 0
                              << 2 << 0 << 0 << 1 << 2 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0
                              << QSize(0, 0) << QSize(INT_MAX, INT_MAX) << QSize(1, 1) << QSize(1, INT_MAX) << QSize(INT_MAX, 1) << QSize(0, 0) << qint32(XCB_GRAVITY_NORTH_WEST);
    QTest::newRow("minSize") << QPoint() << QSize() << QSize(1, 2) << QSize() << QSize() << QSize() << QSize() << QSize() << 0
                             << 16 << 0 << 0 << 0 << 0 << 1 << 2 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0
                             << QSize(1, 2) << QSize(INT_MAX, INT_MAX) << QSize(1, 1) << QSize(1, INT_MAX) << QSize(INT_MAX, 1) << QSize(0, 0) << qint32(XCB_GRAVITY_NORTH_WEST);
    QTest::newRow("maxSize") << QPoint() << QSize() << QSize() << QSize(1, 2) << QSize() << QSize() << QSize() << QSize() << 0
                             << 32 << 0 << 0 << 0 << 0 << 0 << 0 << 1 << 2 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0
                             << QSize(0, 0) << QSize(1, 2) << QSize(1, 1) << QSize(1, INT_MAX) << QSize(INT_MAX, 1) << QSize(0, 0) << qint32(XCB_GRAVITY_NORTH_WEST);
    QTest::newRow("maxSize0") << QPoint() << QSize() << QSize() << QSize(0, 0) << QSize() << QSize() << QSize() << QSize() << 0
                              << 32 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0
                              << QSize(0, 0) << QSize(1, 1) << QSize(1, 1) << QSize(1, INT_MAX) << QSize(INT_MAX, 1) << QSize(0, 0) << qint32(XCB_GRAVITY_NORTH_WEST);
    QTest::newRow("min/maxSize") << QPoint() << QSize() << QSize(1, 2) << QSize(3, 4) << QSize() << QSize() << QSize() << QSize() << 0
                                 << 48 << 0 << 0 << 0 << 0 << 1 << 2 << 3 << 4 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0
                                 << QSize(1, 2) << QSize(3, 4) << QSize(1, 1) << QSize(1, INT_MAX) << QSize(INT_MAX, 1) << QSize(0, 0) << qint32(XCB_GRAVITY_NORTH_WEST);
    QTest::newRow("resizeInc") << QPoint() << QSize() << QSize() << QSize() << QSize(1, 2) << QSize() << QSize() << QSize() << 0
                               << 64 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 1 << 2 << 0 << 0 << 0 << 0 << 0 << 0
                               << QSize(0, 0) << QSize(INT_MAX, INT_MAX) << QSize(1, 2) << QSize(1, INT_MAX) << QSize(INT_MAX, 1) << QSize(0, 0) << qint32(XCB_GRAVITY_NORTH_WEST);
    QTest::newRow("resizeInc0") << QPoint() << QSize() << QSize() << QSize() << QSize(0, 0) << QSize() << QSize() << QSize() << 0
                                << 64 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0
                                << QSize(0, 0) << QSize(INT_MAX, INT_MAX) << QSize(1, 1) << QSize(1, INT_MAX) << QSize(INT_MAX, 1) << QSize(0, 0) << qint32(XCB_GRAVITY_NORTH_WEST);
    QTest::newRow("aspect") << QPoint() << QSize() << QSize() << QSize() << QSize() << QSize(1, 2) << QSize(3, 4) << QSize() << 0
                            << 128 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 1 << 2 << 3 << 4 << 0 << 0
                            << QSize(0, 0) << QSize(INT_MAX, INT_MAX) << QSize(1, 1) << QSize(1, 2) << QSize(3, 4) << QSize(0, 0) << qint32(XCB_GRAVITY_NORTH_WEST);
    QTest::newRow("aspectDivision0") << QPoint() << QSize() << QSize() << QSize() << QSize() << QSize(1, 0) << QSize(3, 0) << QSize() << 0
                                     << 128 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 1 << 0 << 3 << 0 << 0 << 0
                                     << QSize(0, 0) << QSize(INT_MAX, INT_MAX) << QSize(1, 1) << QSize(1, 1) << QSize(3, 1) << QSize(0, 0) << qint32(XCB_GRAVITY_NORTH_WEST);
    QTest::newRow("baseSize") << QPoint() << QSize() << QSize() << QSize() << QSize() << QSize() << QSize() << QSize(1, 2) << 0
                              << 256 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 1 << 2
                              << QSize(1, 2) << QSize(INT_MAX, INT_MAX) << QSize(1, 1) << QSize(1, INT_MAX) << QSize(INT_MAX, 1) << QSize(1, 2) << qint32(XCB_GRAVITY_NORTH_WEST);
    QTest::newRow("gravity") << QPoint() << QSize() << QSize() << QSize() << QSize() << QSize() << QSize() << QSize() << qint32(XCB_GRAVITY_STATIC)
                             << 512 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0 << 0
                             << QSize(0, 0) << QSize(INT_MAX, INT_MAX) << QSize(1, 1) << QSize(1, INT_MAX) << QSize(INT_MAX, 1) << QSize(0, 0) << qint32(XCB_GRAVITY_STATIC);
    QTest::newRow("all") << QPoint(1, 2) << QSize(3, 4) << QSize(5, 6) << QSize(7, 8) << QSize(9, 10) << QSize(11, 12) << QSize(13, 14) << QSize(15, 16) << 1
                         << 1011 << 1 << 2 << 3 << 4 << 5 << 6 << 7 << 8 << 9 << 10 << 11 << 12 << 13 << 14 << 15 << 16
                         << QSize(5, 6) << QSize(7, 8) << QSize(9, 10) << QSize(11, 12) << QSize(13, 14) << QSize(15, 16) << qint32(XCB_GRAVITY_NORTH_WEST);
}

void TestXcbSizeHints::testSizeHints()
{
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    QFETCH(QPoint, userPos);
    if (!userPos.isNull()) {
        xcb_icccm_size_hints_set_position(&hints, 1, userPos.x(), userPos.y());
    }
    QFETCH(QSize, userSize);
    if (userSize.isValid()) {
        xcb_icccm_size_hints_set_size(&hints, 1, userSize.width(), userSize.height());
    }
    QFETCH(QSize, minSize);
    if (minSize.isValid()) {
        xcb_icccm_size_hints_set_min_size(&hints, minSize.width(), minSize.height());
    }
    QFETCH(QSize, maxSize);
    if (maxSize.isValid()) {
        xcb_icccm_size_hints_set_max_size(&hints, maxSize.width(), maxSize.height());
    }
    QFETCH(QSize, resizeInc);
    if (resizeInc.isValid()) {
        xcb_icccm_size_hints_set_resize_inc(&hints, resizeInc.width(), resizeInc.height());
    }
    QFETCH(QSize, minAspect);
    QFETCH(QSize, maxAspect);
    if (minAspect.isValid() && maxAspect.isValid()) {
        xcb_icccm_size_hints_set_aspect(&hints, minAspect.width(), minAspect.height(), maxAspect.width(), maxAspect.height());
    }
    QFETCH(QSize, baseSize);
    if (baseSize.isValid()) {
        xcb_icccm_size_hints_set_base_size(&hints, baseSize.width(), baseSize.height());
    }
    QFETCH(qint32, gravity);
    if (gravity != 0) {
        xcb_icccm_size_hints_set_win_gravity(&hints, (xcb_gravity_t)gravity);
    }
    xcb_icccm_set_wm_normal_hints(QX11Info::connection(), m_testWindow, &hints);
    xcb_flush(QX11Info::connection());

    Xcb::GeometryHints geoHints;
    geoHints.init(m_testWindow);
    geoHints.read();
    QCOMPARE(geoHints.hasAspect(), minAspect.isValid() && maxAspect.isValid());
    QCOMPARE(geoHints.hasBaseSize(), baseSize.isValid());
    QCOMPARE(geoHints.hasMaxSize(), maxSize.isValid());
    QCOMPARE(geoHints.hasMinSize(), minSize.isValid());
    QCOMPARE(geoHints.hasPosition(), !userPos.isNull());
    QCOMPARE(geoHints.hasResizeIncrements(), resizeInc.isValid());
    QCOMPARE(geoHints.hasSize(), userSize.isValid());
    QCOMPARE(geoHints.hasWindowGravity(), gravity != 0);
    QTEST(geoHints.baseSize().toSize(), "expectedBaseSize");
    QTEST(geoHints.maxAspect(), "expectedMaxAspect");
    QTEST(geoHints.maxSize().toSize(), "expectedMaxSize");
    QTEST(geoHints.minAspect(), "expectedMinAspect");
    QTEST(geoHints.minSize().toSize(), "expectedMinSize");
    QTEST(geoHints.resizeIncrements().toSize(), "expectedResizeIncrements");
    QTEST(qint32(geoHints.windowGravity()), "expectedGravity");

    auto sizeHints = geoHints.m_sizeHints;
    QVERIFY(sizeHints);
    QTEST(sizeHints->flags, "expectedFlags");
    QTEST(sizeHints->pad[0], "expectedPad0");
    QTEST(sizeHints->pad[1], "expectedPad1");
    QTEST(sizeHints->pad[2], "expectedPad2");
    QTEST(sizeHints->pad[3], "expectedPad3");
    QTEST(sizeHints->minWidth, "expectedMinWidth");
    QTEST(sizeHints->minHeight, "expectedMinHeight");
    QTEST(sizeHints->maxWidth, "expectedMaxWidth");
    QTEST(sizeHints->maxHeight, "expectedMaxHeight");
    QTEST(sizeHints->widthInc, "expectedWidthInc");
    QTEST(sizeHints->heightInc, "expectedHeightInc");
    QTEST(sizeHints->minAspect[0], "expectedMinAspectNum");
    QTEST(sizeHints->minAspect[1], "expectedMinAspectDen");
    QTEST(sizeHints->maxAspect[0], "expectedMaxAspectNum");
    QTEST(sizeHints->maxAspect[1], "expectedMaxAspectDen");
    QTEST(sizeHints->baseWidth, "expectedBaseWidth");
    QTEST(sizeHints->baseHeight, "expectedBaseHeight");
    QCOMPARE(sizeHints->winGravity, gravity);

    // copy
    Xcb::GeometryHints::NormalHints::SizeHints sizeHints2 = *sizeHints;
    QTEST(sizeHints2.flags, "expectedFlags");
    QTEST(sizeHints2.pad[0], "expectedPad0");
    QTEST(sizeHints2.pad[1], "expectedPad1");
    QTEST(sizeHints2.pad[2], "expectedPad2");
    QTEST(sizeHints2.pad[3], "expectedPad3");
    QTEST(sizeHints2.minWidth, "expectedMinWidth");
    QTEST(sizeHints2.minHeight, "expectedMinHeight");
    QTEST(sizeHints2.maxWidth, "expectedMaxWidth");
    QTEST(sizeHints2.maxHeight, "expectedMaxHeight");
    QTEST(sizeHints2.widthInc, "expectedWidthInc");
    QTEST(sizeHints2.heightInc, "expectedHeightInc");
    QTEST(sizeHints2.minAspect[0], "expectedMinAspectNum");
    QTEST(sizeHints2.minAspect[1], "expectedMinAspectDen");
    QTEST(sizeHints2.maxAspect[0], "expectedMaxAspectNum");
    QTEST(sizeHints2.maxAspect[1], "expectedMaxAspectDen");
    QTEST(sizeHints2.baseWidth, "expectedBaseWidth");
    QTEST(sizeHints2.baseHeight, "expectedBaseHeight");
    QCOMPARE(sizeHints2.winGravity, gravity);
}

void TestXcbSizeHints::testSizeHintsEmpty()
{
    xcb_size_hints_t xcbHints;
    memset(&xcbHints, 0, sizeof(xcbHints));
    xcb_icccm_set_wm_normal_hints(QX11Info::connection(), m_testWindow, &xcbHints);
    xcb_flush(QX11Info::connection());

    Xcb::GeometryHints hints;
    hints.init(m_testWindow);
    hints.read();
    QVERIFY(!hints.hasAspect());
    QVERIFY(!hints.hasBaseSize());
    QVERIFY(!hints.hasMaxSize());
    QVERIFY(!hints.hasMinSize());
    QVERIFY(!hints.hasPosition());
    QVERIFY(!hints.hasResizeIncrements());
    QVERIFY(!hints.hasSize());
    QVERIFY(!hints.hasWindowGravity());

    QCOMPARE(hints.baseSize(), QSize(0, 0));
    QCOMPARE(hints.maxAspect(), QSize(INT_MAX, 1));
    QCOMPARE(hints.maxSize(), QSize(INT_MAX, INT_MAX));
    QCOMPARE(hints.minAspect(), QSize(1, INT_MAX));
    QCOMPARE(hints.minSize(), QSize(0, 0));
    QCOMPARE(hints.resizeIncrements(), QSize(1, 1));
    QCOMPARE(hints.windowGravity(), XCB_GRAVITY_NORTH_WEST);

    auto sizeHints = hints.m_sizeHints;
    QVERIFY(sizeHints);
    QCOMPARE(sizeHints->flags, 0);
    QCOMPARE(sizeHints->pad[0], 0);
    QCOMPARE(sizeHints->pad[1], 0);
    QCOMPARE(sizeHints->pad[2], 0);
    QCOMPARE(sizeHints->pad[3], 0);
    QCOMPARE(sizeHints->minWidth, 0);
    QCOMPARE(sizeHints->minHeight, 0);
    QCOMPARE(sizeHints->maxWidth, 0);
    QCOMPARE(sizeHints->maxHeight, 0);
    QCOMPARE(sizeHints->widthInc, 0);
    QCOMPARE(sizeHints->heightInc, 0);
    QCOMPARE(sizeHints->minAspect[0], 0);
    QCOMPARE(sizeHints->minAspect[1], 0);
    QCOMPARE(sizeHints->maxAspect[0], 0);
    QCOMPARE(sizeHints->maxAspect[1], 0);
    QCOMPARE(sizeHints->baseWidth, 0);
    QCOMPARE(sizeHints->baseHeight, 0);
    QCOMPARE(sizeHints->winGravity, 0);
}

void TestXcbSizeHints::testSizeHintsNotSet()
{
    Xcb::GeometryHints hints;
    hints.init(m_testWindow);
    hints.read();
    QVERIFY(!hints.m_sizeHints);
    QVERIFY(!hints.hasAspect());
    QVERIFY(!hints.hasBaseSize());
    QVERIFY(!hints.hasMaxSize());
    QVERIFY(!hints.hasMinSize());
    QVERIFY(!hints.hasPosition());
    QVERIFY(!hints.hasResizeIncrements());
    QVERIFY(!hints.hasSize());
    QVERIFY(!hints.hasWindowGravity());

    QCOMPARE(hints.baseSize(), QSize(0, 0));
    QCOMPARE(hints.maxAspect(), QSize(INT_MAX, 1));
    QCOMPARE(hints.maxSize(), QSize(INT_MAX, INT_MAX));
    QCOMPARE(hints.minAspect(), QSize(1, INT_MAX));
    QCOMPARE(hints.minSize(), QSize(0, 0));
    QCOMPARE(hints.resizeIncrements(), QSize(1, 1));
    QCOMPARE(hints.windowGravity(), XCB_GRAVITY_NORTH_WEST);
}

void TestXcbSizeHints::geometryHintsBeforeInit()
{
    Xcb::GeometryHints hints;
    QVERIFY(!hints.hasAspect());
    QVERIFY(!hints.hasBaseSize());
    QVERIFY(!hints.hasMaxSize());
    QVERIFY(!hints.hasMinSize());
    QVERIFY(!hints.hasPosition());
    QVERIFY(!hints.hasResizeIncrements());
    QVERIFY(!hints.hasSize());
    QVERIFY(!hints.hasWindowGravity());

    QCOMPARE(hints.baseSize(), QSize(0, 0));
    QCOMPARE(hints.maxAspect(), QSize(INT_MAX, 1));
    QCOMPARE(hints.maxSize(), QSize(INT_MAX, INT_MAX));
    QCOMPARE(hints.minAspect(), QSize(1, INT_MAX));
    QCOMPARE(hints.minSize(), QSize(0, 0));
    QCOMPARE(hints.resizeIncrements(), QSize(1, 1));
    QCOMPARE(hints.windowGravity(), XCB_GRAVITY_NORTH_WEST);
}

void TestXcbSizeHints::geometryHintsBeforeRead()
{
    xcb_size_hints_t xcbHints;
    memset(&xcbHints, 0, sizeof(xcbHints));
    xcb_icccm_size_hints_set_position(&xcbHints, 1, 1, 2);
    xcb_icccm_set_wm_normal_hints(QX11Info::connection(), m_testWindow, &xcbHints);
    xcb_flush(QX11Info::connection());

    Xcb::GeometryHints hints;
    hints.init(m_testWindow);
    QVERIFY(!hints.hasAspect());
    QVERIFY(!hints.hasBaseSize());
    QVERIFY(!hints.hasMaxSize());
    QVERIFY(!hints.hasMinSize());
    QVERIFY(!hints.hasPosition());
    QVERIFY(!hints.hasResizeIncrements());
    QVERIFY(!hints.hasSize());
    QVERIFY(!hints.hasWindowGravity());

    QCOMPARE(hints.baseSize(), QSize(0, 0));
    QCOMPARE(hints.maxAspect(), QSize(INT_MAX, 1));
    QCOMPARE(hints.maxSize(), QSize(INT_MAX, INT_MAX));
    QCOMPARE(hints.minAspect(), QSize(1, INT_MAX));
    QCOMPARE(hints.minSize(), QSize(0, 0));
    QCOMPARE(hints.resizeIncrements(), QSize(1, 1));
    QCOMPARE(hints.windowGravity(), XCB_GRAVITY_NORTH_WEST);
}

Q_CONSTRUCTOR_FUNCTION(forceXcb)
QTEST_MAIN(TestXcbSizeHints)
#include "test_xcb_size_hints.moc"
