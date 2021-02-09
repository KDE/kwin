/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// KWin
#include "tabbox/desktopchain.h"

#include <QTest>

using namespace KWin::TabBox;

class TestDesktopChain : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void chainInit_data();
    void chainInit();
    void chainAdd_data();
    void chainAdd();
    void resize_data();
    void resize();
    void resizeAdd();
    void useChain();
};

void TestDesktopChain::chainInit_data()
{
    QTest::addColumn<uint>("size");
    QTest::addColumn<uint>("next");
    QTest::addColumn<uint>("result");

    QTest::newRow("0/1") << (uint)0 << (uint)1 << (uint)1;
    QTest::newRow("0/5") << (uint)0 << (uint)5 << (uint)1;
    QTest::newRow("1/1") << (uint)1 << (uint)1 << (uint)1;
    QTest::newRow("1/2") << (uint)1 << (uint)2 << (uint)1;
    QTest::newRow("4/1") << (uint)4 << (uint)1 << (uint)2;
    QTest::newRow("4/2") << (uint)4 << (uint)2 << (uint)3;
    QTest::newRow("4/3") << (uint)4 << (uint)3 << (uint)4;
    QTest::newRow("4/4") << (uint)4 << (uint)4 << (uint)1;
    QTest::newRow("4/5") << (uint)4 << (uint)5 << (uint)1;
    QTest::newRow("4/7") << (uint)4 << (uint)7 << (uint)1;
}

void TestDesktopChain::chainInit()
{
    QFETCH(uint, size);
    QFETCH(uint, next);
    DesktopChain chain(size);
    QTEST(chain.next(next), "result");

    DesktopChainManager manager(this);
    manager.resize(0, size);
    QTEST(manager.next(next), "result");
}

void TestDesktopChain::chainAdd_data()
{
    QTest::addColumn<uint>("size");
    QTest::addColumn<uint>("add");
    QTest::addColumn<uint>("next");
    QTest::addColumn<uint>("result");

    // invalid size, should not crash
    QTest::newRow("0/1/1/1") << (uint)0 << (uint)1 << (uint)1 << (uint)1;
    // moving first element to the front, shouldn't change the chain
    QTest::newRow("4/1/1/2") << (uint)4 << (uint)1 << (uint)1 << (uint)2;
    QTest::newRow("4/1/2/3") << (uint)4 << (uint)1 << (uint)2 << (uint)3;
    QTest::newRow("4/1/3/4") << (uint)4 << (uint)1 << (uint)3 << (uint)4;
    QTest::newRow("4/1/4/1") << (uint)4 << (uint)1 << (uint)4 << (uint)1;
    // moving an element from middle to front, should reorder
    QTest::newRow("4/3/1/1") << (uint)4 << (uint)3 << (uint)1 << (uint)2;
    QTest::newRow("4/3/2/4") << (uint)4 << (uint)3 << (uint)2 << (uint)4;
    QTest::newRow("4/3/3/1") << (uint)4 << (uint)3 << (uint)3 << (uint)1;
    QTest::newRow("4/3/4/3") << (uint)4 << (uint)3 << (uint)4 << (uint)3;
    // adding an element which does not exist - should leave the chain untouched
    QTest::newRow("4/5/1/2") << (uint)4 << (uint)5 << (uint)1 << (uint)2;
    QTest::newRow("4/5/2/3") << (uint)4 << (uint)5 << (uint)2 << (uint)3;
    QTest::newRow("4/5/3/4") << (uint)4 << (uint)5 << (uint)3 << (uint)4;
    QTest::newRow("4/5/4/1") << (uint)4 << (uint)5 << (uint)4 << (uint)1;
}

void TestDesktopChain::chainAdd()
{
    QFETCH(uint, size);
    QFETCH(uint, add);
    QFETCH(uint, next);
    DesktopChain chain(size);
    chain.add(add);
    QTEST(chain.next(next), "result");

    DesktopChainManager manager(this);
    manager.resize(0, size);
    manager.addDesktop(0, add);
    QTEST(manager.next(next), "result");
}

void TestDesktopChain::resize_data()
{
    QTest::addColumn<uint>("size");
    QTest::addColumn<uint>("add");
    QTest::addColumn<uint>("newSize");
    QTest::addColumn<uint>("next");
    QTest::addColumn<uint>("result");

    // basic test - increment by one
    QTest::newRow("1->2/1") << (uint)1 << (uint)1 << (uint)2 << (uint)1 << (uint)2;
    QTest::newRow("1->2/2") << (uint)1 << (uint)1 << (uint)2 << (uint)2 << (uint)1;
    // more complex test - increment by three, keep chain untouched
    QTest::newRow("3->6/1") << (uint)3 << (uint)1 << (uint)6 << (uint)1 << (uint)2;
    QTest::newRow("3->6/2") << (uint)3 << (uint)1 << (uint)6 << (uint)2 << (uint)3;
    QTest::newRow("3->6/3") << (uint)3 << (uint)1 << (uint)6 << (uint)3 << (uint)4;
    QTest::newRow("3->6/4") << (uint)3 << (uint)1 << (uint)6 << (uint)4 << (uint)5;
    QTest::newRow("3->6/5") << (uint)3 << (uint)1 << (uint)6 << (uint)5 << (uint)6;
    QTest::newRow("3->6/6") << (uint)3 << (uint)1 << (uint)6 << (uint)6 << (uint)1;
    // increment by three, but change it before
    QTest::newRow("3->6/3/1") << (uint)3 << (uint)3 << (uint)6 << (uint)1 << (uint)2;
    QTest::newRow("3->6/3/2") << (uint)3 << (uint)3 << (uint)6 << (uint)2 << (uint)4;
    QTest::newRow("3->6/3/3") << (uint)3 << (uint)3 << (uint)6 << (uint)3 << (uint)1;
    QTest::newRow("3->6/3/4") << (uint)3 << (uint)3 << (uint)6 << (uint)4 << (uint)5;
    QTest::newRow("3->6/3/5") << (uint)3 << (uint)3 << (uint)6 << (uint)5 << (uint)6;
    QTest::newRow("3->6/3/6") << (uint)3 << (uint)3 << (uint)6 << (uint)6 << (uint)3;

    // basic test - decrement by one
    QTest::newRow("2->1/1") << (uint)2 << (uint)1 << (uint)1 << (uint)1 << (uint)1;
    QTest::newRow("2->1/2") << (uint)2 << (uint)2 << (uint)1 << (uint)1 << (uint)1;
    // more complex test - decrement by three, keep chain untouched
    QTest::newRow("6->3/1") << (uint)6 << (uint)1 << (uint)3 << (uint)1 << (uint)2;
    QTest::newRow("6->3/2") << (uint)6 << (uint)1 << (uint)3 << (uint)2 << (uint)3;
    QTest::newRow("6->3/3") << (uint)6 << (uint)1 << (uint)3 << (uint)3 << (uint)1;
    // more complex test - decrement by three, move element to front
    QTest::newRow("6->3/6/1") << (uint)6 << (uint)6 << (uint)3 << (uint)1 << (uint)2;
    QTest::newRow("6->3/6/2") << (uint)6 << (uint)6 << (uint)3 << (uint)2 << (uint)3;
    QTest::newRow("6->3/6/3") << (uint)6 << (uint)6 << (uint)3 << (uint)3 << (uint)1;
}

void TestDesktopChain::resize()
{
    QFETCH(uint, size);
    DesktopChain chain(size);
    QFETCH(uint, add);
    chain.add(add);
    QFETCH(uint, newSize);
    chain.resize(size, newSize);
    QFETCH(uint, next);
    QTEST(chain.next(next), "result");

    DesktopChainManager manager(this);
    manager.resize(0, size);
    manager.addDesktop(0, add);
    manager.resize(size, newSize);
    QTEST(manager.next(next), "result");
}

void TestDesktopChain::resizeAdd()
{
    // test that verifies that add works after shrinking the chain
    DesktopChain chain(6);
    DesktopChainManager manager(this);
    manager.resize(0, 6);
    chain.add(4);
    manager.addDesktop(0, 4);
    chain.add(5);
    manager.addDesktop(4, 5);
    chain.add(6);
    manager.addDesktop(5, 6);
    QCOMPARE(chain.next(6), (uint)5);
    QCOMPARE(manager.next(6), (uint)5);
    QCOMPARE(chain.next(5), (uint)4);
    QCOMPARE(manager.next(5), (uint)4);
    QCOMPARE(chain.next(4), (uint)1);
    QCOMPARE(manager.next(4), (uint)1);
    chain.resize(6, 3);
    manager.resize(6, 3);
    QCOMPARE(chain.next(3), (uint)3);
    QCOMPARE(manager.next(3), (uint)3);
    QCOMPARE(chain.next(1), (uint)3);
    QCOMPARE(manager.next(1), (uint)3);
    QCOMPARE(chain.next(2), (uint)3);
    QCOMPARE(manager.next(2), (uint)3);
    // add
    chain.add(1);
    manager.addDesktop(3, 1);
    QCOMPARE(chain.next(3), (uint)3);
    QCOMPARE(manager.next(3), (uint)3);
    QCOMPARE(chain.next(1), (uint)3);
    QCOMPARE(manager.next(1), (uint)3);
    chain.add(2);
    manager.addDesktop(1, 2);
    QCOMPARE(chain.next(1), (uint)3);
    QCOMPARE(manager.next(1), (uint)3);
    QCOMPARE(chain.next(2), (uint)1);
    QCOMPARE(manager.next(2), (uint)1);
    QCOMPARE(chain.next(3), (uint)2);
    QCOMPARE(manager.next(3), (uint)2);
}

void TestDesktopChain::useChain()
{
    DesktopChainManager manager(this);
    manager.resize(0, 4);
    manager.addDesktop(0, 3);
    // creating the first chain, should keep it unchanged
    manager.useChain(QStringLiteral("test"));
    QCOMPARE(manager.next(3), (uint)1);
    QCOMPARE(manager.next(1), (uint)2);
    QCOMPARE(manager.next(2), (uint)4);
    QCOMPARE(manager.next(4), (uint)3);
    // but creating a second chain, should create an empty one
    manager.useChain(QStringLiteral("second chain"));
    QCOMPARE(manager.next(1), (uint)2);
    QCOMPARE(manager.next(2), (uint)3);
    QCOMPARE(manager.next(3), (uint)4);
    QCOMPARE(manager.next(4), (uint)1);
    // adding a desktop should only affect the currently used one
    manager.addDesktop(3, 2);
    QCOMPARE(manager.next(1), (uint)3);
    QCOMPARE(manager.next(2), (uint)1);
    QCOMPARE(manager.next(3), (uint)4);
    QCOMPARE(manager.next(4), (uint)2);
    // verify by switching back
    manager.useChain(QStringLiteral("test"));
    QCOMPARE(manager.next(3), (uint)1);
    QCOMPARE(manager.next(1), (uint)2);
    QCOMPARE(manager.next(2), (uint)4);
    QCOMPARE(manager.next(4), (uint)3);
    manager.addDesktop(3, 1);
    // use second chain again and put 4th desktop to front
    manager.useChain(QStringLiteral("second chain"));
    manager.addDesktop(3, 4);
    // just for the fun a third chain, and let's shrink it
    manager.useChain(QStringLiteral("third chain"));
    manager.resize(4, 3);
    QCOMPARE(manager.next(1), (uint)2);
    QCOMPARE(manager.next(2), (uint)3);
    // it must have affected all chains
    manager.useChain(QStringLiteral("test"));
    QCOMPARE(manager.next(1), (uint)3);
    QCOMPARE(manager.next(3), (uint)2);
    QCOMPARE(manager.next(2), (uint)1);
    manager.useChain(QStringLiteral("second chain"));
    QCOMPARE(manager.next(3), (uint)2);
    QCOMPARE(manager.next(1), (uint)3);
    QCOMPARE(manager.next(2), (uint)1);
}

QTEST_MAIN(TestDesktopChain)
#include "test_desktopchain.moc"
